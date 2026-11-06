#pragma once
#include "CutCondensedPressure.h"
#include <map>
namespace judas::volume_grid {
struct CutDirectedEdge {std::size_t from,to;CutCompensated amount;};
class CutDirectedTransport {
public:
 bool Build(const std::vector<double>& capacity,const std::vector<CutDirectedEdge>&,
            const std::vector<CutCompensated>& exteriorOut,std::string&);
 bool Solve(const std::vector<CutCompensated>& rhs,double absoluteBudget,
            std::vector<CutCompensated>&,std::string&)const;
 bool SolveFraction(const std::vector<CutCompensated>& rhs,double errorBudget,
                    std::vector<CutCompensated>&,std::string&);
private:
 friend class CutFastDirectedTransport; // Shares the unchanged reference factorization and original operator.
 bool Prepare(const std::vector<double>&,const std::vector<CutDirectedEdge>&,const std::vector<CutCompensated>&,std::string&);
 bool SolveInternal(const std::vector<CutCompensated>&,double,std::vector<CutCompensated>&,std::string&,const std::vector<CutCompensated>* barrier=nullptr)const;
 struct Step {std::size_t node;CutCompensated diagonal;std::vector<std::pair<std::size_t,CutCompensated>> in,out;};
 std::vector<CutCompensated> barrierCache_;
 std::vector<double> capacity_;
 std::vector<CutDirectedEdge> edges_;
 std::vector<CutCompensated> exterior_,ground_;
 std::vector<std::map<std::size_t,CutCompensated>> in_,out_;
 std::vector<Step> steps_;
 std::vector<bool> removed_;
};
}
