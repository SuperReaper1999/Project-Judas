#include "RuntimeUI.h"
#include "InputSystem.h"
#include "Renderer.h"
#include "ResourceManager.h"

#include <algorithm>
#include <limits>

namespace {
const UIElement* DragElement(const UIDocument& document, const std::string& id) {
    const auto it = std::find_if(document.elements.begin(), document.elements.end(),
                                 [&](const auto& element) { return element.id == id; });
    return it == document.elements.end() ? nullptr : &*it;
}
bool Interactive(const UIElement& e) {
    return !e.dragType.empty() || !e.dropType.empty() || int(e.kind) >= int(UIKind::Button);
}
bool OnScreen(const UILayout* layout) {
    return layout&&layout->visible&&layout->enabled&&
           glm::all(glm::greaterThan(glm::min(layout->rect.position+layout->rect.size,layout->clip.position+layout->clip.size),
                                    glm::max(layout->rect.position,layout->clip.position)));
}
}

std::string RuntimeUI::Name(std::uint32_t handle)const {
    const auto it=m_documents.find(handle);
    return it==m_documents.end()?std::string{}:it->second.name;
}

bool RuntimeUI::DragAvailable(const UIDragState& drag) const {
    const auto source = m_documents.find(drag.source);
    if (source == m_documents.end()) return false;
    if(source->second.player && Paused())return false;
    const auto* element = DragElement(source->second.doc, drag.element);
    const auto* layout = LayoutOf(drag.source, drag.element);
    return element && element->dragType == drag.type && !drag.type.empty() &&
           OnScreen(layout);
}

void RuntimeUI::DragEvent(const UIDragState& drag, const char* type, bool target) {
    const auto source = m_documents.find(drag.source);
    if (source == m_documents.end()) return;
    const auto destination = m_documents.find(drag.target);
    UIEvent event;
    event.sourceDocument = source->second.name;
    event.sourceElement = drag.element;
    event.dragType = drag.type;
    event.position = drag.position;
    event.player = source->second.player;
    if (destination != m_documents.end()) {
        event.targetDocument = destination->second.name;
        event.targetElement = drag.targetElement;
    }
    event.document = target ? event.targetDocument : event.sourceDocument;
    event.element = target ? event.targetElement : event.sourceElement;
    event.type = type;
    m_events.push_back(std::move(event));
}

bool RuntimeUI::BeginDrag(std::uint32_t handle, const std::string& id) {
    auto document = m_documents.find(handle);
    if (document == m_documents.end()) return false;
    if (m_layoutWidth) Layout(m_layoutWidth, m_layoutHeight);
    UIDragState state;
    state.source = handle;
    state.element = id;
    const auto* e = DragElement(document->second.doc, id);
    if (!e) return false;
    state.type = e->dragType;
    state.keyboard = true;
    state.revision = document->second.inputRevision;
    if (!DragAvailable(state)) return false;
    const auto* layout = LayoutOf(handle, id);
    state.position = layout->rect.position + layout->rect.size * .5f;
    EndDrag(document->second.player, false);
    m_drags.emplace(document->second.player, state);
    DragEvent(state, "dragStart");
    return true;
}

void RuntimeUI::EndDrag(std::uint64_t player, bool drop) {
    const auto found = m_drags.find(player);
    if (found == m_drags.end()) return;
    auto state = found->second;
    m_drags.erase(found); // Events cannot observe a still-active finished transaction.
    const auto destination = m_documents.find(state.target);
    const auto* e = destination == m_documents.end() ? nullptr : DragElement(destination->second.doc, state.targetElement);
    const auto* layout = LayoutOf(state.target, state.targetElement);
    const bool accepted = drop && DragAvailable(state) && e && OnScreen(layout) && e->dropType == state.type;
    if (accepted) DragEvent(state, "drop", true);
    else { state.target = 0; state.targetElement.clear(); }
    DragEvent(state, accepted ? "dragEnd" : "dragCancel");
}

void RuntimeUI::CancelDrag(std::uint32_t handle) {
    const auto document = m_documents.find(handle);
    if (document == m_documents.end()) return;
    const auto it = m_drags.find(document->second.player);
    if (it != m_drags.end() && it->second.source == handle) EndDrag(document->second.player, false);
}

std::optional<UIDragState> RuntimeUI::Drag(std::uint32_t handle) const {
    const auto document = m_documents.find(handle);
    if (document == m_documents.end()) return {};
    const auto it = m_drags.find(document->second.player);
    return it == m_drags.end() ? std::optional<UIDragState>{} : it->second;
}

void RuntimeUI::UpdateDrag(InputSystem& input, glm::vec2 pointer, bool available,
                           int, int, std::uint64_t player) {
    const auto it = m_drags.find(player);
    if (it == m_drags.end()) return;
    auto& state = it->second;
    if (state.revision == std::numeric_limits<std::uint64_t>::max()) state.revision = input.ResetRevision();
    if (!DragAvailable(state) || state.revision != input.ResetRevision() ||
        (!state.keyboard && !available) || input.Action("pause").pressed) {
        EndDrag(player, false);
        input.ConsumeBindings({"pause","ui_activate"});
        return;
    }
    std::uint32_t modal=0;
    for(const auto& doc:m_documents)if(doc.second.player==player&&doc.second.doc.visible&&doc.second.doc.enabled&&doc.second.doc.modal)modal=doc.first;
    if(modal&&modal!=state.source){EndDrag(player,false);return;}
    const auto previousTarget = state.target;
    const auto previousElement = state.targetElement;
    state.target = 0;
    state.targetElement.clear();
    if (state.keyboard) {
        // Same document's normal focus/navigation is the controller destination.
        auto& source = m_documents.at(state.source);
        const auto* e = DragElement(source.doc, source.focus);
        const auto* l = LayoutOf(state.source, source.focus);
        if (e && OnScreen(l)) {
            state.position = l->rect.position + l->rect.size * .5f;
            if (e->dropType == state.type) { state.target = state.source; state.targetElement = e->id; }
        }
    } else {
        state.position = pointer;
        // Front-to-back hit testing respects clipping, player boundaries and
        // modal occlusion. A foreground control blocks dropping through it.
        for (auto doc = m_documents.rbegin(); doc != m_documents.rend(); ++doc) {
            const auto& in = doc->second;
            if (in.player != player || !in.doc.visible || !in.doc.enabled || (modal&&doc->first!=modal)) continue;
            const UIElement* hit = nullptr;
            for (const auto& e : in.doc.elements) {
                const auto& l = in.layout.at(e.id);
                if (l.visible && Interactive(e) && l.rect.Contains(pointer) && l.clip.Contains(pointer)) hit = &e;
            }
            if (hit) {
                if (in.layout.at(hit->id).enabled && hit->dropType == state.type) { state.target = doc->first; state.targetElement = hit->id; }
                break;
            }
            if (in.doc.modal) break;
        }
    }
    if (state.target && (previousTarget != state.target || previousElement != state.targetElement))
        DragEvent(state, "dragOver", true);
    if (!state.keyboard) {
        const auto click = input.Action("ui_click");
        if (click.released) EndDrag(player, true);
        else if (!click.held) EndDrag(player, false);
    }
}

bool RuntimeUI::SetPreviewCamera(std::uint32_t handle, const std::string& id, std::uint64_t camera) {
    auto document = m_documents.find(handle);
    if (document == m_documents.end()) return false;
    const auto* element = DragElement(document->second.doc, id);
    if (!element || element->kind != UIKind::Image) return false;
    if (camera) document->second.previews[id] = camera;
    else document->second.previews.erase(id);
    return true;
}
std::uint64_t RuntimeUI::PreviewCamera(std::uint32_t handle, const std::string& id) const {
    const auto document = m_documents.find(handle);
    if (document == m_documents.end()) return 0;
    const auto found = document->second.previews.find(id);
    return found == document->second.previews.end() ? 0 : found->second;
}

void RuntimeUI::DrawDrags(Renderer& renderer) {
    for (const auto& entry : m_drags) {
        const auto& drag = entry.second;
        if (!DragAvailable(drag)) continue;
        auto& in = m_documents.at(drag.source);
        const auto* e = DragElement(in.doc, drag.element);
        const glm::vec2 position = drag.position + glm::vec2(14, 14);
        renderer.SetUIClip({in.viewport.x*m_layoutWidth, in.viewport.y*m_layoutHeight},
                           {in.viewport.z*m_layoutWidth, in.viewport.w*m_layoutHeight});
        renderer.DrawUIRect(position, {80,64}, {.06f,.055f,.04f,.93f});
        renderer.DrawUIRect(position, {80,2}, e->focusColor);
        if (m_resources && !e->texture.empty()) {
            if(std::find(in.refs.begin(),in.refs.end(),e->texture)==in.refs.end()){m_resources->AddRef(e->texture);in.refs.push_back(e->texture);}
            m_resources->RequestTexture(e->texture);
            const auto texture = m_resources->TryGetTexture(e->texture);
            if (texture.IsValid()) renderer.DrawUIImage(position, {80,64}, texture, {1,1,1,.85f}, true);
        }
        if (!e->text.empty()) renderer.DrawUIText(e->text, position+glm::vec2(5,20), .65f, e->color);
    }
}
