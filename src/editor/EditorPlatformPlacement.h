#pragma once

struct ImGuiViewport;

// SDL/XWayland compositors may constrain native placement. Keep the editor's
// hit geometry in the same frame as the admitted OS window, without changing
// docking, focus, input events or the portable in-window path.
void InstallEditorPlatformPlacement();
void ReconcileEditorPlatformPlacement(ImGuiViewport* viewport);
