#include "AIReservations.h"
#include <cmath>
#include <stdexcept>

namespace {
void Identity(const std::string& key, std::uint64_t claimant) {
    if (!claimant || key.empty() || key.size() > 256 || key.find('\0') != std::string::npos)
        throw std::invalid_argument("AI reservation requires a valid scope and 1..256-byte target key");
}
}
void AIReservationBoard::Time(double now) {
    if (!std::isfinite(now) || now < m_time)
        throw std::invalid_argument("AI reservation time must be finite and monotonic");
    m_time = now;
}
void AIReservationBoard::Drop(std::map<std::string, Lease>::iterator it) {
    auto count = m_counts.find(it->second.claimant);
    if (--count->second == 0) m_counts.erase(count);
    m_leases.erase(it);
}
void AIReservationBoard::Prune(double now) {
    for (auto it = m_leases.begin(); it != m_leases.end();) {
        if (it->second.expires <= now) { auto expired = it++; Drop(expired); }
        else ++it;
    }
}
bool AIReservationBoard::Acquire(const std::string& target, std::uint64_t claimant,
                                  double now, double seconds) {
    Identity(target, claimant);
    if (!std::isfinite(seconds) || seconds < .001 || seconds > 60 || (!std::isfinite(now + seconds) || !(now + seconds > now)))
        throw std::invalid_argument("AI target lease must be .001..60 finite simulation seconds");
    Time(now);
    auto found = m_leases.find(target);
    if (found != m_leases.end() && found->second.expires <= now) { Drop(found); found = m_leases.end(); }
    if (found != m_leases.end()) {
        if (found->second.claimant != claimant) return false;
        found->second.expires = now + seconds;
        return true;
    }
    auto count = m_counts.find(claimant);
    if (m_leases.size() >= 4096 || (count != m_counts.end() && count->second >= 256)) {
        Prune(now);
        count = m_counts.find(claimant);
        if (m_leases.size() >= 4096 || (count != m_counts.end() && count->second >= 256)) return false;
    }
    // Commit both indices together even if allocation fails.
    auto inserted = m_leases.emplace(target, Lease{claimant, now + seconds});
    try { ++m_counts[claimant]; }
    catch (...) { m_leases.erase(inserted.first); throw; }
    return true;
}
bool AIReservationBoard::Available(const std::string& target, std::uint64_t claimant, double now) {
    Identity(target, claimant);
    Time(now);
    auto found = m_leases.find(target);
    if (found != m_leases.end() && found->second.expires <= now) { Drop(found); return true; }
    return found == m_leases.end() || found->second.claimant == claimant;
}
bool AIReservationBoard::Release(const std::string& target, std::uint64_t claimant) {
    Identity(target, claimant);
    auto found = m_leases.find(target);
    if (found == m_leases.end() || found->second.claimant != claimant) return false;
    Drop(found);
    return true;
}
void AIReservationBoard::ReleaseAll(std::uint64_t claimant) {
    if (!claimant) throw std::invalid_argument("AI reservation scope required");
    for (auto it = m_leases.begin(); it != m_leases.end();) {
        if (it->second.claimant == claimant) { auto owned = it++; Drop(owned); }
        else ++it;
    }
}
std::size_t AIReservationBoard::Size(double now) {
    Time(now); Prune(now); return m_leases.size();
}
