#pragma once

struct ImGuiViewport;
struct TextureData;

// Called after the SDL and shared ImGui renderer bindings are initialized.
bool InstallVulkanEditorViewports();
bool ReadVulkanEditorViewport(const ImGuiViewport &viewport, TextureData &pixels);
