#pragma once
#include <glm/glm.hpp>
#include <cstdint>
#include <string>
#include <vector>

// An assembly of scalar rotational degrees of freedom. Named indices belong
// only to this component; external terminals use ordinary entity identities.
// The runtime supplies the real effective inverse mass, including bearing
// reactions. This core neither owns rigid bodies nor polls driver input.
enum class PowertrainShaftKind { Internal, Wheel, Body };
enum class PowertrainSourceKind { Combustion, Electric, Custom };
enum class PowertrainLinkKind { Gear, Clutch, Differential };
struct PowertrainShaft {
    std::string name;
    PowertrainShaftKind kind=PowertrainShaftKind::Internal;
    double inertia=.2, initialAngularVelocity=0, viscousDrag=0;
    uint64_t target=0, bearing=0;
    glm::vec3 axis{1,0,0}; // internal/body: bearing-local, or world if no bearing
};
struct PowertrainTorquePoint { double rpm=0, torque=0; };
struct PowertrainSource {
    std::string name;
    unsigned shaft=0;
    PowertrainSourceKind kind=PowertrainSourceKind::Custom;
    std::vector<PowertrainTorquePoint> torqueCurve;
    double maxTorque=200, maxPower=100000, starterTorque=0;
};
// Optional physical continuity, expressed through existing material attachment
// identities. Commands remain driver policy; a released/cut physical mount
// cannot be reconnected merely by enabling its transmission link in JS.
struct PowertrainConnection {
    uint64_t owner=0;
    std::string attachment,rootAttachment;
    bool groups=false; // Explicit material group connectivity, without an attachment constraint.
};
struct PowertrainLink {
    std::string name;
    PowertrainLinkKind kind=PowertrainLinkKind::Gear;
    unsigned input=0, output=1, outputB=2;
    double ratio=1, maxTorque=200;
    PowertrainConnection connection;
    // Gear/clutch: output speed = ratio * input speed. Differential:
    // (left + right)/2 = ratio * input. maxTorque is OUTPUT torque capacity
    // for a clutch; an ideal gear/differential does not have a hidden limiter.
};
struct PowertrainBrake { std::string name; unsigned shaft=0; double maxTorque=200; };
struct PowertrainSettings {
    bool enabled=true;
    std::vector<PowertrainShaft> shafts;
    std::vector<PowertrainSource> sources;
    std::vector<PowertrainLink> links;
    std::vector<PowertrainBrake> brakes;
};
struct PowertrainSourceCommand { double input=0, customTorque=0, starter=0; };
struct PowertrainLinkCommand { bool enabled=true, overrideRatio=false; double engagement=1, ratio=1; };
struct PowertrainBrakeCommand { double engagement=0; };
struct PowertrainCommand {
    std::vector<PowertrainSourceCommand> sources;
    std::vector<PowertrainLinkCommand> links;
    std::vector<PowertrainBrakeCommand> brakes;
};
struct PowertrainShaftState { double angularVelocity=0, angle=0, torque=0; bool available=false; };
struct PowertrainLinkState { double impulse=0, torque=0, slip=0; bool active=false, saturated=false,connected=true; };
struct PowertrainState {
    bool available=false, converged=true;
    std::string diagnostic;
    std::vector<PowertrainShaftState> shafts;
    std::vector<double> sourceTorque, brakeTorque;
    std::vector<PowertrainLinkState> links;
    unsigned iterations=0;
    double residual=0, kineticEnergy=0, constraintEnergyChange=0;
};
bool ValidatePowertrainSettings(const PowertrainSettings&,std::string& error);
bool ValidatePowertrainCommand(const PowertrainSettings&,const PowertrainCommand&,std::string& error);
bool PowertrainSettingsEqual(const PowertrainSettings&,const PowertrainSettings&);

inline constexpr unsigned kPowertrainMaxShafts=32, kPowertrainMaxRows=64, kPowertrainMaxSources=32, kPowertrainMaxBrakes=32;

class PowertrainInstance {
public:
    PowertrainSettings settings;
    PowertrainCommand command;
    PowertrainState state;
    // Native diagnostic: source rotational impulse work since construction.
    // Read-only observation, not simulated energy or durable powertrain state.
    // Includes every coupled interval; resets on cold reconstruction.
    double suppliedWork=0;
    std::vector<double> absoluteSpin; // durable internal rotor momentum coordinate
    std::vector<bool> internalInitialized; // first publication includes bearing motion once
    void Reset(const PowertrainSettings&);
    // Row-major N*N inverse mass W is supplied by the owner. For an unmounted
    // internal shaft W_ii = 1/inertia. Shared bearing reactions also produce
    // off-diagonal entries. A false availability entry disconnects that DOF's
    // sources, brake and every incident link without aliasing a new body slot.
    // Returned N angular impulses are applied exactly once by the owner;
    // state holds omega + W * impulse. Sources/drag integrate only when true.
    std::vector<double> Step(double dt,const std::vector<double>& velocity,
        const std::vector<double>& inverseMass,const std::vector<bool>& available,
        bool integrateSources=true,const std::vector<bool>& linkConnected={});
    // Call once, after the interval's final contact/load projection. Repeated
    // constraint projections themselves never advance time or drive sources.
    void AdvanceAngles(double dt);
    void Persist(class SaveArchive&);
private:
    std::vector<double> linkBudgetUsed, brakeBudgetUsed; // transient interval impulse capacities
};
