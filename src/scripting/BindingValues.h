#pragma once
#include "../CameraProjection.h"
#include "../Scene.h"
#include "../ScriptBindingValues.h"
#include <initializer_list>

namespace ScriptBindingValues {
JSValue Vec(JSContext *, glm::vec3);
bool KnownOptions(JSContext *, JSValueConst, std::initializer_list<const char *>);
bool ReadCameraLens(JSContext *, JSValueConst, CameraLens &);
JSValue CameraLensValue(JSContext *, const CameraLens &);
bool ReadSpriteValues(JSContext *, JSValueConst, SpriteSettings &);
JSValue SpriteValue(JSContext *, const SpriteSettings &);
bool ReadPlanarValues(JSContext *, JSValueConst, std::shared_ptr<const PlanarCollider> &);
JSValue PlanarValue(JSContext *, const PlanarCollider &);
} // namespace ScriptBindingValues
