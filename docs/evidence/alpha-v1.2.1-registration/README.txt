Alpha v1.2.1 qualification correction, 2 November 2026

First candidate: 08d188d705b030b8030ed454fccb0f8eec1705c1.
Original Experimental CI: https://github.com/SuperReaper1999/Project-Judas/actions/runs/41528162632
The native cases passed; prealpha.gate-registration failed two Alpha expectation
checks (single-config and Ninja Multi-Config). The oracle omitted the new
RuntimeWorld-owned fluid cadence fixture from its explicit unavailable set.
Production registration correctly derived that restriction from the existing
Alpha policy. The repair changes only this test expectation and adds a targeted
negative control: removing cadence's Alpha skip, or adding a skip in Experimental,
must be detected. No runtime policy, solver or physical assertion changes.

before.log: local unchanged reproduction, 9 discovered, one failure and two
Ninja-dependent skips (Ninja not installed on this host).
after.log: registration plus release-manifest, packaging and current-doc checks;
37 discovered, 35 passed, two Ninja-dependent skips. Hosted final qualification
must exercise Ninja and multi-config; these local skips are not passes.

Commands:
python3 -m unittest tests.test_validation_registration
python3 -m unittest tests.test_validation_registration tests.test_release_manifest tests.test_release_packaging tests.test_current_docs

The first Linux release preparation independently passed 92 native gates,
coordinated cold readers and SDK/moved-export operations. New final artifacts
must be rebuilt against the corrected candidate, not relabelled from that run.
