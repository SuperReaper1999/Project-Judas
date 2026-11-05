"""Independent straight-tube displacement oracle, not a fluid implementation.

A one-metre, one-square-metre initially gas-filled tube receives water at
2 m^3/s. All material moves at 2 m/s. The outlet phase must change at t=.5 s.
A frozen gas outlet can satisfy the algebraic EOS with negative gas volume;
that is precisely why closure alone is not a physical acceptance condition.
"""
from fractions import Fraction as F
import json

rows = []
for dt in (F(1, 4), F(1, 2), F(1), F(2)):
    transferred = 2 * dt
    actual_water = min(F(1), transferred)
    outlet_water = max(F(0), transferred - 1)
    outlet_gas = min(F(1), transferred)
    frozen_water = transferred
    frozen_mass = F(6, 5) * (1 - transferred)
    frozen_residual = (1 - frozen_water) - frozen_mass / F(6, 5)
    assert actual_water + outlet_water == transferred
    assert (1 - actual_water) + outlet_gas == 1
    assert frozen_residual == 0
    rows.append({
        "dt": str(dt), "incomingWater": str(transferred),
        "analyticEndWater": str(actual_water),
        "analyticOutletWater": str(outlet_water),
        "analyticOutletGas": str(outlet_gas),
        "frozenOutletEndWater": str(frozen_water),
        "frozenOutletEndGasMass": str(frozen_mass),
        "frozenEOSResidual": str(frozen_residual),
        "frozenStatePhysical": 0 <= frozen_water <= 1 and frozen_mass >= 0,
    })
print(json.dumps({"scope": "analytic oracle only; no engine acceptance", "cases": rows}, indent=2))
