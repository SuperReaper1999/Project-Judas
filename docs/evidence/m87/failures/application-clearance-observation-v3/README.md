# Physical settling observed too early

This preserved run passed restored physical movement after the shelf clearance
repair, with **0.186691284 m** actual excursion during the same 40-frame window.
It then failed two newly authored observations (50 checks / 2 failures): the test
assumed all spring mechanisms had slept seven seconds after removing power, then
used an assembly that was still settling as the purported sleeping negative control.

The corrected follow-up retained native physical thresholds, authored spring and
damping values, motor force/speed, timestep and the actual 96-body sleep condition.
It waited for measured quiescence with a finite 15-second fixture watchdog. Actual
sleep arrived after **7.816667 seconds**, not the assumed seven seconds. Maximum
articulated linear speed was 0.179247 m/s at one second, 0.068546 m/s at four seconds,
0.010834 m/s at seven seconds, then zero when the ordinary quiet-time contract
completed. The intervening oscillation is physical spring behaviour; no settling
delay or forced sleep was inserted into production.

The independent negative control is now chosen from bodies observed actually
sleeping before the impulse. The final identical-content application run passes
**51 checks / 0 failures**. Preserved raw failure source/log/receipt here distinguish
this fixture observation correction from an engine change.
