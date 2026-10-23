#pragma once
#include "JointTypes.h"
#include "RigidBody.h"
#include <array>
#include <cstddef>
#include <vector>
struct JointInput {RigidBody* a;RigidBody* b;JointState* state;std::array<float,12>* warm;glm::vec3 pivotOffsetA{0},pivotOffsetB{0};};
// Rows use the same sequential accumulated-impulse method as contacts. No pose
// teleportation: velocity bias stabilizes drift; soft rows are implicit springs.
class JointSolver {
public:
 // Appending adds only newly awakened joint participants at the same physical
 // boundary. Existing rows, accumulated impulses and observations are retained.
 // positionalRate: fraction of a rigid row's position error corrected per dt.
 // warmEqualityOnly: warm only rigid two-sided (equality) rows; limits,
 // drives, springs and materials start cold.
 void Prepare(const std::vector<JointInput>& inputs,float dt,bool warm=true,bool impact=false,bool append=false,float positionalRate=.2f,bool warmEqualityOnly=false);
 // useBias=false (substep relax) drops only the rigid rows' positional
 // correction; springs, resistance and motors keep their targets.
 void SolveIteration(std::size_t first=0,bool useBias=true);
 void ResetImpulses();
 const std::vector<RigidBody*>& Bodies()const{return m_bodies;}
 bool Empty()const{return m_rows.empty();}
 std::size_t RowCount()const{return m_rows.size();}
private:
 // Membership only: this table is never traversed to build or solve rows.
 // Retained flat storage avoids both quadratic pointer scans and a separate
 // allocation for every participant at every preparation boundary.
 class PointerMembership {
 public:
  void Clear();
  bool Insert(const void* pointer);
 private:
  void Grow();
  static std::size_t Hash(const void* pointer);
  std::vector<const void*> slots;
  std::size_t used=0;
 };
 struct Row {RigidBody* a;RigidBody* b;glm::vec3 linearA,linearB,angularA,angularB;glm::mat3 inertiaA,inertiaB;
  float mass,bias,gamma,low,high,impulse;float* cache=nullptr;float* motor=nullptr;JointState* observation=nullptr;bool positional=false;};
 void Apply(Row& r,float impulse);
 std::vector<Row> m_rows;std::vector<RigidBody*> m_bodies;
 PointerMembership m_bodyMembership,m_preparedMembership;
};
