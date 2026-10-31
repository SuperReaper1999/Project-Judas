#include "FluidHydrostatics.h"
#include "FluidSolidSupport.h"
#include "FluidWetFootprint.h"
#include "GravityField.h"
#include "Narrowphase.h"

#include <algorithm>
#include <optional>
#include <cmath>
#include <limits>
#include <stdexcept>
#include <glm/gtc/quaternion.hpp>

namespace {
bool Finite(const glm::vec3& v) { return std::isfinite(v.x) && std::isfinite(v.y) && std::isfinite(v.z); }
void RequireSize(const glm::vec3& h) {
    if (!Finite(h) || h.x <= 0 || h.y <= 0 || h.z <= 0) throw std::invalid_argument("fluid volume requires finite positive dimensions");
}
void AppendBox(FluidVolumeQuadrature& out, const glm::vec3& center, const glm::vec3& h, unsigned n) {
    const glm::vec3 small = h / static_cast<float>(n);
    const double v = 8.0 * h.x * h.y * h.z;
    const float weight = static_cast<float>(v / (static_cast<double>(n) * n * n));
    for (unsigned z = 0; z < n; ++z) for (unsigned y = 0; y < n; ++y) for (unsigned x = 0; x < n; ++x) {
        const glm::vec3 p = center - h + glm::vec3(2*x+1, 2*y+1, 2*z+1) * small;
        out.samples.push_back({p, small, 0, weight});
    }
    out.solidBoxes.push_back({center,h});
    out.totalVolume += static_cast<float>(v);
    out.boundRadius = std::max(out.boundRadius, glm::length(glm::abs(center) + h));
}
struct Box { glm::vec3 lo, hi; };
std::vector<Box> Subtract(const Box& a, const Box& b) {
    const glm::vec3 lo = glm::max(a.lo,b.lo), hi = glm::min(a.hi,b.hi);
    if (lo.x >= hi.x || lo.y >= hi.y || lo.z >= hi.z) return {a};
    std::vector<Box> out;
    auto append = [&](glm::vec3 l, glm::vec3 h) { if (l.x<h.x && l.y<h.y && l.z<h.z) out.push_back({l,h}); };
    append(a.lo, {lo.x,a.hi.y,a.hi.z}); append({hi.x,a.lo.y,a.lo.z},a.hi);
    append({lo.x,a.lo.y,a.lo.z},{hi.x,lo.y,a.hi.z}); append({lo.x,hi.y,a.lo.z},{hi.x,a.hi.y,a.hi.z});
    append({lo.x,lo.y,a.lo.z},{hi.x,hi.y,lo.z}); append({lo.x,lo.y,hi.z},{hi.x,hi.y,a.hi.z});
    return out;
}

}

FluidVolumeQuadrature MakeBoxFluidVolume(const glm::vec3& halfExtents, unsigned n) {
    RequireSize(halfExtents);
    if (!n) throw std::invalid_argument("fluid volume sample count is zero");
    FluidVolumeQuadrature out; AppendBox(out,glm::vec3(0),halfExtents,n); return out;
}
FluidVolumeQuadrature MakeSphereFluidVolume(float radius, unsigned layers, unsigned pairs) {
    if (!(radius > 0) || !std::isfinite(radius) || !layers || !pairs) throw std::invalid_argument("invalid fluid sphere quadrature");
    constexpr double pi = 3.1415926535897932384626433832795;
    FluidVolumeQuadrature out;
    out.totalVolume = static_cast<float>((4.0/3.0)*pi*radius*radius*radius); out.boundRadius=radius; out.solidSphereRadius=radius;
    const float weight = out.totalVolume / static_cast<float>(2.0*layers*pairs);
    const float footprint = 0.5f * std::cbrt(weight);
    for (unsigned l=0;l<layers;++l) {
        const float r = radius * static_cast<float>(std::cbrt((l+0.5)/layers));
        for (unsigned k=0;k<pairs;++k) {
            const double y=(k+0.5)/pairs, azimuth=k*pi*(3.0-std::sqrt(5.0));
            const double radial=std::sqrt(1-y*y);
            const glm::vec3 p=r*glm::vec3(radial*std::cos(azimuth),y,radial*std::sin(azimuth));
            out.samples.push_back({p,glm::vec3(0),footprint,weight});
            out.samples.push_back({-p,glm::vec3(0),footprint,weight});
        }
    }
    return out;
}
FluidVolumeQuadrature MakeCompoundFluidVolume(const std::vector<CompoundBox>& boxes, unsigned n) {
    if (!n) throw std::invalid_argument("fluid volume sample count is zero");
    bool oriented=false;for(const auto& box:boxes){if(box.type!=ShapeType::Box)throw std::invalid_argument("fluid displacement requires box children");oriented|=box.rotation!=glm::quat(1,0,0,0);}
    if(oriented){
        // Existing axis-aligned union remains unchanged. Rotated children require
        // disjoint interiors; reject overlap instead of inventing an AABB union.
        for(size_t i=0;i<boxes.size();++i)for(size_t j=0;j<i;++j){RigidBody a,b;a.position=boxes[i].localCenter;a.orientation=boxes[i].rotation;b.position=boxes[j].localCenter;b.orientation=boxes[j].rotation;auto contacts=ComputeContacts(Shape::Box(boxes[i].halfExtents),a,Shape::Box(boxes[j].halfExtents),b,0);for(int k=0;k<contacts.count;++k)if(contacts.points[k].penetration>1e-6)throw std::invalid_argument("rotated fluid displacement children must have disjoint interiors");}
        FluidVolumeQuadrature out;for(auto& box:boxes){RequireSize(box.halfExtents);FluidVolumeQuadrature child;AppendBox(child,{},box.halfExtents,n);for(auto sample:child.samples){sample.localPosition=box.localCenter+box.rotation*sample.localPosition;sample.localRotation=box.rotation;out.samples.push_back(sample);}out.solidBoxes.push_back(box);out.totalVolume+=child.totalVolume;out.boundRadius=std::max(out.boundRadius,glm::length(box.localCenter)+glm::length(box.halfExtents));}return out;
    }
    std::vector<Box> partition;
    for (const auto& box:boxes) {
        RequireSize(box.halfExtents);
        if (!Finite(box.localCenter)) throw std::invalid_argument("nonfinite compound center");
        std::vector<Box> pending{{box.localCenter-box.halfExtents,box.localCenter+box.halfExtents}};
        for (const auto& covered:partition) {
            std::vector<Box> next;
            for (const auto& piece:pending) { const auto rest=Subtract(piece,covered); next.insert(next.end(),rest.begin(),rest.end()); }
            pending=std::move(next);
        }
        partition.insert(partition.end(),pending.begin(),pending.end());
    }
    FluidVolumeQuadrature out;
    for (const auto& box:partition) AppendBox(out,(box.lo+box.hi)*0.5f,(box.hi-box.lo)*0.5f,n);
    return out;
}

std::size_t FluidHydrostaticField::Hash::operator()(const Key& k) const {
    auto mix=[](std::uint64_t x) { x ^= x>>30; x *= UINT64_C(0xbf58476d1ce4e5b9); x ^= x>>27; x *= UINT64_C(0x94d049bb133111eb); return x^(x>>31); };
    return static_cast<std::size_t>(mix(static_cast<std::uint64_t>(k.x)) ^ mix(static_cast<std::uint64_t>(k.y)+UINT64_C(0x9e3779b97f4a7c15)) ^ mix(static_cast<std::uint64_t>(k.z)+UINT64_C(0x3c6ef372fe94f82a)));
}
FluidHydrostaticField::Key FluidHydrostaticField::Cell(const glm::vec3& p) const {
    return {static_cast<std::int64_t>(std::floor(static_cast<double>(p.x)/m_cellSize)),static_cast<std::int64_t>(std::floor(static_cast<double>(p.y)/m_cellSize)),static_cast<std::int64_t>(std::floor(static_cast<double>(p.z)/m_cellSize))};
}
void FluidHydrostaticField::Build(const std::vector<FluidParticle>& particles,float density,const std::vector<bool>* excluded) {
    if (!(density>0) || !std::isfinite(density) || (excluded && excluded->size()!=particles.size())) throw std::invalid_argument("invalid hydrostatic field density or exclusion mask");
    m_particles.clear(); m_cells.clear(); m_density=density; m_maxSpacing=0;
    m_min=glm::vec3(std::numeric_limits<float>::max()); m_max=-m_min;
    for (std::size_t i=0;i<particles.size();++i) {
        if (excluded && (*excluded)[i]) continue;
        const auto& p=particles[i];
        if (!(p.mass>0) || !std::isfinite(p.mass) || !Finite(p.position) || !Finite(p.velocity) || !Finite(p.acceleration)) throw std::invalid_argument("invalid particle in hydrostatic field");
        const float volume=p.mass/density, spacing=std::cbrt(volume);
        m_particles.push_back({p.position,p.velocity,p.acceleration,spacing,volume});
        m_maxSpacing=std::max(m_maxSpacing,spacing);
        m_min=glm::min(m_min,p.position-glm::vec3(spacing*0.5f)); m_max=glm::max(m_max,p.position+glm::vec3(spacing*0.5f));
    }
    m_cellSize=m_maxSpacing>0 ? 2*m_maxSpacing : 1;
    for (std::size_t i=0;i<m_particles.size();++i) m_cells[Cell(m_particles[i].position)].push_back(i);
    m_occupiedCells.clear();m_occupiedCells.reserve(m_cells.size());
    for(const auto& cell:m_cells)m_occupiedCells.push_back(cell.first);
    std::sort(m_occupiedCells.begin(),m_occupiedCells.end(),[](const Key& a,const Key& b){
        if(a.z!=b.z)return a.z<b.z;
        if(a.y!=b.y)return a.y<b.y;
        return a.x<b.x;
    });
}
FluidFieldSample FluidHydrostaticField::Query(const glm::vec3& point,const glm::vec3& direction,float halfHeight,float radius,const glm::vec3& tangentAxis,const FluidSolidQuery* solid,bool connectedWetSupport) const {
    FluidFieldSample out; out.density=m_density;
    if (m_particles.empty()) return out;
    if (!Finite(point) || !Finite(direction) || !(glm::length(direction)>0) || !std::isfinite(halfHeight) || halfHeight<0 || !std::isfinite(radius) || radius<0) throw std::invalid_argument("invalid hydrostatic query");
    const glm::dvec3 up=glm::normalize(glm::dvec3(direction));
    const glm::dvec3 projectedTangent=glm::dvec3(tangentAxis)-up*glm::dot(up,glm::dvec3(tangentAxis));
    if (!Finite(tangentAxis) || !(glm::dot(projectedTangent,projectedTangent)>0)) throw std::invalid_argument("invalid hydrostatic column tangent");
    const glm::dvec3 tangent=glm::normalize(projectedTangent), bitangent=glm::cross(up,tangent);
    // No column can overlap this sample if the entire particle interval AABB
    // lies above/below it. Reach extends donor searches, not occupied height.
    // Use a conservative rounding allowance; near-boundary queries still run
    // the original full expressions. Applies to every gravity orientation.
    const glm::dvec3 center=(glm::dvec3(m_min)+glm::dvec3(m_max))*.5;
    const glm::dvec3 half=(glm::dvec3(m_max)-glm::dvec3(m_min))*.5;
    const double projected=glm::dot(center-glm::dvec3(point),up);
    const double extentH=glm::dot(half,glm::abs(up));
    const double rounding=32*std::numeric_limits<float>::epsilon()*(glm::dot(glm::abs(center)+glm::abs(glm::dvec3(point))+half,glm::abs(up))+halfHeight+1);
    if(projected-extentH>halfHeight+rounding || projected+extentH< -halfHeight-rounding)return out;
    // A world AABB is loose for tilted/radial fields. Test the exact height
    // interval used by the unchanged column reconstruction before allocating
    // columns or testing solid occlusion. Once intervals straddle the sample,
    // this shortcut cannot reject and stops scanning. No world-up assumption.
    bool entirelyAbove=true,entirelyBelow=true;
    for(const auto& particle:m_particles) {
        const double h=glm::dot(glm::dvec3(particle.position)-glm::dvec3(point),up);
        entirelyAbove &= h-particle.spacing*.5 > halfHeight+rounding;
        entirelyBelow &= h+particle.spacing*.5 < -halfHeight-rounding;
        if(!entirelyAbove && !entirelyBelow)break;
    }
    if(entirelyAbove || entirelyBelow)return out;
    const double reach=radius+2.0*m_maxSpacing, reach2=reach*reach;
    const double columnWidth=m_maxSpacing, verticalReach=reach+halfHeight;
    // Anchor the lattice at actual field geometry. A query-centred lattice
    // puts many regular particle columns exactly on bin ties; float rotation
    // can then split successive rows into different columns. This reference
    // rotates/translates with the particle field. Its identity is deterministic
    // input order, so changing particle order can change this approximation.
    const glm::dvec3 reference=glm::dvec3(m_particles.front().position)-glm::dvec3(point);
    const double originX=glm::dot(reference,tangent), originZ=glm::dot(reference,bitangent);
    const float extent=static_cast<float>(reach+halfHeight+m_maxSpacing);
    if (glm::any(glm::lessThan(point+glm::vec3(extent),m_min)) || glm::any(glm::greaterThan(point-glm::vec3(extent),m_max))) return out;
    struct Interval { double lo,hi; std::size_t particle; };
    struct ColumnInterval {std::int64_t x,z;Interval interval;std::size_t visit;double actualX,actualZ;};
    std::vector<ColumnInterval> columns;columns.reserve(std::min(m_particles.size(),std::size_t{512}));
    double bulkWeight=0,extensionWeight=0;
    glm::dvec3 bulkVelocity(0),bulkAcceleration(0),extensionVelocity(0),extensionAcceleration(0);
    auto add=[&](std::size_t index) {
        const auto& p=m_particles[index];
        const glm::dvec3 d=glm::dvec3(p.position)-glm::dvec3(point);
        const double h=glm::dot(d,up);
        if (h-p.spacing*0.5>verticalReach || h+p.spacing*0.5< -verticalReach) return;
        const double actualX=glm::dot(d,tangent),actualZ=glm::dot(d,bitangent);
        const auto x=static_cast<std::int64_t>(std::floor((actualX-originX)/columnWidth+.5));
        const auto z=static_cast<std::int64_t>(std::floor((actualZ-originZ)/columnWidth+.5));
        const double columnX=originX+x*columnWidth, columnZ=originZ+z*columnWidth;
        const double lateral2=columnX*columnX+columnZ*columnZ;
        if (lateral2>=reach2) return;
        columns.push_back({x,z,{h-p.spacing*0.5,h+p.spacing*0.5,index},columns.size(),actualX,actualZ});
    };
    const Key lo=Cell(point-glm::vec3(extent)), hi=Cell(point+glm::vec3(extent));
    const double cells=(static_cast<double>(hi.x)-lo.x+1)*(static_cast<double>(hi.y)-lo.y+1)*(static_cast<double>(hi.z)-lo.z+1);
    // Equivalent brute scan prevents huge empty hash traversals for a large body.
    if (cells>4.0*m_particles.size()) for (std::size_t i=0;i<m_particles.size();++i) add(i);
    else if(cells>static_cast<double>(m_occupiedCells.size())) {
        // Visit the same nonempty cells in precisely the previous z/y/x order,
        // instead of hashing a large rectangular population of empty cells.
        for(const auto& cell:m_occupiedCells) {
            if(cell.x<lo.x||cell.x>hi.x||cell.y<lo.y||cell.y>hi.y||cell.z<lo.z||cell.z>hi.z)continue;
            for(const auto i:m_cells.at(cell))add(i);
        }
    }
    else for (auto z=lo.z;z<=hi.z;++z) for (auto y=lo.y;y<=hi.y;++y) for (auto x=lo.x;x<=hi.x;++x) {
        const auto it=m_cells.find({x,y,z}); if (it!=m_cells.end()) for (const auto i:it->second) add(i);
    }
    // Match ordered-map column order and original visitation within a column,
    // without allocating a tree node and growing vector for every column.
    std::sort(columns.begin(),columns.end(),[](const ColumnInterval& a,const ColumnInterval& b){
        if(a.x!=b.x)return a.x<b.x;
        if(a.z!=b.z)return a.z<b.z;
        return a.visit<b.visit;
    });
    std::optional<FluidHydrostaticDetail::PreparedSolidQuery> occlusion;
    if(solid && !columns.empty())occlusion.emplace(tangent,up,bitangent,columnWidth,halfHeight,*solid);
    double columnWeight=0, weightedFraction=0;
    std::vector<FluidWetColumnFootprint> wetFootprint;
    for(std::size_t begin=0,end=0;begin<columns.size();begin=end) {
        const auto x=columns[begin].x,z=columns[begin].z;
        end=begin+1;while(end<columns.size() && columns[end].x==x && columns[end].z==z)++end;
        const glm::dvec3 columnCenter=glm::dvec3(point)+tangent*(originX+x*columnWidth)+bitangent*(originZ+z*columnWidth);
        // Connectivity belongs to observed water, not the query's occlusion
        // mask or thin vertical footprint. Water below/around a solid still
        // connects its pool; it cannot itself contribute a masked wet sample.
        wetFootprint.push_back({x,z,{originX+x*columnWidth,originZ+z*columnWidth},false});

        Interval envelope=columns[begin].interval;
        for(std::size_t k=begin;k<end;++k) {
            envelope.lo=std::min(envelope.lo,columns[k].interval.lo);
            envelope.hi=std::max(envelope.hi,columns[k].interval.hi);
        }
        const double occupied=halfHeight>0 ? std::max(0.0,std::min(envelope.hi,static_cast<double>(halfHeight))-std::max(envelope.lo,-static_cast<double>(halfHeight))) : (envelope.lo<=0 && envelope.hi>=0 ? 1.0 : 0.0);
        wetFootprint.back().observedWet=occupied>0;
        if (occlusion && occlusion->Occluded(columnCenter)) continue;

        // The exterior field is extended through the SAME unoccluded columns
        // for occupancy and bulk motion. Sampling motion inside a solid's
        // displaced/contact shadow feeds its own one-way wall impulse back as
        // analytic hydrostatic support. This geometric exclusion has no mass,
        // velocity or object-name condition and leaves uniform free fall intact.
        for(std::size_t k=begin;k<end;++k) {
            const auto& interval=columns[k].interval;
            const auto& p=m_particles[interval.particle];
            const glm::dvec3 d=glm::dvec3(p.position)-glm::dvec3(point);
            const double distance2=glm::dot(d,d);
            if (distance2<reach2) {
                const double t=1.0-distance2/reach2,w=p.volume*t*t;
                bulkWeight+=w;bulkVelocity+=w*glm::dvec3(p.velocity);
                bulkAcceleration+=w*glm::dvec3(p.acceleration);
            }
            // A wet column envelope may have no local spherical donors.
            // Extend actual donor motion over the admitted geometric support
            // only in that missing-data case; do not overwrite resolved flow.
            const double overlap=std::max(0.0,std::min(interval.hi,verticalReach)-
                                              std::max(interval.lo,-verticalReach));
            const double columnX=originX+x*columnWidth,columnZ=originZ+z*columnWidth;
            const double t=1.0-(columnX*columnX+columnZ*columnZ)/reach2;
            const double w=p.volume*(overlap/p.spacing)*t*t;
            extensionWeight+=w;extensionVelocity+=w*glm::dvec3(p.velocity);
            extensionAcceleration+=w*glm::dvec3(p.acceleration);
        }
        // Hydrostatic occupancy reconstructs a locally filled column, rather
        // than treating disordered PBF sample intervals as literal air gaps.
        // Only the finite vertical neighbourhood above participates: detached
        // layers farther away do not become an infinite liquid column. Air
        // pockets inside this local envelope are unresolved by this mode.
        const double columnX=originX+x*columnWidth, columnZ=originZ+z*columnWidth;
        const double lateral2=columnX*columnX+columnZ*columnZ;
        const double t=1.0-lateral2/reach2, w=columnWidth*columnWidth*t*t;
        columnWeight+=w;
        weightedFraction+=w*(halfHeight>0 ? occupied/(2*halfHeight) : (occupied>0 ? 1.0 : 0.0));
        if (occupied > 0) {
            wetFootprint.back().wet = true;
        }
    }
    // A bin centre can be half a column from its actual particle centre;
    // adding the particle's half-width needs a full column on each side.
    // This is only a conservative rejection hull. Physical admission below
    // checks actual footprints and does not enlarge water to this bound.
    const double outerWidth=columnWidth*(connectedWetSupport?2:1);
    if (!FluidWetColumnsContainOrigin(wetFootprint,outerWidth,connectedWetSupport)) return out;
    if (connectedWetSupport) {
        if (occlusion) occlusion->SelectSupport(glm::dvec3(point));
        const auto covered=[&](std::int64_t x,std::int64_t z) {
            if (!occlusion) return false;
            const double px=originX+x*columnWidth,pz=originZ+z*columnWidth;
            if (px*px+pz*pz>=reach2) return false;
            return occlusion->Touches(glm::dvec3(point)+tangent*px+bitangent*pz);
        };
        const auto notEmpty=[&](std::int64_t x,std::int64_t z) {
            const double px=originX+x*columnWidth,pz=originZ+z*columnWidth;
            const auto worldColumn=glm::dvec3(point)+tangent*px+bitangent*pz;
            if(occlusion && occlusion->Occluded(worldColumn))return true;
            // Binning is not geometry: jittered particles in neighbouring
            // bins can still overlap this apparently empty column footprint.
            for(int dx:{-1,0,1})for(int dz:{-1,0,1}) {
                const auto key=std::make_pair(x+dx,z+dz);
                auto it=std::lower_bound(columns.begin(),columns.end(),key,
                    [](const auto& c,const auto& k){return c.x<k.first||(c.x==k.first&&c.z<k.second);});
                for(;it!=columns.end()&&it->x==key.first&&it->z==key.second;++it) {
                    const auto& particle=m_particles[it->interval.particle];
                    const double halfReach=(columnWidth+particle.spacing)*.5;
                    const double tolerance=8*std::numeric_limits<float>::epsilon()*
                        (1+glm::length(glm::dvec3(particle.position))+glm::length(glm::dvec3(point)));
                    if(std::abs(it->actualX-px)<halfReach-tolerance&&std::abs(it->actualZ-pz)<halfReach-tolerance)return true;
                }
            }
            return false;
        };
        const auto wetContact=[&](std::int64_t x,std::int64_t z) {
            if(!occlusion)return false;
            const auto key=std::make_pair(x,z);
            auto it=std::lower_bound(columns.begin(),columns.end(),key,
                [](const auto& c,const auto& k){return c.x<k.first||(c.x==k.first&&c.z<k.second);});
            for(;it!=columns.end()&&it->x==x&&it->z==z;++it) {
                const auto& particle=m_particles[it->interval.particle];
                const auto actual=glm::dvec3(point)+tangent*it->actualX+bitangent*it->actualZ;
                if(occlusion->Touches(actual,particle.spacing*.5))return true;
            }
            return false;
        };
        if (!FluidWetSupportThroughSolid(wetFootprint,columnWidth,{originX,originZ},covered,notEmpty,wetContact)) return out;
    }
    if (columnWeight>0) out.fraction=static_cast<float>(weightedFraction/columnWeight);
    if (out.fraction>0) {
        if (bulkWeight>0) { out.velocity=glm::vec3(bulkVelocity/bulkWeight); out.acceleration=glm::vec3(bulkAcceleration/bulkWeight); }
        else if (extensionWeight>0) { out.velocity=glm::vec3(extensionVelocity/extensionWeight); out.acceleration=glm::vec3(extensionAcceleration/extensionWeight); }
    }
    return out;
}
FluidHydrostaticResult EvaluateFluidHydrostatics(const FluidHydrostaticField& field,const FluidVolumeQuadrature& volume,const BodyTransform& pose,const GravityField& gravity,FluidVolumeGeometry geometry) {
    FluidHydrostaticResult out; out.totalVolume=volume.totalVolume; out.density=field.RestDensity(); out.submergedCentroid=pose.position;
    double wet=0; glm::dvec3 center(0),velocity(0),acceleration(0),force(0),torque(0);
    for (const auto& sample:volume.samples) {
        const glm::vec3 offset=pose.rotation*sample.localPosition, point=pose.position+offset, g=gravity.Sample(point);
        const glm::vec3 up=glm::dot(g,g)>0 ? -glm::normalize(g) : pose.rotation*glm::vec3(0,1,0);
        const glm::vec3 localUp=glm::conjugate(pose.rotation)*up;
        const float height=glm::dot(glm::abs(glm::conjugate(sample.localRotation)*localUp),sample.localHalfExtents)+sample.localRadius;
        glm::vec3 tangent(0);float best=-1;
        for (int axis=0;axis<3;++axis) {
            glm::vec3 localAxis(0);localAxis[axis]=1;
            const glm::vec3 candidate=pose.rotation*localAxis;
            const glm::vec3 projected=candidate-up*glm::dot(candidate,up);
            const float length2=glm::dot(projected,projected);
            if (length2>best) {best=length2;tangent=projected;}
        }
        const FluidSolidQuery solid{volume,pose};
        const auto fluid=field.Query(point,up,height,volume.boundRadius,tangent,
            geometry==FluidVolumeGeometry::DisplacedSolid ? &solid : nullptr,true);
        const double v=static_cast<double>(sample.volume)*fluid.fraction;
        const glm::dvec3 f=-static_cast<double>(fluid.density)*v*(glm::dvec3(g)-glm::dvec3(fluid.acceleration));
        wet+=v; center+=v*glm::dvec3(point); velocity+=v*glm::dvec3(fluid.velocity); acceleration+=v*glm::dvec3(fluid.acceleration);
        force+=f; torque+=glm::cross(glm::dvec3(offset),f);
    }
    out.submergedVolume=static_cast<float>(wet); out.fraction=volume.totalVolume>0 ? static_cast<float>(wet/volume.totalVolume) : 0;
    out.buoyancyForce=glm::vec3(force); out.buoyancyTorque=glm::vec3(torque);
    if (wet>0) { out.submergedCentroid=glm::vec3(center/wet); out.fluidVelocity=glm::vec3(velocity/wet); out.fluidAcceleration=glm::vec3(acceleration/wet); }
    return out;
}
