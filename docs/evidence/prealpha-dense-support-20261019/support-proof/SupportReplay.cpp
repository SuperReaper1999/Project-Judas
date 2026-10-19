// Research-only branch observer. Call the actual unmodified original method,
// then replay its arithmetic with named branches and require identical result.
#include "ReplayInputs.h"

namespace {
Json Failure(PhysicsWorld::Impl& world,const std::array<unsigned,4>& key,double start,double finish){
    Json out={{"partition_bounds",Json::array()}};
    auto done=[&](bool result,const char*reason){out["result"]=result;out["reason"]=reason;return out;};
    const auto&a=world.bodies[key[0]];const auto&b=world.bodies[key[1]];
    if(!world.ContinuationPrimitive(a)||!world.ContinuationPrimitive(b))return done(false,"unsupported_primitive");
    const auto&aa=world.ImpactPoseAt(key[0],start);const auto&bb=world.ImpactPoseAt(key[1],start);
    const float clearance=std::min(ContactSolver::kPenetrationSlop,
        float(.25*std::min(world.PrimitiveFeature(key[0],key[2]),world.PrimitiveFeature(key[1],key[3]))));
    out["clearance_m"]=clearance;
    const auto m=world.PairAt(key[0],key[1],key[2],key[3],start,clearance);
    out["manifold_rows"]=m.count;out["manifold_uncertain"]=m.uncertain;
    if(!m.count||m.uncertain)return done(false,!m.count?"no_start_margin_manifold":"uncertain_start_margin_manifold");
    const auto normal=m.points[0].preciseNormal;out["normal"]=Json::array({normal.x,normal.y,normal.z});
    out["normal_bits"]=contact_capture::Bits(normal);out["start_separations_m"]=Json::array();
    for(int i=0;i<m.count;++i){out["start_separations_m"].push_back(m.points[i].signedSeparation);
        if(m.points[i].signedSeparation < -clearance)return done(false,"already_over_unchanged_clearance");}
    std::array<const RigidMotionSegment*,2> segments{};
    for(unsigned side=0;side<2;++side){
        const auto&body=world.bodies[key[side]];if(!body.HasMotion())continue;
        const auto&path=body.motion.Segments();auto it=std::upper_bound(path.begin(),path.end(),start,
            [](double t,const auto&s){return t<s.begin;});
        if(it==path.begin())return done(false,"missing_segment");
        segments[side]=&*--it;
        if(finish>segments[side]->end)return done(false,"crosses_segment_discontinuity");
    }
    using contact_geometry::Iv;Iv norm(0);
    for(int k=0;k<3;++k)norm=norm+Iv(normal[k])*Iv(normal[k]);
    const Iv allowance=Iv(double(clearance))*contact_geometry::Sqrt(norm);
    std::string rangeFailure;
    auto gap=[&](double lo,double hi)->std::optional<Iv>{
        auto extent=[&](unsigned side,const PhysicsWorld::Impl::ImpactPoseSample&sample){
            auto pose=sample.pose;pose.position=glm::vec3(0);
            return rigid_motion_bounds::TryPrimitiveSweepProjection(world.bodies[key[side]].shape,
                segments[side],pose,pose,lo,hi,normal,glm::dvec3(0),*sample.orientation);
        };
        const auto first=extent(0,aa),second=extent(1,bb);
        const auto relative=rigid_motion_bounds::TryRelativeCentreProjection(
            segments[0],segments[1],aa.pose,bb.pose,lo,hi,normal);
        if(!first||!second||!relative){rangeFailure=!first?"first_extent_unavailable":!second?"second_extent_unavailable":"relative_center_unavailable";return std::nullopt;}
        return *relative+Iv(first->lo)-Iv(second->hi)+allowance;
    };
    const auto entire=gap(start,finish);if(!entire)return done(false,rangeFailure.c_str());
    out["whole_gap_interval_m"]={entire->lo,entire->hi};
    if(entire->lo>0)return done(true,"whole_interval_certified");
    const auto end=gap(finish,finish);if(!end)return done(false,rangeFailure.c_str());
    out["end_gap_interval_m"]={end->lo,end->hi};
    if(end->hi<=0)return done(false,"end_plane_fails_clearance");
    auto certify=[&](auto&&self,double lo,double hi,unsigned depth)->bool{
        const auto bound=gap(lo,hi);Json item={{"start",lo},{"finish",hi},{"remaining_depth",depth}};
        if(bound)item["gap_interval_m"]={bound->lo,bound->hi};else item["range_failure"]=rangeFailure;
        out["partition_bounds"].push_back(std::move(item));
        if(!bound)return false;
        if(bound->lo>0)return true;
        if(!depth||bound->hi<=0)return false;
        const double mid=(lo+hi)*.5;return self(self,lo,mid,depth-1)&&self(self,mid,hi,depth-1);
    };
    if(certify(certify,start,finish,3))return done(true,"bounded_partition_certified");
    return done(false,"bounded_partition_not_certified");
}
Json NativeDepth(PhysicsWorld::Impl&world,const std::array<unsigned,4>&key,double start,double finish){
    double maxDepth=0;unsigned over=0,uncertain=0;double at=start;
    constexpr unsigned intervals=256;
    for(unsigned sample=0;sample<=intervals;++sample){const double time=start+(finish-start)*(double(sample)/intervals);
        const auto m=world.PairAt(key[0],key[1],key[2],key[3],time,0);uncertain+=m.uncertain;
        for(int row=0;row<m.count;++row){const double depth=-m.points[row].signedSeparation;
            if(depth>maxDepth){maxDepth=depth;at=time;}over+=depth>ContactSolver::kPenetrationSlop;}
    }
    return {{"method","257 actual native manifold samples; diagnostic, NOT a continuous proof"},
        {"maximum_depth_m",maxDepth},{"maximum_at",at},{"rows_over_unchanged_slop",over},{"uncertain_samples",uncertain}};
}
}
int main(int argc,char**argv){
    if(argc!=3)throw std::runtime_error("usage: support-replay PAIR_TRACE OUTPUT_JSON");
    std::ifstream in(argv[1]);std::string line;Json output=Json::array();unsigned records=0,disagree=0,accepted=0;
    while(std::getline(in,line)){
        const auto input=Json::parse(line);++records;const auto key=input.at("key").get<std::array<unsigned,4>>();
        PhysicsWorld physics;physics.Init();auto&world=*physics.m_impl;
        world.bodies.resize(std::max(key[0],key[1])+1);Assign(world.bodies[key[0]],input.at("body_a"));Assign(world.bodies[key[1]],input.at("body_b"));world.aliveSlots={key[0],key[1]};
        const double start=Double(input.at("start")),finish=Double(input.at("finish"));world.stepDuration=Double(input.at("step_duration"));world.impactQueryAnchorTime=start;
        const bool actual=world.SupportContinuationSafe(key,start,finish);auto traced=Failure(world,key,start,finish);
        const bool parity=actual==traced.at("result").get<bool>();disagree+=!parity;accepted+=actual;
        traced["record"]=records;traced["stage"]=input.at("stage");traced["after_global_impact_sequence"]=input.at("after_global_impact_sequence");traced["start"]=start;traced["finish"]=finish;
        traced["actual_unmodified_result"]=actual;traced["diagnostic_branch_parity"]=parity;
        if(!actual)traced["sampled_native_depth"]=NativeDepth(world,key,start,finish);
        output.push_back(std::move(traced));
    }
    Json out={{"schema","judas.original-support-failure-replay.v1"},{"records",records},{"accepted",accepted},{"rejected",records-accepted},{"diagnostic_parity_failures",disagree},{"results",output}};
    std::ofstream(argv[2])<<out.dump(2)<<'\n';std::cout<<"support replay records="<<records<<" accepted="<<accepted<<" rejected="<<records-accepted<<" diagnostic_mismatches="<<disagree<<'\n';return disagree?1:0;
}
