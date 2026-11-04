#include "AI.h"
#include "../../RuntimeWorld.h"
#include "../../ScriptAIReservations.h"
#include "../../ScriptAISensing.h"
#include "../../ScriptUtilityAI.h"
#include "../BindingContext.h"
#include "../BindingRegistry.h"
#include "../BindingValues.h"

using namespace ScriptBindingValues;
namespace ScriptBindings {
namespace {
class AIState final : public BindingModuleState {
  public:
    std::unique_ptr<ScriptUtilityAI> ai;
    std::unique_ptr<ScriptAISensing> senses;
    std::unique_ptr<ScriptAIReservations> reservations;
    ~AIState() override {
        ai.reset();
        reservations.reset();
        senses.reset();
    }
    void ReleaseClaims(uint64_t owner, uint64_t slot) override {
        if (reservations)
            reservations->ReleaseClaims(owner, slot);
    }
    std::string Retire(uint64_t owner, uint64_t slot) override {
        const auto error = ai ? ai->Retire(owner, slot) : std::string{};
        if (senses)
            senses->Retire(owner, slot);
        if (reservations)
            reservations->Retire(owner, slot);
        return error;
    }
    void ShiftOrigin() override {
        if (senses)
            senses->ShiftOrigin();
    }
};
JSValue HandleAI(BindingContext &context, const std::string &op, int argc, JSValueConst *argv) {
    auto *s = &context;
    auto *c = context.js;
    auto &world = *context.world;
    const bool create = op == "aiCreate" || op == "senseCreate" || op == "reservationCreate";
    if (s->stopping || s->retiringAI) {
        if (op == "reservationCreate" || op == "reservationAcquire")
            return JS_ThrowTypeError(c, "Cannot acquire AI targets while retiring a script");
        if (op == "senseCreate")
            return JS_ThrowTypeError(c, "Cannot create AI senses while retiring a script");
        if (op == "aiCreate")
            return JS_ThrowTypeError(c, "Cannot create AI while retiring a script");
    }
    if (!create && !s->states.Find("AI")) {
        const char *message = op.rfind("reservation", 0) == 0 ? "stale AI reservation scope"
                              : op.rfind("sense", 0) == 0     ? "stale AI sensor"
                                                              : "stale AI agent";
        return JS_ThrowReferenceError(c, "%s", message);
    }
    auto &state = s->states.GetOrCreate<AIState>("AI");
    if (op.rfind("reservation", 0) == 0) {
        if (!state.reservations) {
            if (op != "reservationCreate")
                return JS_ThrowReferenceError(c, "stale AI reservation scope");
            state.reservations = std::make_unique<ScriptAIReservations>();
        }
        return state.reservations->Dispatch(c, op, argc, argv, world, s->currentOwner,
                                            s->currentSlot);
    }
    if (op.rfind("sense", 0) == 0) {
        if (!state.senses) {
            if (op != "senseCreate")
                return JS_ThrowReferenceError(c, "stale AI sensor");
            state.senses = std::make_unique<ScriptAISensing>(c);
        }
        return state.senses->Dispatch(op, argc, argv, world, s->currentOwner, s->currentSlot);
    }
    if (op.rfind("ai", 0) == 0) {
        if (!state.ai) {
            if (op != "aiCreate")
                return JS_ThrowReferenceError(c, "stale AI agent");
            state.ai = std::make_unique<ScriptUtilityAI>(c);
        }
        return state.ai->Dispatch(op, argc, argv, s->currentOwner, s->currentSlot);
    }
    return JS_ThrowTypeError(c, "unknown AI operation");
}
} // namespace
std::vector<ScriptAIInspection> InspectAI(const BindingStates &states) {
    const auto *state = dynamic_cast<const AIState *>(states.Find("AI"));
    return state && state->ai ? state->ai->Inspect() : std::vector<ScriptAIInspection>{};
}
void RegisterAIBindings(BindingRegistry &registry) {
    registry.Add("AI",
                 {"aiCreate", "aiDispose", "aiDue", "aiFinish", "aiInspect", "aiRestore",
                  "aiSnapshot", "aiTick", "reservationAcquire", "reservationAvailable",
                  "reservationCreate", "reservationDispose", "reservationRelease",
                  "reservationReleaseAll", "senseCreate", "senseDispose", "senseSample"},
                 HandleAI);
}
} // namespace ScriptBindings
