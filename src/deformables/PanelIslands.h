#pragma once
#include "MeshData.h"
#include <array>
#include <cmath>
#include <map>
#include <numeric>
// Imported UV/material seams define visible panel islands. This is preparation
// metadata, not a fracture criterion: physical connections decide separation.
inline std::vector<unsigned> MeshPanelIslands(const MeshData& mesh){
    std::vector<unsigned> groups(mesh.indices.size()/3,~0u);unsigned next=0;
    auto primitives=mesh.primitives;
    if(primitives.empty())primitives.push_back({0,unsigned(mesh.indices.size()),-1});
    for(const auto& p:primitives){
        std::map<std::array<int,5>,unsigned> vertices;std::vector<unsigned> parent;std::vector<glm::uvec3> triangles;
        auto root=[&](unsigned i){while(parent[i]!=i)i=parent[i];return i;};
        for(unsigned t=p.first;t<p.first+p.count;t+=3){
            glm::uvec3 ids;
            for(unsigned k=0;k<3;++k){const auto& v=mesh.vertices.at(mesh.indices.at(t+k));
                std::array<int,5> key{int(std::round(v.position.x*1e5)),int(std::round(v.position.y*1e5)),int(std::round(v.position.z*1e5)),int(std::round(v.uv.x*1e5)),int(std::round(v.uv.y*1e5))};
                auto [it,added]=vertices.emplace(key,unsigned(parent.size()));if(added)parent.push_back(unsigned(parent.size()));ids[k]=it->second;
            }
            parent[root(ids.y)]=root(ids.x);parent[root(ids.z)]=root(ids.x);triangles.push_back(ids);
        }
        std::map<unsigned,unsigned> assigned;
        for(unsigned t=0;t<triangles.size();++t){auto [it,added]=assigned.emplace(root(triangles[t].x),next);if(added)++next;groups[p.first/3+t]=it->second;}
    }
    return groups;
}
