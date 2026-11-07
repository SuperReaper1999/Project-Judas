#pragma once
#include "CutCondensedPressure.h"
#include <map>
namespace judas::volume_grid {
class WaterPressureILU {
  public:
    bool Build(const std::vector<std::map<std::size_t, CutCompensated>> &, std::string &);
    bool Build(const std::vector<std::map<std::size_t, CutCompensated>> &, std::string &,
               unsigned fillLevel);
    bool Apply(const std::vector<double> &, std::vector<double> &, std::string &) const;

  private:
    std::vector<double> scale_, diagonal_;
    std::vector<std::map<std::size_t, double>> rows_;
};
} // namespace judas::volume_grid
