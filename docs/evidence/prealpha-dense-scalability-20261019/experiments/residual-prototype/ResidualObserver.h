#pragma once

// Isolated observer only. Reads prepared constraints and body state; never
// changes impulses, velocities, contacts, solver order or production budgets.
#include <array>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <iomanip>
#include <map>
#include <numeric>
#include <string>
#include <vector>

namespace residual_observer {
struct Numbers {
    std::vector<double> values;
    void Add(double value) { values.push_back(value); }
    void Write(std::ostream& out) const {
        auto sorted = values;
        std::sort(sorted.begin(), sorted.end());
        out << "{\"count\":" << sorted.size();
        if (!sorted.empty()) {
            const auto middle = sorted.size()/2;
            const double median = sorted.size()%2 ? sorted[middle] :
                (sorted[middle-1]+sorted[middle])*.5;
            out << ",\"minimum\":" << sorted.front()
                << ",\"median\":" << median
                << ",\"p95_floor\":" << sorted[std::size_t(.95*(sorted.size()-1))]
                << ",\"maximum\":" << sorted.back()
                << ",\"mean\":" << std::accumulate(sorted.begin(), sorted.end(), 0.0)/sorted.size();
        }
        out << '}';
    }
};

inline double NormalSpeed(const ContactConstraint& row) {
    // Match the actual binary32 solver expression, then retain its result in
    // double for aggregation. These cached offsets are the solver's inputs.
    const auto va = row.bodyA->linearVelocity +
        glm::cross(row.bodyA->angularVelocity, row.velocityOffsetA);
    const auto vb = row.bodyB->linearVelocity +
        glm::cross(row.bodyB->angularVelocity, row.velocityOffsetB);
    return glm::dot(va-vb, row.normal);
}

struct RowSummary {
    Numbers speeds, closingSpeeds, activeResiduals, inactiveViolations;
    Numbers absoluteComplementarityProducts, impulses, separations;
    std::size_t active = 0, negativeImpulse = 0;
    void Add(const ContactConstraint& row) {
        const double vn = NormalSpeed(row);
        const double gapVelocity = vn-double(row.restitutionBias);
        speeds.Add(vn);
        if (vn < 0) closingSpeeds.Add(-vn);
        impulses.Add(row.normalImpulse);
        separations.Add(row.signedSeparation);
        if (row.normalImpulse > 0) {
            ++active;
            activeResiduals.Add(std::abs(gapVelocity));
        } else {
            inactiveViolations.Add(std::max(-gapVelocity, 0.0));
        }
        negativeImpulse += row.normalImpulse < 0;
        absoluteComplementarityProducts.Add(std::abs(double(row.normalImpulse)*gapVelocity));
    }
    void Write(std::ostream& out) const {
        out << "{\"rows\":" << speeds.values.size() << ",\"active_impulse_rows\":" << active
            << ",\"negative_impulse_rows\":" << negativeImpulse << ",\"normal_speed_mps\":";
        speeds.Write(out); out << ",\"negative_normal_speed_magnitude_mps\":";
        closingSpeeds.Write(out); out << ",\"active_normal_residual_mps\":";
        activeResiduals.Write(out); out << ",\"inactive_normal_violation_mps\":";
        inactiveViolations.Write(out); out << ",\"abs_impulse_times_normal_residual\":";
        absoluteComplementarityProducts.Write(out); out << ",\"normal_impulse_Ns\":";
        impulses.Write(out); out << ",\"signed_separation_m\":";
        separations.Write(out); out << '}';
    }
};

inline RowSummary ObserveRows(const std::vector<ContactConstraint>& rows) {
    RowSummary result;
    for (const auto& row : rows) result.Add(row);
    return result;
}

inline bool SamePatchCandidate(const ContactConstraint& a, const ContactConstraint& b) {
    return a.bodyA == b.bodyA && a.bodyB == b.bodyB &&
        std::memcmp(&a.normal.x, &b.normal.x, sizeof(float)) == 0 &&
        std::memcmp(&a.normal.y, &b.normal.y, sizeof(float)) == 0 &&
        std::memcmp(&a.normal.z, &b.normal.z, sizeof(float)) == 0;
}

inline double MatrixEntry(const ContactConstraint& a, const ContactConstraint& b) {
    const glm::dvec3 na(a.normal), nb(b.normal);
    const auto ja = glm::cross(glm::dvec3(a.velocityOffsetA), na);
    const auto jb = glm::cross(glm::dvec3(b.velocityOffsetA), nb);
    const auto ka = glm::cross(glm::dvec3(a.velocityOffsetB), na);
    const auto kb = glm::cross(glm::dvec3(b.velocityOffsetB), nb);
    return (double(a.bodyA->inverseMass)+a.bodyB->inverseMass)*glm::dot(na, nb) +
        glm::dot(ja, glm::dmat3(a.inverseInertiaA)*jb) +
        glm::dot(ka, glm::dmat3(a.inverseInertiaB)*kb);
}

inline unsigned NumericalRank(std::array<std::array<double, 4>, 4> matrix, unsigned count) {
    double scale = 0;
    for (unsigned i=0; i<count; ++i)
        for (unsigned j=0; j<count; ++j) scale=std::max(scale, std::abs(matrix[i][j]));
    // Diagnostic only, explicitly reported in the record. No solver decision
    // uses this threshold or the rank result.
    const double threshold = std::max(1e-14, scale*1e-7);
    unsigned rank=0;
    for (unsigned column=0; column<count && rank<count; ++column) {
        unsigned pivot=rank;
        for (unsigned row=rank+1; row<count; ++row)
            if (std::abs(matrix[row][column])>std::abs(matrix[pivot][column])) pivot=row;
        if (std::abs(matrix[pivot][column])<=threshold) continue;
        std::swap(matrix[pivot], matrix[rank]);
        for (unsigned row=rank+1; row<count; ++row) {
            const double factor=matrix[row][column]/matrix[rank][column];
            for (unsigned j=column; j<count; ++j) matrix[row][j]-=factor*matrix[rank][j];
        }
        ++rank;
    }
    return rank;
}

struct GroupSummary {
    std::array<std::size_t, 6> runSizes{}; // 1..4, and larger-than-four
    std::array<std::size_t, 5> ranks{};
    std::array<RowSummary, 6> residualByRunSize;
    Numbers normalizedOffDiagonal;
    std::map<const RigidBody*, std::size_t> incidentGroups;
    std::size_t groups=0, fourPointRankDeficient=0;

    explicit GroupSummary(const std::vector<ContactConstraint>& rows) {
        for (std::size_t begin=0; begin<rows.size();) {
            std::size_t end=begin+1;
            while (end<rows.size() && SamePatchCandidate(rows[begin],rows[end])) ++end;
            const auto count=end-begin;
            const auto bucket=std::min<std::size_t>(5,count);
            ++runSizes[bucket]; ++groups;
            ++incidentGroups[rows[begin].bodyA];
            ++incidentGroups[rows[begin].bodyB];
            for (auto i=begin; i<end; ++i) residualByRunSize[bucket].Add(rows[i]);
            if (count<=4) {
                std::array<std::array<double,4>,4> matrix{};
                for (std::size_t i=0; i<count; ++i)
                    for (std::size_t j=0; j<count; ++j)
                        matrix[i][j]=MatrixEntry(rows[begin+i],rows[begin+j]);
                const auto rank=NumericalRank(matrix,unsigned(count));
                ++ranks[rank];
                fourPointRankDeficient += count==4 && rank<4;
                for (std::size_t i=0; i<count; ++i)
                    for (std::size_t j=i+1; j<count; ++j)
                        if (matrix[i][i]>0 && matrix[j][j]>0)
                            normalizedOffDiagonal.Add(std::abs(matrix[i][j])/
                                std::sqrt(matrix[i][i]*matrix[j][j]));
            }
            begin=end;
        }
    }
    void Write(std::ostream& out) const {
        out << "{\"identity\":\"contiguous same ordered body pair and bit-identical normal; primitive IDs unavailable, not universal manifold identity\""
            << ",\"groups\":" << groups << ",\"run_size_counts\":{";
        for (unsigned i=1; i<6; ++i) {
            if (i>1) out << ',';
            out << '"' << (i==5?">4":std::to_string(i)) << "\":" << runSizes[i];
        }
        out << "},\"normal_matrix_rank_threshold\":\"max(1e-14, max(abs(K))*1e-7), diagnostic only\""
            << ",\"normal_matrix_rank_counts_up_to_four_rows\":[";
        for (unsigned i=0; i<5; ++i) {if(i)out<<',';out<<ranks[i];}
        out << "],\"four_point_rank_deficient_groups\":" << fourPointRankDeficient
            << ",\"normalized_abs_off_diagonal_K\":";
        normalizedOffDiagonal.Write(out);
        Numbers incident;
        for (const auto& entry : incidentGroups) if(!entry.first->IsStatic()) incident.Add(double(entry.second));
        out << ",\"contact_groups_incident_per_dynamic_body\":";
        incident.Write(out);
        out << ",\"residual_by_contiguous_run_size\":{";
        for (unsigned i=1; i<6; ++i) {
            if(i>1)out<<',';
            out << '"' << (i==5?">4":std::to_string(i)) << "\":";
            residualByRunSize[i].Write(out);
        }
        out << "}}";
    }
};

inline bool Enabled() { return std::getenv("JUDAS_IMPACT_RESIDUAL_OUTPUT") != nullptr; }

inline void Write(const RowSummary& before, const std::vector<ContactConstraint>& after,
                  const ImpactResult& result, float dt, bool joints) {
    const char* path=std::getenv("JUDAS_IMPACT_RESIDUAL_OUTPUT");
    if(!path) return;
    static std::size_t sequence=0;
    ++sequence;
    // The diagnostic is bounded even if an unattended workload cascades. A
    // terminal marker explicitly records censoring instead of implying coverage.
    constexpr std::size_t cap=4096;
    if(sequence>cap+1) return;
    std::ofstream out(path,std::ios::app);
    if(!out) return;
    if(sequence==cap+1) {out<<"{\"truncated_after_records\":"<<cap<<"}\n";return;}
    out << std::setprecision(17) << "{\"sequence\":" << sequence
        << ",\"observer\":\"ImpactSolver post-Prepare inputs and post-Solve output; no mutation\""
        << ",\"dt\":" << dt << ",\"joints\":" << (joints?"true":"false")
        << ",\"contacts\":" << result.contacts
        << ",\"effective_restitution\":" << result.effectiveRestitution
        << ",\"energy_before_J\":" << result.kineticBefore
        << ",\"energy_after_J\":" << result.kineticAfter
        << ",\"boundary_work_J\":" << result.prescribedBoundaryWork
        << ",\"energy_tolerance_J\":" << result.energyTolerance
        << ",\"energy_guard_pass\":" << (result.energyBudgetSatisfied?"true":"false")
        << ",\"safe_retry\":" << (result.safetyFallback?"true":"false")
        << ",\"before\":";
    before.Write(out);out<<",\"after\":";ObserveRows(after).Write(out);
    out<<",\"groups_after\":";GroupSummary(after).Write(out);out<<"}\n";
}
} // namespace residual_observer
