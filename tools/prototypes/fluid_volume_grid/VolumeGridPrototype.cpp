// Disposable numerical prototype, not an engine backend. See GATES.md/README.txt.
#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <cstdint>
#include <limits>
#include <numeric>
#include <stdexcept>
#include <string>
#include <vector>
#include <sys/resource.h>

struct Vec {
    double x=0,y=0,z=0;
    double& operator[](int a) { return a==0?x:(a==1?y:z); }
    double operator[](int a) const { return a==0?x:(a==1?y:z); }
};
Vec operator+(Vec a,Vec b){return {a.x+b.x,a.y+b.y,a.z+b.z};}
Vec operator-(Vec a,Vec b){return {a.x-b.x,a.y-b.y,a.z-b.z};}
Vec operator*(Vec a,double s){return {a.x*s,a.y*s,a.z*s};}
Vec operator/(Vec a,double s){return a*(1/s);}
double Dot(Vec a,Vec b){return a.x*b.x+a.y*b.y+a.z*b.z;}
Vec Cross(Vec a,Vec b){return {a.y*b.z-a.z*b.y,a.z*b.x-a.x*b.z,a.x*b.y-a.y*b.x};}
double Norm(Vec a){return std::sqrt(Dot(a,a));}
bool Finite(Vec a){return std::isfinite(a.x)&&std::isfinite(a.y)&&std::isfinite(a.z);}
using Clock=std::chrono::steady_clock;
double Ms(Clock::time_point start){return std::chrono::duration<double,std::milli>(Clock::now()-start).count();}
struct Stats {double projection=0,transport=0,total=0,maxDiv=0,residual=0,dualMassError=0;int iterations=0;};

class Grid {
public:
    int nx,ny,nz;
    double h;
    std::vector<double> water,pressure;
    std::array<std::vector<double>,3> velocity;
    std::vector<unsigned char> solid;
    Stats stats;
    Grid(int x,int y,int z,double spacing):nx(x),ny(y),nz(z),h(spacing),water(x*y*z,0),pressure(x*y*z,0),solid(x*y*z,0){
        velocity[0].resize((nx+1)*ny*nz);velocity[1].resize(nx*(ny+1)*nz);velocity[2].resize(nx*ny*(nz+1));
    }
    int Count(int a)const{return a==0?nx:(a==1?ny:nz);}
    int Cell(int i,int j,int k)const{return (k*ny+j)*nx+i;}
    bool Inside(int i,int j,int k)const{return i>=0&&j>=0&&k>=0&&i<nx&&j<ny&&k<nz;}
    int Face(int a,int i,int j,int k)const{return a==0?(k*ny+j)*(nx+1)+i:(a==1?(k*(ny+1)+j)*nx+i:(k*ny+j)*nx+i);}
    Vec Center(int i,int j,int k)const{return {(i+.5)*h,(j+.5)*h,(k+.5)*h};}
    double Density(int c)const{return 1+999*water[c];}
    template<class F> void Faces(F f)const {
        for(int a=0;a<3;++a)for(int k=0;k<nz+(a==2);++k)for(int j=0;j<ny+(a==1);++j)for(int i=0;i<nx+(a==0);++i){
            int l[3]={i,j,k},r[3]={i,j,k};--l[a];
            const int left=Inside(l[0],l[1],l[2])?Cell(l[0],l[1],l[2]):-1;
            const int right=Inside(r[0],r[1],r[2])?Cell(r[0],r[1],r[2]):-1;
            f(a,i,j,k,Face(a,i,j,k),left,right);
        }
    }
    bool Fluid(int c)const{return c>=0&&!solid[c];}
    bool Open(int left,int right)const{return Fluid(left)&&Fluid(right);}
    double Beta(int left,int right)const{return Open(left,right)?2/(Density(left)+Density(right)):0;}
    double Volume()const{return std::accumulate(water.begin(),water.end(),0.0)*h*h*h;}
    double Divergence(int i,int j,int k,const std::array<std::vector<double>,3>& u)const{
        return (u[0][Face(0,i+1,j,k)]-u[0][Face(0,i,j,k)]+u[1][Face(1,i,j+1,k)]-u[1][Face(1,i,j,k)]+u[2][Face(2,i,j,k+1)]-u[2][Face(2,i,j,k)])/h;
    }
    double Energy(const std::array<std::vector<double>,3>& u)const{
        double e=0;Faces([&](int a,int,int,int,int f,int l,int r){if(Open(l,r))e+=.5*u[a][f]*u[a][f]/Beta(l,r)*h*h*h;});return e;
    }
    std::size_t OwnedBytes()const{
        std::size_t size=(water.capacity()+pressure.capacity())*sizeof(double)+solid.capacity();
        for(auto& u:velocity) size+=u.capacity()*sizeof(double);
        return size;
    }
    bool AdmitBox(Vec lo,Vec hi,double angle,std::string& error) const{
        if(!Finite(lo)||!Finite(hi)||!std::isfinite(angle)){error="nonfinite solid";return false;}
        if(angle!=0){error="rotated/cut solids require real cut geometry; unsupported by prototype";return false;}
        for(int a=0;a<3;++a)if(!(lo[a]>=0&&hi[a]>lo[a]&&hi[a]<=Count(a)*h)||std::abs(lo[a]/h-std::round(lo[a]/h))>1e-12||std::abs(hi[a]/h-std::round(hi[a]/h))>1e-12){error="solid boundary must align to grid; thin/subcell solids unsupported";return false;}
        return true;
    }
    bool AddBox(Vec lo,Vec hi,double angle,std::string& error){
        if(!AdmitBox(lo,hi,angle,error))return false;
        for(int k=0;k<nz;++k)for(int j=0;j<ny;++j)for(int i=0;i<nx;++i){auto p=Center(i,j,k);if(p.x>lo.x&&p.x<hi.x&&p.y>lo.y&&p.y<hi.y&&p.z>lo.z&&p.z<hi.z){solid[Cell(i,j,k)]=1;water[Cell(i,j,k)]=0;}}
        return true;
    }
    // Solver owns no publication; invalid input/failure leaves external arrays unchanged.
    bool Project(const std::array<std::vector<double>,3>& input,double dt,Vec gravity,std::array<std::vector<double>,3>& output,std::vector<double>& outPressure,Stats& measured,std::string& error,int budget=2000,const std::vector<double>* occupied=nullptr)const{
        const auto& quantity=occupied?*occupied:water;
        const auto coefficient=[&](int l,int r){return Open(l,r)?2/(2+999*(quantity[l]+quantity[r])):0;};
        if(!(dt>0)||!std::isfinite(dt)||!Finite(gravity)){error="invalid timestep/gravity";return false;}
        for(std::size_t c=0;c<quantity.size();++c)if(!std::isfinite(quantity[c])||quantity[c]<-1e-12||quantity[c]>1+1e-12||(solid[c]&&quantity[c]!=0)){error="invalid occupied quantity";return false;}
        for(int a=0;a<3;++a)for(double v:input[a])if(!std::isfinite(v)){error="nonfinite velocity";return false;}
        auto u=input;
        Faces([&](int a,int,int,int,int f,int l,int r){u[a][f]=Open(l,r)?u[a][f]+dt*gravity[a]:0;});
        const std::size_t n=quantity.size();
        std::vector<double> diagonal(n,0),rhs(n,0),p(n,0),residual(n,0),z(n,0),direction(n,0),product(n,0);
        struct Edge {int left,right;double weight;};
        std::vector<Edge> edges;edges.reserve(3*n);
        Faces([&](int,int,int,int,int,int l,int r){if(Open(l,r)){const double w=coefficient(l,r)/(h*h);edges.push_back({l,r,w});diagonal[l]+=w;diagonal[r]+=w;}});
        for(int k=0;k<nz;++k)for(int j=0;j<ny;++j)for(int i=0;i<nx;++i){int c=Cell(i,j,k);if(!solid[c])rhs[c]=-Divergence(i,j,k,u)/dt;}
        // Identify components instead of pinning an arbitrary global row across barriers.
        std::vector<int> component(n,-1);std::vector<std::vector<int>> members;
        for(std::size_t start=0;start<n;++start)if(!solid[start]&&component[start]<0){
            const int id=static_cast<int>(members.size());members.emplace_back();auto& list=members.back();list.push_back(static_cast<int>(start));component[start]=id;
            for(std::size_t q=0;q<list.size();++q){int c=list[q],i=c%nx,j=(c/nx)%ny,k=c/(nx*ny);for(int a=0;a<3;++a)for(int sign:{-1,1}){int ijk[3]={i,j,k};ijk[a]+=sign;if(!Inside(ijk[0],ijk[1],ijk[2]))continue;int other=Cell(ijk[0],ijk[1],ijk[2]);if(!solid[other]&&component[other]<0){component[other]=id;list.push_back(other);}}}
        }
        for(const auto& list:members){double sum=0,scale=0;for(int c:list){sum+=rhs[c];scale+=std::abs(rhs[c]);}if(std::abs(sum)>1e-10*std::max(1.0,scale)){error="incompatible sealed component";return false;}const double roundoffMean=sum/list.size();for(int c:list)rhs[c]-=roundoffMean;}
        const auto multiply=[&](const std::vector<double>& x,std::vector<double>& y){
            std::fill(y.begin(),y.end(),0);
            for(const auto& edge:edges){const double flux=edge.weight*(x[edge.left]-x[edge.right]);y[edge.left]+=flux;y[edge.right]-=flux;}
        };
        const auto dot=[](const std::vector<double>& x,const std::vector<double>& y){double s=0;for(std::size_t c=0;c<x.size();++c)s+=x[c]*y[c];return s;};
        double maxRhs=0;for(double b:rhs)maxRhs=std::max(maxRhs,std::abs(b));
        const double tolerance=std::max(1e-11,1e-11*maxRhs);
        residual=rhs;
        for(std::size_t c=0;c<n;++c)z[c]=diagonal[c]>0?residual[c]/diagonal[c]:0;
        direction=z;double rz=dot(residual,z);bool converged=false;
        for(int iteration=0;iteration<=budget;++iteration){
            double largest=0;for(double r:residual)largest=std::max(largest,std::abs(r));measured.residual=largest;
            if(largest<=tolerance){converged=true;break;}
            if(iteration==budget)break;
            multiply(direction,product);const double curvature=dot(direction,product);
            if(!(curvature>0)||!std::isfinite(curvature)||!std::isfinite(rz)){error="pressure operator breakdown";return false;}
            const double alpha=rz/curvature;
            for(std::size_t c=0;c<n;++c){p[c]+=alpha*direction[c];residual[c]-=alpha*product[c];z[c]=diagonal[c]>0?residual[c]/diagonal[c]:0;}
            const double next=dot(residual,z),beta=next/rz;rz=next;
            for(std::size_t c=0;c<n;++c)direction[c]=z[c]+beta*direction[c];
            measured.iterations=iteration+1;
        }
        if(!converged){error="pressure iteration budget exhausted; state not published";return false;}
        for(const auto& list:members){double mean=0;for(int c:list)mean+=p[c];mean/=list.size();for(int c:list)p[c]-=mean;}
        Faces([&](int a,int,int,int,int f,int l,int r){if(Open(l,r))u[a][f]-=dt*coefficient(l,r)*(p[r]-p[l])/h;});
        measured.maxDiv=0;
        for(int k=0;k<nz;++k)for(int j=0;j<ny;++j)for(int i=0;i<nx;++i)if(!solid[Cell(i,j,k)])measured.maxDiv=std::max(measured.maxDiv,std::abs(Divergence(i,j,k,u)));
        if(!std::isfinite(measured.maxDiv)||measured.maxDiv>1e-7){error="pressure publication divergence failed";return false;}
        output=std::move(u);outPressure=std::move(p);return true;
    }
    bool Advect(const std::array<std::vector<double>,3>& u,double dt,std::vector<double>& out,std::string& error,std::array<std::vector<double>,3>* outMassFlux=nullptr)const{
        std::vector<double> outgoing(water.size(),0),next=water;
        Faces([&](int a,int,int,int,int f,int l,int r){if(Open(l,r)){double q=dt/h*u[a][f];outgoing[q>=0?l:r]+=std::abs(q);}});
        if(*std::max_element(outgoing.begin(),outgoing.end())>1+1e-12){error="volume transport CFL exceeded; state not published";return false;}
        Faces([&](int a,int,int,int,int f,int l,int r){if(Open(l,r)){double q=dt/h*u[a][f];double amount=q*(q>=0?water[l]:water[r]);next[l]-=amount;next[r]+=amount;}});
        for(double value:next)if(!std::isfinite(value)||value < -1e-10||value>1+1e-10){error="unbounded volume transport; state not published";return false;}
        if(outMassFlux){
            *outMassFlux=u;
            Faces([&](int a,int,int,int,int f,int l,int r){(*outMassFlux)[a][f]=Open(l,r)?u[a][f]*(1+999*(u[a][f]>=0?water[l]:water[r])):0;});
        }
        out=std::move(next);return true;
    }
    // Staggered dual cells use the average of the same primal mass fluxes.
    // Thus momentum and VOF use identical mass transport, not separately
    // interpolated density. First-order upwind momentum is dissipative.
    bool AdvectMomentum(const std::array<std::vector<double>,3>& u,
                        const std::array<std::vector<double>,3>& massFlux,
                        const std::vector<double>& nextWater,double dt,
                        std::array<std::vector<double>,3>& out,double& maxMassError,
                        std::string& error)const{
        std::array<std::vector<double>,3> momentum=u,evolvedMass=u,outgoingMass=u;
        for(auto& field:outgoingMass)std::fill(field.begin(),field.end(),0);
        const auto dualMass=[&](const std::vector<double>& quantity,int l,int r){
            return .5*((Fluid(l)?1+999*quantity[l]:0)+(Fluid(r)?1+999*quantity[r]:0));
        };
        Faces([&](int a,int,int,int,int f,int l,int r){const double m=dualMass(water,l,r);momentum[a][f]=m*u[a][f];evolvedMass[a][f]=m;});
        const auto flux=[&](int a,const int* xyz){
            for(int b=0;b<3;++b)if(xyz[b]<0||xyz[b]>=Count(b)+(a==b))return 0.0;
            return massFlux[a][Face(a,xyz[0],xyz[1],xyz[2])];
        };
        Faces([&](int a,int i,int j,int k,int f,int,int){
            for(int b=0;b<3;++b){
                int neighbor[3]={i,j,k};++neighbor[b];
                if(neighbor[b]>=Count(b)+(a==b))continue;
                const int other=Face(a,neighbor[0],neighbor[1],neighbor[2]);
                int left[3]={i,j,k},right[3]={i,j,k};
                if(a==b)++right[b];else{--left[a];++left[b];++right[b];}
                const double transportedMass=.5*(flux(b,left)+flux(b,right))*dt/h;
                const double transportedMomentum=transportedMass*(transportedMass>=0?u[a][f]:u[a][other]);
                outgoingMass[a][transportedMass>=0?f:other]+=std::abs(transportedMass);
                momentum[a][f]-=transportedMomentum;momentum[a][other]+=transportedMomentum;
                evolvedMass[a][f]-=transportedMass;evolvedMass[a][other]+=transportedMass;
            }
        });
        bool valid=true;auto next=u;maxMassError=0;int debugRows=0;
        Faces([&](int a,int,int,int,int f,int l,int r){
            const double actual=dualMass(nextWater,l,r);
            if(outgoingMass[a][f]>dualMass(water,l,r)+1e-10){valid=false;if(debugRows++<8)std::fprintf(stderr,"DUAL CFL a=%d f=%d outgoing=%.17g mass=%.17g\n",a,f,outgoingMass[a][f],dualMass(water,l,r));}
            maxMassError=std::max(maxMassError,std::abs(evolvedMass[a][f]-actual));
            if(std::abs(evolvedMass[a][f]-actual)>1e-9*std::max(1.0,actual)){valid=false;if(debugRows++<8)std::fprintf(stderr,"DUAL MASS a=%d f=%d evolved=%.17g actual=%.17g\n",a,f,evolvedMass[a][f],actual);}
            next[a][f]=Open(l,r)?momentum[a][f]/actual:0;
            if(!std::isfinite(next[a][f]))valid=false;
        });
        if(!valid){error="primal/dual mass flux disagreement or dual CFL exceeded; state not published";return false;}
        out=std::move(next);return true;
    }
    bool Step(double dt,Vec gravity,std::string& error,int budget=2000){
        const auto total=Clock::now();Stats measured;
        auto start=Clock::now();
        std::array<std::vector<double>,3> projected,advected,finalVelocity,massFlux;
        std::vector<double> firstPressure,nextPressure,nextWater;
        if(!Project(velocity,dt,gravity,projected,firstPressure,measured,error,budget)) return false;
        measured.projection=Ms(start);start=Clock::now();
        if(!Advect(projected,dt,nextWater,error,&massFlux)) return false;
        if(!AdvectMomentum(projected,massFlux,nextWater,dt,advected,measured.dualMassError,error)) return false;
        measured.transport=Ms(start);start=Clock::now();
        Stats second;
        if(!Project(advected,dt,{0,0,0},finalVelocity,nextPressure,second,error,budget,&nextWater))return false;
        measured.projection+=Ms(start);measured.iterations+=second.iterations;
        measured.maxDiv=std::max(measured.maxDiv,second.maxDiv);measured.residual=std::max(measured.residual,second.residual);
        for(std::size_t c=0;c<nextPressure.size();++c)nextPressure[c]+=firstPressure[c];
        velocity=std::move(finalVelocity);pressure=std::move(nextPressure);water=std::move(nextWater);measured.total=Ms(total);stats=measured;return true;
    }
    double Sample(const std::vector<double>& field,Vec p)const{
        int lo[3];double t[3];for(int a=0;a<3;++a){const double at=p[a]/h-.5;lo[a]=static_cast<int>(std::floor(at));t[a]=at-lo[a];if(lo[a]<0||lo[a]+1>=Count(a))throw std::runtime_error("query outside interior interpolation support");}
        double result=0;for(int k=0;k<2;++k)for(int j=0;j<2;++j)for(int i=0;i<2;++i)result+=field[Cell(lo[0]+i,lo[1]+j,lo[2]+k)]*(i?t[0]:1-t[0])*(j?t[1]:1-t[1])*(k?t[2]:1-t[2]);return result;
    }
    std::pair<Vec,Vec> SolidTraction(Vec gravity,Vec origin)const{
        Vec force,torque;
        Faces([&](int a,int i,int j,int k,int,int l,int r){
            const bool leftSolid=l>=0&&solid[l],rightSolid=r>=0&&solid[r];if(leftSolid==rightSolid||!(Fluid(l)||Fluid(r)))return;
            const int fluid=Fluid(l)?l:r;Vec x={(i+.5)*h,(j+.5)*h,(k+.5)*h};x[a]-=.5*h;
            const Vec fluidCenter=Center(fluid%nx,(fluid/nx)%ny,fluid/(nx*ny));
            // Half-cell hydrostatic reconstruction is exact only in a static affine patch.
            const double faceP=pressure[fluid]+Density(fluid)*Dot(gravity,x-fluidCenter);
            Vec normal;normal[a]=leftSolid?1:-1;const Vec f=normal*(-faceP*h*h);force=force+f;torque=torque+Cross(x-origin,f);
        });return {force,torque};
    }
    // Actual scalar-field isosurface extraction; display does not own water volume.
    std::vector<Vec> Surface()const{
        std::vector<Vec> triangles;constexpr int corner[8][3]={{0,0,0},{1,0,0},{0,1,0},{1,1,0},{0,0,1},{1,0,1},{0,1,1},{1,1,1}};
        constexpr int tetra[6][4]={{0,1,3,7},{0,3,2,7},{0,2,6,7},{0,6,4,7},{0,4,5,7},{0,5,1,7}};
        constexpr int edge[6][2]={{0,1},{0,2},{0,3},{1,2},{1,3},{2,3}};
        for(int k=0;k<nz-1;++k)for(int j=0;j<ny-1;++j)for(int i=0;i<nx-1;++i){
            Vec p[8];double a[8];for(int c=0;c<8;++c){p[c]=Center(i+corner[c][0],j+corner[c][1],k+corner[c][2]);a[c]=water[Cell(i+corner[c][0],j+corner[c][1],k+corner[c][2])]-.5;}
            for(const auto& t:tetra){std::vector<Vec> cut;for(auto& e:edge){int u=t[e[0]],v=t[e[1]];if((a[u]>=0)!=(a[v]>=0))cut.push_back(p[u]+(p[v]-p[u])*(a[u]/(a[u]-a[v])));}
                std::vector<Vec> unique;for(auto x:cut){bool seen=false;for(auto y:unique)seen|=Dot(x-y,x-y)<1e-24;if(!seen)unique.push_back(x);}cut=std::move(unique);
                if(cut.size()==3)triangles.insert(triangles.end(),cut.begin(),cut.end());
                if(cut.size()==4){
                    Vec centre;for(auto p:cut)centre=centre+p;centre=centre/4;
                    const Vec axis=(cut[0]-centre)/Norm(cut[0]-centre);
                    Vec normal=Cross(cut[1]-cut[0],cut[2]-cut[0]);normal=normal/Norm(normal);
                    const Vec transverse=Cross(normal,axis);
                    std::sort(cut.begin(),cut.end(),[&](Vec x,Vec y){return std::atan2(Dot(x-centre,transverse),Dot(x-centre,axis))<std::atan2(Dot(y-centre,transverse),Dot(y-centre,axis));});
                    triangles.insert(triangles.end(),{cut[0],cut[1],cut[2],cut[0],cut[2],cut[3]});
                }
            }
        }return triangles;
    }
};

struct Checks {
    int total=0,failed=0;
    void Expect(bool passed,const std::string& label,double measured=0){++total;if(!passed)++failed;std::printf("%s %s measured=%.17g\n",passed?"PASS":"FAIL",label.c_str(),measured);}
};
bool EqualBits(const std::vector<double>& a,const std::vector<double>& b){return a.size()==b.size()&&std::memcmp(a.data(),b.data(),a.size()*sizeof(double))==0;}
std::uint64_t Digest(const Grid& grid){
    std::uint64_t value=1469598103934665603ULL;
    const auto append=[&](const std::vector<double>& field){const auto* bytes=reinterpret_cast<const unsigned char*>(field.data());for(std::size_t i=0;i<field.size()*sizeof(double);++i){value^=bytes[i];value*=1099511628211ULL;}};
    append(grid.water);append(grid.pressure);for(const auto& field:grid.velocity)append(field);return value;
}
Vec Rotate(Vec p){const Vec n=Vec{1,2,3}/std::sqrt(14.0);const double c=std::cos(.73),s=std::sin(.73);return p*c+Cross(n,p)*s+n*(Dot(n,p)*(1-c));}

void Hydrostatic(Checks& checks,Vec gravity,const char* label){
    Grid grid(8,8,8,.5);std::fill(grid.water.begin(),grid.water.end(),1);std::string error;
    checks.Expect(grid.AddBox({1.5,1.5,1.5},{2.5,2.5,2.5},0,error),std::string(label)+" aligned solid admission");
    checks.Expect(grid.Step(1.0/90,gravity,error),std::string(label)+" pressure solve");
    double speed=0;for(auto& u:grid.velocity)for(double v:u)speed=std::max(speed,std::abs(v));
    checks.Expect(speed<2e-10,std::string(label)+" static hydrostatic velocity",speed);
    const Vec origin={-.3,.2,-.7};const auto reaction=grid.SolidTraction(gravity,origin);const Vec expected=gravity*(-1000);
    const double forceError=Norm(reaction.first-expected),torqueError=Norm(reaction.second-Cross(Vec{2,2,2}-origin,expected));
    checks.Expect(forceError<2e-6,std::string(label)+" displaced-volume force",forceError);
    checks.Expect(torqueError<8e-6,std::string(label)+" off-centre torque",torqueError);
    for(double mass:{500.0,1000.0,2000.0}){const Vec accel=(reaction.first+gravity*mass)/mass;const double againstGravity=Dot(accel,gravity);checks.Expect(mass<1000?againstGravity<0:(mass==1000?Norm(accel)<1e-8:againstGravity>0),std::string(label)+" force-only density classification "+std::to_string(mass),againstGravity);}
    const auto worldForce=Rotate(reaction.first),expectedWorld=Rotate(gravity)*(-1000);checks.Expect(Norm(worldForce-expectedWorld)<2e-6,std::string(label)+" rotated-chart force",Norm(worldForce-expectedWorld));
}

void Gates(){
    Checks checks;std::string error;
    Hydrostatic(checks,{0,-9.81,0},"vertical");Hydrostatic(checks,{1.2,-9.81,.7},"oblique gravity");
    Grid patch(8,8,8,.5);const Vec gradient={1.2,-3.7,.6};
    for(int k=0;k<8;++k)for(int j=0;j<8;++j)for(int i=0;i<8;++i)patch.pressure[patch.Cell(i,j,k)]=2+Dot(gradient,patch.Center(i,j,k));
    double worst=0;for(Vec p:std::vector<Vec>{{1.13,1.42,1.89},{2.2,1.7,.9},{.7,2.6,2.2}})worst=std::max(worst,std::abs(patch.Sample(patch.pressure,p)-(2+Dot(gradient,p))));
    checks.Expect(worst<4e-15,"affine point reconstruction",worst);
    // A non-axis-aligned finite-face query is a query test, not an oblique cut-cell solver.
    const Vec centre={2,2,2},tangent=Rotate({0,1,0});double faceIntegral=0;
    for(double s:{-.5,.5})for(double t:{-.5,.5})faceIntegral+=.25*patch.Sample(patch.pressure,centre+tangent*(s/std::sqrt(3.0))+Rotate({0,0,1})*(t/std::sqrt(3.0)));
    checks.Expect(std::abs(faceIntegral-(2+Dot(gradient,centre)))<4e-15,"oblique affine face pressure quadrature",faceIntegral-(2+Dot(gradient,centre)));
    const double trueFlux=Dot(Vec{0,-1,0},Vec{1,0,0});const double badTwoPoint=((1-1.0/3)-(1-2.0/3))/(2.0/3);
    checks.Expect(trueFlux==0&&std::abs(badTwoPoint-.5)<1e-15,"negative control detects skew two-point false normal flux",badTwoPoint);
    checks.Expect(!patch.AdmitBox({1.5,1.5,1.5},{2.5,2.5,2.5},.1,error),"rotated solid explicitly rejected");
    checks.Expect(!patch.AdmitBox({1.5,1.5,1.5},{2.5,2.5,2.68},0,error),"subcell solid explicitly rejected");
    Grid transport(4,2,2,1);transport.water[transport.Cell(1,0,0)]=.5;transport.velocity[0][transport.Face(0,2,0,0)]=1;std::vector<double> advected;
    checks.Expect(transport.Advect(transport.velocity,.1,advected,error),"paired volume transport succeeds");
    checks.Expect(std::abs(advected[transport.Cell(1,0,0)]-.45)<1e-15&&std::abs(advected[transport.Cell(2,0,0)]-.05)<1e-15,"known-answer donor and receiver quantities");
    checks.Expect(std::accumulate(advected.begin(),advected.end(),0.0)==.5,"paired transfer conserves exact binary initial quantity");
    std::vector<double> rejected={123};checks.Expect(!transport.Advect(transport.velocity,2,rejected,error)&&rejected==std::vector<double>{123},"CFL rejection preserves output");
    Grid disturbed(12,8,10,.25);std::fill(disturbed.water.begin(),disturbed.water.end(),1);
    disturbed.Faces([&](int a,int i,int j,int k,int f,int l,int r){if(disturbed.Open(l,r))disturbed.velocity[a][f]=.2*std::sin(1.3*i+.8*j+.6*k+2*a);});
    const double energyBefore=disturbed.Energy(disturbed.velocity);const auto beforeWater=disturbed.water;const auto beforeVelocity=disturbed.velocity;const auto beforePressure=disturbed.pressure;
    checks.Expect(!disturbed.Step(.01,{0,-9.81,0},error,0),"forced solve failure rejected");
    bool rollback=EqualBits(beforeWater,disturbed.water)&&EqualBits(beforePressure,disturbed.pressure);for(int a=0;a<3;++a)rollback&=EqualBits(beforeVelocity[a],disturbed.velocity[a]);checks.Expect(rollback,"failed projection publishes no partial state");
    checks.Expect(!disturbed.Step(.01,{0,std::numeric_limits<double>::quiet_NaN(),0},error),"NaN gravity rejected");
    checks.Expect(disturbed.Step(.01,{0,0,0},error),"disturbed pressure projection succeeds");
    checks.Expect(disturbed.stats.maxDiv<1e-7,"projected divergence",disturbed.stats.maxDiv);
    const double energyAfter=disturbed.Energy(disturbed.velocity);checks.Expect(energyAfter<=energyBefore+1e-9,"zero-work pressure projection cannot add kinetic energy",energyAfter-energyBefore);
    Grid pool(24,6,16,.5);for(int k=0;k<pool.nz;++k)for(int j=0;j<pool.ny;++j)for(int i=0;i<pool.nx;++i)pool.water[pool.Cell(i,j,k)]=std::clamp((1.875-j*.5)/.5,0.0,1.0);
    checks.Expect(pool.Volume()==180,"diagnostic pool initial volume",pool.Volume());
    pool.Faces([&](int a,int i,int j,int k,int f,int l,int r){if(pool.Open(l,r))pool.velocity[a][f]=.08*std::sin(.7*i+.9*j+1.1*k+2*a);});
    const double initial=pool.Volume();double maxDiv=0;bool steps=true;for(int step=0;step<90;++step){if(!pool.Step(1.0/90,{0,-9.81,0},error)){std::printf("POOL failure step=%d %s\n",step,error.c_str());steps=false;break;}maxDiv=std::max(maxDiv,pool.stats.maxDiv);}
    checks.Expect(steps,"90 actual grid pressure/volume updates");
    checks.Expect(std::abs(pool.Volume()-initial)<1e-9,"pool volume retained without renormalization",pool.Volume()-initial);
    double minimum=1,maximum=0;for(double a:pool.water){minimum=std::min(minimum,a);maximum=std::max(maximum,a);}checks.Expect(minimum>=-1e-10&&maximum<=1+1e-10,"bounded fractions without clamping",maximum);
    checks.Expect(maxDiv<1e-7,"pool pressure divergence",maxDiv);
    auto surface=pool.Surface();checks.Expect(!surface.empty(),"surface extracted from authoritative fractions",surface.size()/3);
    // Independent primal/dual consistency and actual momentum transport controls.
    Grid momentum(10,6,6,.5);
    for(int k=0;k<momentum.nz;++k)for(int j=0;j<momentum.ny;++j)for(int i=0;i<momentum.nx;++i)momentum.water[momentum.Cell(i,j,k)]=.2+.15*(i%3);
    const auto stream=[&](int i,int j){return i>=2&&i<=momentum.nx-2?momentum.h*std::min(j,momentum.ny-j):0.0;};
    momentum.Faces([&](int a,int i,int j,int,int f,int l,int r){momentum.velocity[a][f]=!momentum.Open(l,r)?0:(a==0?(stream(i,j+1)-stream(i,j))/momentum.h:(a==1?-(stream(i+1,j)-stream(i,j))/momentum.h:0));});
    std::array<std::vector<double>,3> mixtureFlux,transportedVelocity;std::vector<double> transportedWater;double dualError=0;
    checks.Expect(momentum.Advect(momentum.velocity,.01,transportedWater,error,&mixtureFlux),"variable-density primal mass transport");
    const bool momentumOk=momentum.AdvectMomentum(momentum.velocity,mixtureFlux,transportedWater,.01,transportedVelocity,dualError,error);checks.Expect(momentumOk,"same-flux dual momentum transport");if(!momentumOk)throw std::runtime_error(error);
    double constantError=0;momentum.Faces([&](int a,int i,int j,int,int f,int l,int r){if(a==0&&i>=3&&i<=momentum.nx-3&&j==1&&momentum.Open(l,r))constantError=std::max(constantError,std::abs(transportedVelocity[a][f]-1));});
    checks.Expect(dualError<1e-10,"dual mass equals averaged transported cell mass",dualError);
    checks.Expect(constantError<1e-14,"variable-density transport preserves constant interior velocity",constantError);
    std::fill(momentum.water.begin(),momentum.water.end(),.5);
    const auto markerStream=[&](int i,int j){double extra=i<=4?0:(i<=7?-.1*(i-4):-.1*(10-i));return stream(i,j)+((j>0&&j<momentum.ny)?extra:0);};
    momentum.Faces([&](int a,int i,int j,int,int f,int l,int r){momentum.velocity[a][f]=!momentum.Open(l,r)?0:(a==0?(markerStream(i,j+1)-markerStream(i,j))/momentum.h:(a==1?-(markerStream(i+1,j)-markerStream(i,j))/momentum.h:0));});
    checks.Expect(momentum.Advect(momentum.velocity,.01,transportedWater,error,&mixtureFlux)&&momentum.AdvectMomentum(momentum.velocity,mixtureFlux,transportedWater,.01,transportedVelocity,dualError,error),"tangential momentum marker transport");
    const double marker=transportedVelocity[1][momentum.Face(1,4,2,2)],markerExpected=.2*(1-.01/.5);
    checks.Expect(std::abs(marker-markerExpected)<1e-14,"known-answer transported momentum marker",marker-markerExpected);
    checks.Expect(std::abs(.2-markerExpected)>1e-3,"negative control rejects frozen-velocity update",.2-markerExpected);
    Grid variable(8,6,6,.5);for(int k=0;k<variable.nz;++k)for(int j=0;j<variable.ny;++j)for(int i=0;i<variable.nx;++i)variable.water[variable.Cell(i,j,k)]=.1+.8*std::pow(std::sin(.7*i+.5*j+.8*k),2);
    variable.Faces([&](int a,int i,int j,int k,int f,int l,int r){if(variable.Open(l,r))variable.velocity[a][f]=.15*std::sin(.3*i+.7*j+.9*k+a);});
    const double variableEnergy=variable.Energy(variable.velocity),variableVolume=variable.Volume();
    checks.Expect(variable.Step(.01,{0,0,0},error),"variable-density full momentum/projection update");
    checks.Expect(variable.Energy(variable.velocity)<=variableEnergy+1e-8,"closed zero-work variable-density update does not add energy",variable.Energy(variable.velocity)-variableEnergy);
    checks.Expect(std::abs(variable.Volume()-variableVolume)<1e-10,"variable-density update retains water quantity",variable.Volume()-variableVolume);
    // Gravity-driven dam release: pressure is solved each step on changing density,
    // then the resulting flow actually transports the material, not a frozen field.
    Grid dam(24,12,12,.25);
    for(int k=0;k<dam.nz;++k)for(int j=0;j<dam.ny;++j)for(int i=0;i<dam.nx;++i)dam.water[dam.Cell(i,j,k)]=(i<8&&j<8)?1:0;
    const auto centreX=[](const Grid& g){double sum=0,weight=0;for(int k=0;k<g.nz;++k)for(int j=0;j<g.ny;++j)for(int i=0;i<g.nx;++i){double a=g.water[g.Cell(i,j,k)];sum+=a*(i+.5)*g.h;weight+=a;}return sum/weight;};
    const double damInitial=dam.Volume(),initialX=centreX(dam);bool damSteps=true;double damSpeed=0;
    for(int step=0;step<60;++step){if(!dam.Step(1.0/240,{0,-9.81,0},error)){std::printf("DAM failure step=%d %s\n",step,error.c_str());damSteps=false;break;}for(const auto& u:dam.velocity)for(double v:u)damSpeed=std::max(damSpeed,std::abs(v));}
    checks.Expect(damSteps,"gravity-driven dam pressure and transport updates");
    checks.Expect(std::abs(dam.Volume()-damInitial)<1e-9,"dam volume preserved without renormalization",dam.Volume()-damInitial);
    checks.Expect(centreX(dam)>initialX+1e-4&&damSpeed>1e-3,"dam flows under gravity and shifts water centre",centreX(dam)-initialX);
    // A barrier creates two separate pressure gauges. Disturbing one side must
    // neither move nor repressure the other side.
    Grid split(10,4,4,.5);std::fill(split.water.begin(),split.water.end(),1);
    checks.Expect(split.AddBox({2,0,0},{2.5,2,2},0,error),"separating whole-cell barrier admitted");
    split.Faces([&](int a,int i,int j,int k,int f,int l,int r){if(i<4&&split.Open(l,r))split.velocity[a][f]=.3*std::sin(.6*i+.7*j+.8*k+a);});
    checks.Expect(split.Step(.01,{0,0,0},error),"disconnected pressure components solve");
    double untouched=0;split.Faces([&](int a,int i,int,int,int f,int l,int r){if(i>=6&&split.Open(l,r))untouched=std::max(untouched,std::abs(split.velocity[a][f]));});
    checks.Expect(untouched==0,"disconnected undisturbed chamber remains exactly still",untouched);
    Grid cfl(4,4,4,1);std::fill(cfl.water.begin(),cfl.water.end(),.5);
    cfl.velocity[0][cfl.Face(0,1,1,1)]=10;cfl.velocity[1][cfl.Face(1,1,2,1)]=10;
    cfl.velocity[0][cfl.Face(0,1,2,1)]=-10;cfl.velocity[1][cfl.Face(1,0,2,1)]=-10;
    const auto cflWater=cfl.water,cflPressure=cfl.pressure;const auto cflVelocity=cfl.velocity;
    checks.Expect(!cfl.Step(1,{0,0,0},error),"completed projection then failed transport rejected");
    bool cflRollback=EqualBits(cflWater,cfl.water)&&EqualBits(cflPressure,cfl.pressure);for(int a=0;a<3;++a)cflRollback&=EqualBits(cflVelocity[a],cfl.velocity[a]);
    checks.Expect(cflRollback,"post-projection transport failure rolls back all public state");
    std::printf("SUMMARY checks=%d failures=%d pool_volume=%.17g water_mass=%.17g dam_shift=%.17g dam_peak=%.17g\n",checks.total,checks.failed,pool.Volume(),pool.Volume()*1000,centreX(dam)-initialX,damSpeed);
    std::printf("DIGEST pool=%016llx dam=%016llx variable=%016llx\n",static_cast<unsigned long long>(Digest(pool)),static_cast<unsigned long long>(Digest(dam)),static_cast<unsigned long long>(Digest(variable)));
    if(checks.failed)throw std::runtime_error("prototype gates failed; preserve this result");
}

double Percentile(std::vector<double> values,double q){std::sort(values.begin(),values.end());return values[std::min(values.size()-1,static_cast<std::size_t>(std::ceil(q*values.size())-1))];}
void Benchmark(int refine,int samples){
    Grid pool(24*refine,6*refine,16*refine,.5/refine);for(int k=0;k<pool.nz;++k)for(int j=0;j<pool.ny;++j)for(int i=0;i<pool.nx;++i)pool.water[pool.Cell(i,j,k)]=std::clamp((1.875-j*pool.h)/pool.h,0.0,1.0);
    pool.Faces([&](int a,int i,int j,int k,int f,int l,int r){if(pool.Open(l,r))pool.velocity[a][f]=.08*std::sin(.7*i/refine+.9*j/refine+1.1*k/refine+2*a);});
    std::vector<double> total,projection,transport,meshing,query;std::string error;std::size_t triangles=0;double querySum=0;
    std::printf("sample,cells,step_ms,projection_ms,transport_ms,mesh_ms,query_ms,pressure_iterations,max_divergence,water_volume,triangles\n");
    for(int step=0;step<samples;++step){if(!pool.Step(1.0/90,{0,-9.81,0},error))throw std::runtime_error(error);auto t=Clock::now();auto mesh=pool.Surface();const double meshMs=Ms(t);triangles=mesh.size()/3;t=Clock::now();for(int q=0;q<1000;++q){const Vec p={.5+10.5*(q%19)/18.0,.5+1.8*(q%13)/12.0,.5+6.5*(q%17)/16.0};querySum+=pool.Sample(pool.water,p);}const double queryMs=Ms(t);total.push_back(pool.stats.total);projection.push_back(pool.stats.projection);transport.push_back(pool.stats.transport);meshing.push_back(meshMs);query.push_back(queryMs);std::printf("%d,%zu,%.9g,%.9g,%.9g,%.9g,%.9g,%d,%.9g,%.17g,%zu\n",step,pool.water.size(),pool.stats.total,pool.stats.projection,pool.stats.transport,meshMs,queryMs,pool.stats.iterations,pool.stats.maxDiv,pool.Volume(),triangles);}
    struct rusage usage{};getrusage(RUSAGE_SELF,&usage);
    std::printf("BENCH cells=%zu samples=%d spacing=%.6g median_ms=%.9g p95_ms=%.9g worst_ms=%.9g projection_median=%.9g transport_median=%.9g mesh_median=%.9g query1000_median=%.9g owned_bytes=%zu peak_rss_kib=%ld water_volume=%.17g last_triangles=%zu query_checksum=%.17g\n",pool.water.size(),samples,pool.h,Percentile(total,.5),Percentile(total,.95),Percentile(total,1),Percentile(projection,.5),Percentile(transport,.5),Percentile(meshing,.5),Percentile(query,.5),pool.OwnedBytes(),usage.ru_maxrss,pool.Volume(),triangles,querySum);
}
int main(int argc,char** argv){
    try{
        if(argc==1){Gates();return 0;}
        if(argc<2||argc>4||std::string(argv[1])!="bench")
            throw std::runtime_error("usage: prototype [bench [refinement 1|2] [samples 1..1000]]");
        const auto bounded=[](const char* text,int maximum,const char* message){
            int value=0;
            if(!*text)throw std::runtime_error(message);
            for(const char* at=text;*at;++at){
                if(*at<'0'||*at>'9')throw std::runtime_error(message);
                const int digit=*at-'0';
                if(value>maximum/10||(value==maximum/10&&digit>maximum%10))
                    throw std::runtime_error(message);
                value=value*10+digit;
            }
            if(value==0)throw std::runtime_error(message);
            return value;
        };
        const int refinement=argc>2?bounded(argv[2],2,"refinement must be 1 or 2"):1;
        const int samples=argc>3?bounded(argv[3],1000,"samples must be 1..1000"):60;
        Benchmark(refinement,samples);return 0;
    }catch(const std::exception& error){std::fprintf(stderr,"ERROR %s\n",error.what());return 1;}
}
