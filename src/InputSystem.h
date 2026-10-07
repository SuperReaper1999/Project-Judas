#pragma once
#include <map>
#include <array>
#include <chrono>
#include <cstdint>
#include <deque>
#include <string>
#include <vector>

// Portable control tokens (key:W, mouse:Left, pad:South, stick:LeftX, ...).
// SDL identifiers belong exclusively to Window's raw-device adapter.
struct InputBinding {
    std::string control;
    float scale=1, deadzone=0;
    // Paired controls use scale for X and scaleY for Y, after processing.
    float scaleY=1;
    bool circular=false;
};
struct InputEntry {
    std::string name;
    bool axis=false;
    std::vector<InputBinding> bindings;
    bool vector=false;
};
struct InputMap {
    std::vector<InputEntry> entries;
    static InputMap Defaults();
    bool Validate(std::string& error) const;
    std::string Serialize() const;
    static bool Parse(const std::string& text,InputMap& out,std::string& error);
    InputEntry* Find(const std::string& name);
    const InputEntry* Find(const std::string& name) const;
    bool Add(const std::string& name,bool axis);
    bool AddVector(const std::string& name);
    bool Rename(const std::string& from,const std::string& to);
    bool Remove(const std::string& name);
    bool AddBinding(const std::string& name,InputBinding binding);
    bool ReplaceBinding(const std::string& name,std::size_t index,InputBinding binding);
    bool RemoveBinding(const std::string& name,std::size_t index);
};
struct InputActionState { bool held=false,pressed=false,released=false; };
struct InputVector { float x=0,y=0; };
struct InputStickSample { std::uint64_t sequence=0;double time=0;float x=0,y=0; };
struct InputStickSnapshot {
    std::vector<InputStickSample> samples;
    std::uint64_t sequence=0;
    bool reset=false,overflow=false;
    std::size_t capacity=128;
};
class InputSystem {
public:
    InputSystem();
    bool SetMap(const InputMap& map,std::string& error);
    const InputMap& Map() const {return m_map;}
    void BeginFrame();
    void SetPhysical(const std::string& control,float value);
    void AddDelta(const std::string& control,float delta);
    void ClearDevice(const std::string& prefix);
    void Reset();
    void DiscardPending();
    void DiscardStickHistory();
    // Consume logical inputs and every action/axis sharing their physical bindings.
    // Suppression persists through catch-up fixed steps until BeginFrame.
    void ConsumeBindings(const std::vector<std::string>& names);
    InputActionState Action(const std::string& name) const;
    float Axis(const std::string& name) const;
    // Latest backend pair and net movement during this render/input frame.
    // These are copied values; fixed-step readers never consume observations.
    InputVector Stick(const std::string& side) const;
    InputVector StickDelta(const std::string& side) const;
    InputVector Vector(const std::string& name) const;
    InputStickSnapshot StickSamples(const std::string& side,std::uint64_t after=0) const;
    // Window's selected-controller adapter submits paired polls atomically.
    // A baseline restores held input without inventing motion on reassignment.
    void SetStick(const std::string& side,float x,float y,bool baseline=false);
    static InputVector CircularDeadzone(InputVector value,float deadzone);
    // All consumers see the same snapshot. Edges accumulate across zero-step
    // frames; first fixed step drains them, later catch-up steps see no repeat.
    void BeginFixedStep() const;
    InputActionState FixedAction(const std::string& name) const;
    static float Deadzone(float value,float deadzone);
private:
    void Evaluate();
    bool StickConsumed(const std::string& side) const;
    std::vector<std::string> m_consumed;
    InputMap m_map;
    std::map<std::string,float> m_raw,m_axes;
    std::map<std::string,InputVector> m_vectors;
    std::map<std::string,InputActionState> m_actions;
    mutable std::map<std::string,InputActionState> m_pending,m_fixed;
    std::array<InputVector,2> m_stickFrameStart{};
    std::array<bool,2> m_stickReady{};
    std::array<std::deque<InputStickSample>,2> m_stickHistory;
    std::array<std::uint64_t,2> m_stickDroppedThrough{};
    std::uint64_t m_stickSequence=0,m_stickResetSequence=0;
    std::chrono::steady_clock::time_point m_inputClock=std::chrono::steady_clock::now();
};
