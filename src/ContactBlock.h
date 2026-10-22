#pragma once

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <limits>

namespace contact_block {
using Vector = std::array<double, 4>;
using Matrix = std::array<Vector, 4>;
struct Diagnostics { std::uint64_t solves=0, fallbacks=0; };
inline thread_local Diagnostics diagnostics;

// A small, frictionless normal LCP. It owns no physical state. The same matrix
// serves all velocity sweeps at an immutable pose. Factorizations are lazy:
// the common full-contact active set needs only one preparation per patch.
class NormalPatch {
  public:
    void Prepare(const Matrix& response, unsigned points) {
        matrix = response;
        count = points;
        preferred = points <= 4 ? (1u << points) - 1 : 0;
        full = {}; recent = {};
        recentMask = std::numeric_limits<unsigned>::max();
    }

    bool Solve(const Vector& q, const Vector& previous, Vector& impulses) {
        if (!count || count > 4)
            return false;
        if (!Build((1u << count) - 1).valid)
            return false;
        // No new residual and no accumulated load: every valid active set
        // produces the same zero impulse. Keep the preferred set unchanged,
        // including its null-space policy. Do not skip the PSD validation or
        // discard a nonzero prior distribution, even when q itself is zero.
        bool zero = true;
        for (unsigned i = 0; i < count; ++i)
            zero = zero && q[i] == 0 && previous[i] == 0;
        if (zero) {
            impulses = {};
            return true;
        }
        if (Try(preferred, q, previous, impulses))
            return true;
        const unsigned all = (1u << count) - 1;
        for (unsigned mask = all;; --mask) {
            if (mask != preferred && Try(mask, q, previous, impulses)) {
                preferred = mask;
                return true;
            }
            if (!mask)
                break;
        }
        return false;
    }

  private:
    struct Factor {
        Matrix vectors{};
        Vector values{};
        std::array<unsigned,4> indices{};
        unsigned count = 0;
        double rankTolerance = 0;
        bool built = false, valid = false;
    };
    Matrix matrix{};
    unsigned count = 0, preferred = 0;
    Factor full{}, recent{};
    unsigned recentMask=std::numeric_limits<unsigned>::max();

    Factor& Build(unsigned mask) {
        const bool entire=mask==((1u<<count)-1);
        if(!entire&&recentMask!=mask){recent={};recentMask=mask;}
        auto& factor = entire?full:recent;
        if (factor.built)
            return factor;
        factor.built = true;
        for (unsigned i=0;i<count;++i)
            if (mask & (1u<<i)) factor.indices[factor.count++] = i;
        Matrix a{};
        double norm = 0;
        for (unsigned i=0;i<factor.count;++i) {
            factor.vectors[i][i] = 1;
            for (unsigned j=0;j<factor.count;++j) {
                const double value = matrix[factor.indices[i]][factor.indices[j]];
                if (!std::isfinite(value) ||
                    value != matrix[factor.indices[j]][factor.indices[i]])
                    return factor;
                a[i][j] = value;
                norm = std::max(norm,std::abs(value));
            }
        }
        const double tolerance = 32 * std::numeric_limits<double>::epsilon() * norm;
        bool converged = factor.count < 2;
        // Largest-off-diagonal Jacobi rotations, deterministic at equal values.
        // Exhaustion rejects this factor; it never changes physical budgets.
        for (unsigned rotation=0;rotation<192 && !converged;++rotation) {
            unsigned p=0,q=0; double largest=0;
            for(unsigned i=0;i<factor.count;++i)
                for(unsigned j=i+1;j<factor.count;++j)
                    if(std::abs(a[i][j])>largest){largest=std::abs(a[i][j]);p=i;q=j;}
            if(largest<=tolerance){converged=true;break;}
            const double tau=(a[q][q]-a[p][p])/(2*a[p][q]);
            const double t=std::isfinite(tau)
                ? std::copysign(1.0,tau)/(std::abs(tau)+std::hypot(1.0,tau))
                : a[p][q]/(a[q][q]-a[p][p]);
            const double c=1/std::sqrt(1+t*t),s=t*c,off=a[p][q];
            a[p][p]-=t*off;a[q][q]+=t*off;a[p][q]=a[q][p]=0;
            for(unsigned k=0;k<factor.count;++k){
                if(k!=p&&k!=q){const double x=a[k][p],y=a[k][q];
                    a[k][p]=a[p][k]=c*x-s*y;a[k][q]=a[q][k]=s*x+c*y;}
                const double x=factor.vectors[k][p],y=factor.vectors[k][q];
                factor.vectors[k][p]=c*x-s*y;factor.vectors[k][q]=s*x+c*y;
            }
        }
        if(!converged)
            return factor;
        factor.rankTolerance=128*std::numeric_limits<double>::epsilon()*norm;
        for(unsigned i=0;i<factor.count;++i){
            factor.values[i]=a[i][i];
            if(!std::isfinite(a[i][i])||a[i][i]<-factor.rankTolerance)
                return factor;
        }
        factor.valid=true;
        return factor;
    }

    bool Try(unsigned mask,const Vector& q,const Vector& previous,Vector& result) {
        auto& factor=Build(mask);
        if(!factor.valid)
            return false;
        Vector candidate{};
        for(unsigned j=0;j<factor.count;++j){
            double component=0;
            if(factor.values[j]>factor.rankTolerance){
                for(unsigned i=0;i<factor.count;++i)
                    component-=factor.vectors[i][j]*q[factor.indices[i]];
                component/=factor.values[j];
            }else{
                // Retain the prior distribution in the null space. No diagonal
                // stiffness, point removal or invented support force is added.
                for(unsigned i=0;i<factor.count;++i)
                    component+=factor.vectors[i][j]*previous[factor.indices[i]];
            }
            for(unsigned i=0;i<factor.count;++i)
                candidate[factor.indices[i]]+=factor.vectors[i][j]*component;
        }
        for(unsigned i=0;i<count;++i){
            if(!std::isfinite(candidate[i])||candidate[i]<0||!std::isfinite(q[i]))
                return false;
            double residual=q[i],magnitude=std::abs(q[i]);
            for(unsigned j=0;j<count;++j){const double term=matrix[i][j]*candidate[j];
                residual+=term;magnitude+=std::abs(term);}
            const double roundoff=128*std::numeric_limits<double>::epsilon()*magnitude;
            if(!std::isfinite(residual)||
               ((mask&(1u<<i))?std::abs(residual)>roundoff:residual < -roundoff))
                return false;
        }
        result=candidate;
        return true;
    }
};
} // namespace contact_block
