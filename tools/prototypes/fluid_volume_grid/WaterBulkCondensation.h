#pragma once
#include "WaterPressureILU.h"
#include <cstdint>
#include <set>
namespace judas::volume_grid {
// Exact scalar elimination of selected gas-storage rows, followed by a bounded
// incomplete factor on the remaining accelerator. All physical rows still take
// part in the external J*x residual and final material/energy admission checks.
class WaterBulkCondensation {
  public:
    bool Build(const std::vector<std::map<std::size_t, CutCompensated>> &,
               const std::vector<bool> &selected, std::string &);
    bool Apply(const std::vector<double> &, std::vector<double> &, std::string &) const;
    std::size_t Eliminated() const {
        return steps_.size();
    }

  private:
    struct Step {
        std::size_t node;
        CutCompensated diagonal;
        std::vector<std::pair<std::size_t, CutCompensated>> row, multipliers;
    };
    std::size_t size_ = 0;
    std::vector<Step> steps_;
    std::vector<std::size_t> remaining_;
    WaterPressureILU reduced_;
};
} // namespace judas::volume_grid
