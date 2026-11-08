#pragma once
struct CollisionAsset;
namespace JudasCollisionDetail {
// Internal shared deterministic BVH construction. Specialized topology builders
// must establish the same validated faces/vertices before invoking this.
void BuildTree(CollisionAsset &);
} // namespace JudasCollisionDetail
