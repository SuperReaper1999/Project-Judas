#pragma once
#include <cstdint>
#include <map>
#include <string>

// Optional exclusive target leases. Keys have project meaning; this board owns
// no inventory, entities, navigation destinations or gameplay effects. Scopes
// are session-local identities, never save or network tokens.
class AIReservationBoard {
public:
    bool Acquire(const std::string& target, std::uint64_t claimant, double now, double leaseSeconds);
    bool Available(const std::string& target, std::uint64_t claimant, double now);
    bool Release(const std::string& target, std::uint64_t claimant);
    void ReleaseAll(std::uint64_t claimant);
    std::size_t Size(double now);
private:
    struct Lease { std::uint64_t claimant; double expires; };
    std::map<std::string, Lease> m_leases;
    std::map<std::uint64_t, std::size_t> m_counts;
    double m_time = 0;
    void Drop(std::map<std::string, Lease>::iterator);
    void Prune(double now);
    void Time(double now);
};
