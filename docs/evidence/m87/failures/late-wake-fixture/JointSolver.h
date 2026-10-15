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
 void Prepare(const std::vector<JointInput>& inputs,float dt,bool warm=true,bool impact=false,bool append=false);
 void SolveIteration(std::size_t first=0);
 void ResetImpulses();
 const std::vector<RigidBody*>& Bodies()const{return m_bodies;}
 bool Empty()const{return m_rows.empty();}
 std::size_t RowCount()const{return m_rows.size();}
private:
 struct Row {RigidBody* a;RigidBody* b;glm::vec3 linearA,linearB,angularA,angularB;glm::mat3 inertiaA,inertiaB;
  float mass,bias,gamma,low,high,impulse;float* cache=nullptr;float* motor=nullptr;JointState* observation=nullptr;};
 void Apply(Row& r,float impulse);
 std::vector<Row> m_rows;std::vector<RigidBody*> m_bodies;std::vector<JointState*> m_prepared;
};
