#pragma once
struct EditorContext;
class EditorCameraGestureState;
// Translate UI key edges into ordinary editor requests. No world tick, direct
// runtime mutation or subsystem-specific panel dispatch occurs here.
void RouteEditorShortcuts(EditorContext &, const EditorCameraGestureState &);
