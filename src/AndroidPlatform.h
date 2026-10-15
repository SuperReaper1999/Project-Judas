#pragma once
#ifdef __ANDROID__
// Java callbacks publish only an atomic observation; audio devices remain owned
// and started/stopped on the ordinary engine thread. The final audio mix
// reads this flag without changing voices or accumulating delayed one-shots.
bool AndroidAudioFocused();
#endif
