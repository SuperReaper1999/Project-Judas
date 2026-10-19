// OFFLINE EXPERIMENT ONLY. Reuses the exact captured system loader.
#define main JudasOriginalOfflineReplayMain
#include "../contact-system-research/Replay.cpp"
#undef main

namespace block_trial {
using Matrix4 = std::array<std::array<double,4>,4>;
using Values4 = std::array<double,4>;
constexpr float epsilon = 1.0e-6f;

glm::vec3 Point(const RigidBody& body, const glm::vec3& r) {
    return body.linearVelocity + glm::cross(body.angularVelocity, r);
}
float Effective(const ContactConstraint& c, const glm::vec3& rA, const glm::vec3& rB,
                const glm::vec3& direction) {
    const glm::vec3 angularA = glm::cross(c.inverseInertiaA * glm::cross(rA, direction), rA);
    const glm::vec3 angularB = glm::cross(c.inverseInertiaB * glm::cross(rB, direction), rB);
    return c.bodyA->inverseMass + c.bodyB->inverseMass + glm::dot(direction, angularA + angularB);
}
void Apply(ContactConstraint& c, const glm::vec3& impulse) {
    if (!c.bodyA->IsStatic()) {
        c.bodyA->linearVelocity += impulse * c.bodyA->inverseMass;
        c.bodyA->angularVelocity += c.inverseInertiaA * glm::cross(c.velocityOffsetA, impulse);
    }
    if (!c.bodyB->IsStatic()) {
        c.bodyB->linearVelocity -= impulse * c.bodyB->inverseMass;
        c.bodyB->angularVelocity -= c.inverseInertiaB * glm::cross(c.velocityOffsetB, impulse);
    }
}
// Literal existing Coulomb update, with unchanged floating expressions.
void Friction(ContactConstraint& c) {
    const auto rA=c.velocityOffsetA,rB=c.velocityOffsetB;
    const glm::vec3 relative=Point(*c.bodyA,rA)-Point(*c.bodyB,rB);
    const glm::vec3 slip=relative-c.normal*glm::dot(relative,c.normal);
    const float slipSpeed=glm::length(slip),limit=c.friction*c.normalImpulse;
    glm::vec3 newTangent=c.tangentImpulse;
    if(slipSpeed>epsilon){
        const glm::vec3 direction=slip/slipSpeed;
        const float k=Effective(c,rA,rB,direction);
        if(k>epsilon)newTangent-=direction*(slipSpeed/k);
    }
    const float magnitude=glm::length(newTangent);
    if(magnitude>limit)newTangent=magnitude>epsilon?newTangent*(limit/magnitude):glm::vec3(0);
    const glm::vec3 applied=newTangent-c.tangentImpulse;
    c.tangentImpulse=newTangent;
    if(glm::dot(applied,applied)>0)Apply(c,applied);
}
void Scalar(ContactConstraint& c) {
    if(c.normalMass<=0)return;
    const float vn=glm::dot(Point(*c.bodyA,c.velocityOffsetA)-Point(*c.bodyB,c.velocityOffsetB),c.normal);
    const float lambda=c.normalMass*(c.restitutionBias-vn);
    const float next=std::max(c.normalImpulse+lambda,0.f),applied=next-c.normalImpulse;
    c.normalImpulse=next;
    if(applied!=0)Apply(c,c.normal*applied);
    Friction(c);
}
struct Inverse { Matrix4 matrix{}; bool valid=true; unsigned rank=0; };
Inverse Pseudoinverse(Matrix4 a,unsigned count,unsigned mask) {
    Inverse result;Matrix4 vectors{};for(unsigned i=0;i<count;++i)vectors[i][i]=1;
    std::array<unsigned,4> index{};unsigned n=0;
    for(unsigned i=0;i<count;++i)if(mask&(1u<<i))index[n++]=i;
    Matrix4 work{};for(unsigned i=0;i<n;++i)for(unsigned j=0;j<n;++j)work[i][j]=a[index[i]][index[j]];
    double scale=0;for(unsigned i=0;i<n;++i)scale=std::max(scale,std::abs(work[i][i]));
    for(unsigned iteration=0;iteration<64;++iteration){
        unsigned p=0,q=0;double largest=0;
        for(unsigned i=0;i<n;++i)for(unsigned j=i+1;j<n;++j)if(std::abs(work[i][j])>largest){largest=std::abs(work[i][j]);p=i;q=j;}
        if(largest<=32*std::numeric_limits<double>::epsilon()*scale)break;
        const double angle=.5*std::atan2(2*work[p][q],work[q][q]-work[p][p]);
        const double c=std::cos(angle),s=std::sin(angle),pp=work[p][p],qq=work[q][q],pq=work[p][q];
        for(unsigned k=0;k<n;++k)if(k!=p&&k!=q){
            const double kp=work[k][p],kq=work[k][q];
            work[k][p]=work[p][k]=c*kp-s*kq;work[k][q]=work[q][k]=s*kp+c*kq;
        }
        work[p][p]=c*c*pp-2*s*c*pq+s*s*qq;
        work[q][q]=s*s*pp+2*s*c*pq+c*c*qq;work[p][q]=work[q][p]=0;
        for(unsigned k=0;k<n;++k){const double kp=vectors[k][p],kq=vectors[k][q];vectors[k][p]=c*kp-s*kq;vectors[k][q]=s*kp+c*kq;}
    }
    const double tolerance=128*std::numeric_limits<double>::epsilon()*scale;
    for(unsigned eigen=0;eigen<n;++eigen){
        const double value=work[eigen][eigen];
        if(value< -tolerance){result.valid=false;return result;}
        if(!(value>tolerance))continue;
        ++result.rank;
        for(unsigned i=0;i<n;++i)for(unsigned j=0;j<n;++j)
            result.matrix[index[i]][index[j]]+=vectors[i][eigen]*vectors[j][eigen]/value;
    }
    return result;
}
struct Block {
    std::size_t first=0;unsigned count=0;Matrix4 matrix{};std::array<Inverse,16> inverses{};
};
struct Solver {
    std::vector<ContactConstraint>& rows;
    std::vector<Block> blocks;
    uint64_t accepted=0,fallback=0,singular=0,normalChecksFailed=0;
    explicit Solver(ContactSolver& solver):rows(const_cast<std::vector<ContactConstraint>&>(solver.Constraints())) {
        for(std::size_t first=0;first<rows.size();){
            unsigned n=1;
            while(n<4&&first+n<rows.size()&&rows[first+n].bodyA==rows[first].bodyA&&rows[first+n].bodyB==rows[first].bodyB&&
                  std::memcmp(&rows[first+n].normal,&rows[first].normal,sizeof(glm::vec3))==0)++n;
            Block block;block.first=first;block.count=n;
            for(unsigned i=0;i<n;++i)for(unsigned j=0;j<n;++j){
                const auto&a=rows[first+i];const auto&b=rows[first+j];
                const glm::dvec3 na(a.normal),nb(b.normal);
                const glm::dvec3 aa=glm::cross(glm::dvec3(a.velocityOffsetA),na),ab=glm::cross(glm::dvec3(a.velocityOffsetB),na);
                const glm::dvec3 ba=glm::cross(glm::dvec3(b.velocityOffsetA),nb),bb=glm::cross(glm::dvec3(b.velocityOffsetB),nb);
                block.matrix[i][j]=(double(a.bodyA->inverseMass)+a.bodyB->inverseMass)*glm::dot(na,nb)+
                    glm::dot(aa,glm::dmat3(a.inverseInertiaA)*ba)+glm::dot(ab,glm::dmat3(a.inverseInertiaB)*bb);
            }
            // Physical inertia is symmetric; pair roundoff is symmetrized in
            // this diagnostic matrix, never hidden by compliance/regularization.
            for(unsigned i=0;i<n;++i)for(unsigned j=i+1;j<n;++j){const double mean=.5*(block.matrix[i][j]+block.matrix[j][i]);block.matrix[i][j]=block.matrix[j][i]=mean;}
            for(unsigned mask=0;mask<(1u<<n);++mask)block.inverses[mask]=Pseudoinverse(block.matrix,n,mask);
            if(block.inverses[(1u<<n)-1].rank<n)++singular;
            blocks.push_back(block);first+=n;
        }
    }
    bool Normal(const Block& block) {
        Values4 old{},b{};double scale=0;
        for(unsigned i=0;i<block.count;++i){
            const auto&c=rows[block.first+i];old[i]=c.normalImpulse;
            b[i]=glm::dot(Point(*c.bodyA,c.velocityOffsetA)-Point(*c.bodyB,c.velocityOffsetB),c.normal)-c.restitutionBias;
            scale=std::max(scale,std::abs(b[i]));
        }
        for(unsigned i=0;i<block.count;++i)for(unsigned j=0;j<block.count;++j)b[i]-=block.matrix[i][j]*old[j];
        Values4 best{};double bestNorm=INFINITY;bool found=false;
        for(unsigned mask=0;mask<(1u<<block.count);++mask){
            const auto&inverse=block.inverses[mask];if(!inverse.valid)continue;
            Values4 trial{};double norm=0,magnitude=scale;
            for(unsigned i=0;i<block.count;++i){
                for(unsigned j=0;j<block.count;++j)trial[i]-=inverse.matrix[i][j]*b[j];
                norm+=trial[i]*trial[i];
            }
            for(unsigned i=0;i<block.count;++i)for(unsigned j=0;j<block.count;++j)
                magnitude=std::max(magnitude,std::abs(block.matrix[i][j]*trial[j]));
            // Backward-error allowance of the existing binary32 point-velocity
            // and impulse kernel, not a contact distance/velocity policy.
            const double tolerance=64*std::numeric_limits<float>::epsilon()*std::max(magnitude,1e-30);
            bool valid=true;
            for(unsigned i=0;i<block.count;++i){
                if(trial[i]<0||!std::isfinite(trial[i])){valid=false;break;}
                double residual=b[i];
                for(unsigned j=0;j<block.count;++j)residual+=block.matrix[i][j]*trial[j];
                valid&=trial[i]>0?std::abs(residual)<=tolerance:residual>= -tolerance;
            }
            if(valid&&norm<bestNorm){best=trial;bestNorm=norm;found=true;}
        }
        if(!found)return false;
        for(unsigned i=0;i<block.count;++i){auto&c=rows[block.first+i];const float next=float(best[i]),delta=next-c.normalImpulse;c.normalImpulse=next;if(delta!=0)Apply(c,c.normal*delta);}
        return true;
    }
    void Sweep(){
        for(const auto&block:blocks){
            if(block.count==1){Scalar(rows[block.first]);continue;}
            if(!Normal(block)){++fallback;for(unsigned i=0;i<block.count;++i)Scalar(rows[block.first+i]);continue;}
            ++accepted;for(unsigned i=0;i<block.count;++i)Friction(rows[block.first+i]);
        }
    }
};
}

int main(int argc,char**argv){
    if(argc!=3)throw std::runtime_error("usage: block-replay SNAPSHOT OUTPUT");
    Json input;std::ifstream(argv[1])>>input;
    System state(input);ContactSolver native;
    for(const auto&c:state.contacts)native.AddContact(*c.a,*c.b,c.geometry,c.friction,0);
    native.Prepare(0);
    const auto setupStart=std::chrono::steady_clock::now();block_trial::Solver solver(native);
    const double setupMs=std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-setupStart).count();
    Json output={{"schema","judas.offline.block-normal.v1"},{"scope","one original captured event; local blocknormal with native friction, no production adoption"},{"sequence",input.at("sequence")},{"bodies",state.bodies.size()},{"contacts",state.contacts.size()},{"blocks",solver.blocks.size()},{"singular_blocks",solver.singular},{"setup_ms",setupMs},{"observations",Json::array()}};
    double cumulative=0;unsigned complete=0;
    for(unsigned sweeps:{10u,20u,50u,100u,200u}){
        const auto start=std::chrono::steady_clock::now();for(;complete<sweeps;++complete)solver.Sweep();cumulative+=std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-start).count();
        output["observations"].push_back({{"sweeps",sweeps},{"solver_ms",cumulative},{"energy_J",Energy(state)},{"residual",Residual(native.Constraints())},{"block_updates",solver.accepted},{"fallback_updates",solver.fallback},{"velocity_digest",VelocityDigest(state)}});
    }
    std::ofstream(argv[2])<<output.dump(2)<<'\n';std::cout<<output.dump(2)<<'\n';
}
