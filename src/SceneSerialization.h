#pragma once

#include <string>

class Scene;

// Milestone 28: the Judas scene file format (`.judas`).
//
// A plain, line-oriented UTF-8 text format written so that saving the same
// Scene twice produces byte-identical output (objects in scene order, keys
// in a fixed order, floats printed with enough digits to round-trip
// exactly) — diff-friendly under version control and readable without a
// tool. See docs/ARCHITECTURE.md, "Milestone 28, Scene file format," for
// the full grammar. The essentials:
//
//   JudasScene 1                     format identifier + version, first line
//   settings ... end                 scene-wide authored settings
//   object <id> "<name>" ... end     one block per object, in scene order
//
// Inside a block each line is `key value...`; a component is introduced by
// its header key (`render sphere`, `body dynamic box`, `gravity radial
// 9.81`, ...) and its fields follow as `component.field value` lines.
//
// Loading is strict: an unknown version, unknown key, missing required
// field, malformed number, non-finite number, duplicate id, or component
// whose prerequisites are absent fails with a message naming the line —
// the Scene is left untouched rather than partially filled. Nothing is
// defaulted silently: every field the writer emits, the reader requires.
bool SaveSceneToString(const Scene& scene, std::string& outText);
bool SaveSceneToFile(const Scene& scene, const std::string& path, std::string& outError);

bool LoadSceneFromString(const std::string& text, Scene& outScene, std::string& outError);
bool LoadSceneFromFile(const std::string& path, Scene& outScene, std::string& outError);

constexpr int kSceneFormatVersion = 1;
