# M98/M99 operator acceptance — 2026-10-17

Accepted implementation: `696f480bc57250f9bf6f9846e32e20dfc28f5363`.
M98 implementation ancestor: `84121564230242a404785af4d60a1475cdb83f4c`, previously
promoted provisionally for this combined review.

The operator confirmed: “perfect confirmed working m98 and m99 push to main <3”.
This authorizes promotion of that exact candidate. The accompanying checkpoint
only records acceptance and updates current documentation; engine, game scripts,
assets, tests, build tools and runtime bindings remain byte-identical to the
human-tested candidate. No validation reruns were needed for this record.

## Physical phone and connection follow-up

- Installed the exact candidate APK on the connected SM-S948B phone. SHA-256:
  `fe8a4ac429987409fa7066529b948e503b8ee824c726e2a8c249e12a515bff14`.
- The initial phone connection showed socket error 111 with a loopback address;
  the PC process was running but its previously verified host listener had closed.
  The reason the listener closed was not established. Source review did not find
  a listener shutdown caused by ordinary remote failure or focus loss.
- Restarted only the launched M99 PC host. Set the reversible ADB route
  `reverse tcp:27999 tcp:27999`. The phone's loopback endpoint then reached Linux
  through its USB cable. No firewall or engine change was made.
- Verified a real established TCP connection to the Linux host and the phone HUD
  reporting `connected / 2 connected peers / local 1`. The operator subsequently
  confirmed working gameplay. [Before](human/phone-before-usb.png) /
  [connected phone](human/phone-connected-usb.png).

This is physical Android-to-Linux **USB-forwarded networking** evidence. Direct
Wi-Fi/LAN reachability, public Internet security/deployment, Windows M99 and
physical controllers remain unvalidated. Earlier emulator and Linux automation
remain scoped as recorded; acceptance does not certify unsupported replication
or live save/scene combinations.

## Release provenance

`main` is fast-forwarded through the preserved accepted implementation commit and
this documentation-only acceptance record. Development stays on `Experimental`.
The 66 recorded candidate source fingerprints were checked against the exact
accepted commit before promotion. Original automated reports and fingerprint files
retain their historical pending-review status; current documentation status
updates and this record have separate `ACCEPTANCE_SHA256.txt` fingerprints.

No protected history, independent fluid research or implementation was changed.
No tag, M100 or Pre-Alpha stabilization work is included.
