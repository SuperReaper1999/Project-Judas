#include "ContactBlock.h"
#include <cstdio>
#include <random>

using namespace contact_block;

int main() {
    unsigned checks=0,failures=0;
    auto check=[&](bool value,const char* label){++checks;if(!value){++failures;std::printf("FAIL %s\n",label);}};
    auto feasible=[](const Matrix& w,const Vector& q,const Vector& x,unsigned n){
        for(unsigned i=0;i<n;++i){long double r=q[i],scale=std::abs(q[i]);
            if(!std::isfinite(x[i])||x[i]<0)return false;
            for(unsigned j=0;j<n;++j){r+=(long double)w[i][j]*x[j];scale+=std::abs((long double)w[i][j]*x[j]);}
            const auto e=1e-11L*std::max(scale,1e-30L);
            if(r < -e || (x[i]>0&&std::abs(r)>e))return false;
        }return true;
    };
    {
        // Four points on a square support one body. Normal response has three
        // independent modes (translation, pitch and roll), not four.
        Matrix w{};Vector q{-1,-1,-1,-1},old{},x{};
        const int rx[4]={-1,1,1,-1},rz[4]={-1,-1,1,1};
        for(unsigned i=0;i<4;++i)for(unsigned j=0;j<4;++j)w[i][j]=1+rx[i]*rx[j]+rz[i]*rz[j];
        NormalPatch patch;patch.Prepare(w,4);
        check(patch.Solve(q,old,x),"rank-three four-corner patch solves without stiffness");
        for(auto value:x)check(std::abs(value-.25)<1e-12,"balanced load remains balanced at four corners");
        Vector oldDistribution{.4,.1,.4,.1};
        check(patch.Solve(q,oldDistribution,x),"consistent null-space distribution retained");
        for(unsigned i=0;i<4;++i)check(std::abs(x[i]-oldDistribution[i])<1e-12,"prior null-space load is not arbitrarily replaced");
        q={1,1,1,1};check(patch.Solve(q,old,x)&&x==Vector{},"separating contact supplies no impulse");
    }
    std::mt19937 rng(0x77a10);
    const auto random=[&](){return (int(rng()%20001)-10000)/10000.;};
    unsigned refused=0;
    for(unsigned test=0;test<12000;++test){
        const unsigned n=1+rng()%4,rank=test%3==0?n:(n>1?n-1:1);
        Matrix basis{},w{};
        for(unsigned i=0;i<n;++i)for(unsigned k=0;k<rank;++k)basis[i][k]=random();
        for(unsigned i=0;i<n;++i)for(unsigned j=i;j<n;++j){
            double value=0;for(unsigned k=0;k<rank;++k)value+=basis[i][k]*basis[j][k];
            w[i][j]=w[j][i]=value;
        }
        Vector known{},slack{},q{},old{},x{};
        for(unsigned i=0;i<n;++i){
            if(rng()%2)known[i]=.01+std::abs(random());else slack[i]=.01+std::abs(random());
            old[i]=std::abs(random());
        }
        for(unsigned i=0;i<n;++i){q[i]=slack[i];for(unsigned j=0;j<n;++j)q[i]-=w[i][j]*known[j];}
        NormalPatch patch;patch.Prepare(w,n);
        const bool solved=patch.Solve(q,old,x);
        refused+=!solved;
        check(!solved||feasible(w,q,x,n),"accepted LCP satisfies independent long-double residual checks");
        // A refusal is allowed to select the legacy solver; well-conditioned
        // positive-definite cases must actually exercise the replacement.
        if(rank==n&&test%3==0)check(solved,"full-rank generated problem solved");
    }
    {
        NormalPatch patch;Vector q{-1,0,0,0},zero{},x{};Matrix w{};
        patch.Prepare(w,1);check(!patch.Solve(q,zero,x),"inconsistent zero-response problem rejected");
        w[0][0]=-1;patch.Prepare(w,1);check(!patch.Solve(q,zero,x),"negative effective mass rejected");
        w[0][0]=std::numeric_limits<double>::infinity();patch.Prepare(w,1);check(!patch.Solve(q,zero,x),"nonfinite response rejected");
        patch.Prepare(w,33);check(!patch.Solve(q,zero,x),"invalid patch size rejected without shifting overflow");
    }
    std::printf("contact block: %u checks, %u failures, %u conservative refusals\n",checks,failures,refused);
    return failures?1:0;
}
