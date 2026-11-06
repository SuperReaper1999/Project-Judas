#include "WaterPressureILU.h"
#include <cmath>
namespace judas::volume_grid {
bool WaterPressureILU::Build(const std::vector<std::map<std::size_t,CutCompensated>>& source,std::string& error){
    const auto n=source.size();WaterPressureILU next;next.scale_.resize(n);next.diagonal_.resize(n);next.rows_.resize(n);
    for(std::size_t i=0;i<n;++i){auto d=source[i].find(i);if(d==source[i].end()||!(d->second.Value()>0.&&std::isfinite(d->second.Value()))){error="liquid ILU lacks a positive diagonal";return false;}next.scale_[i]=std::sqrt(d->second.Value());}
    for(std::size_t i=0;i<n;++i){
        for(auto e:source[i]){if(e.first>=n||!std::isfinite(e.second.Value())){error="invalid liquid ILU entry";return false;}next.rows_[i][e.first]=(e.second/(CutCompensated(next.scale_[i])*CutCompensated(next.scale_[e.first]))).Value();}
        for(auto& entry:next.rows_[i]){
            auto j=entry.first;if(j>=i)break;
            entry.second/=next.diagonal_[j];
            for(auto k=next.rows_[j].upper_bound(j);k!=next.rows_[j].end();++k){auto slot=next.rows_[i].find(k->first);if(slot!=next.rows_[i].end())slot->second-=entry.second*k->second;}
        }
        double pivot=next.rows_[i].at(i);
        if(!(pivot>0.&&std::isfinite(pivot))){error="liquid ILU encountered a nonpositive pivot";return false;}
        next.diagonal_[i]=pivot;
    }
    *this=std::move(next);error.clear();return true;
}
bool WaterPressureILU::Apply(const std::vector<double>& rhs,std::vector<double>& output,std::string& error)const{
    const auto n=rows_.size();if(rhs.size()!=n){error="liquid ILU RHS mismatch";return false;}std::vector<double> x(n);
    for(std::size_t i=0;i<n;++i){x[i]=rhs[i]/scale_[i];for(auto e:rows_[i]){if(e.first>=i)break;x[i]-=e.second*x[e.first];}}
    for(std::size_t i=n;i-->0;){for(auto e=rows_[i].upper_bound(i);e!=rows_[i].end();++e)x[i]-=e->second*x[e->first];x[i]/=diagonal_[i];}
    for(std::size_t i=0;i<n;++i){x[i]/=scale_[i];if(!std::isfinite(x[i])){error="liquid ILU produced nonfinite correction";return false;}}
    output=std::move(x);error.clear();return true;
}
}
