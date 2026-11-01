#pragma once

#include <memory>
#include <string>

// Optional, deterministic utility decisions. This class owns no world entities,
// script callbacks, navigation or movement. The caller supplies observations and
// executes the selected (action,target) through its normal gameplay mechanisms.
class UtilityAgent {
public:
    explicit UtilityAgent(const std::string& definitionJson);
    ~UtilityAgent();
    UtilityAgent(const UtilityAgent&) = delete;
    UtilityAgent& operator=(const UtilityAgent&) = delete;

    // Strict schema validation and canonical defaults, shared by editor/runtime.
    // Malformed data throws std::invalid_argument; failed Tick/Restore leave the
    // last published state unchanged. Candidate snapshots are bounded to 256.
    static std::string NormalizeDefinition(const std::string& definitionJson);
    bool Due(double dt) const;
    std::string Tick(double dt, const std::string& candidatesJson);
    void Finish(const std::string& status); // success, failure or cancelled
    std::string Snapshot() const;
    void Restore(const std::string& snapshotJson);
    std::string Inspect() const;

private:
    struct Impl;
    std::unique_ptr<Impl> m;
};
