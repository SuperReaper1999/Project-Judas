# Preserved M97 cold-save failure

The original `candidate-validation` write/read pair failed at the unchanged exact
joint continuation context check. Bounded tracing compared the stored and
reconstructed byte strings: first difference offset 48 of 281 bytes. Only offsets
48–56 differed. These are the world-frame B quaternion, after scene capture had
converted it through the moving beam owner's inverse pose.

Saved frame B:
`(1, -4.688025607565093e-16, -2.657145498823068e-17, 1.147079237151516e-10)`

Reconstructed frame B:
`(1, -4.440814806744189e-16, -2.8261164949633575e-17, 1.1470790983736379e-10)`

Anchors, other joint parameters and the entire body context matched. The physical
setting is not exactly recoverable from a float inverse/forward frame round trip.
The correction gives new ordinary-joint saves an articulation v5 record containing
exact native physical settings and mapped body identities before restoring existing
warm history. Older v1–v4 readers/contracts remain; the strict context equality is
not relaxed. No missing historical numerical state is invented. Temporary tracing
was removed from final engine source. Saved/current context bytes and original
failure logs are retained beside this note.
