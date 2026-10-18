from pathlib import Path
p=Path(__file__).parent/'sources/PhysicsWorld.cpp';s=p.read_text();at=s.index('    const MeshContactEpisode* Episode(');s=s[:at]+'''    bool MeshFrontSafe(unsigned a,unsigned b,int pa,int pb,double start,double finish) {
        if(!MeshPair(a,b,pa,pb))return false;
        const auto& aa=ImpactPoseAt(a,start);const auto& bb=ImpactPoseAt(b,start);
        const auto first=PrimitiveAt(bodies[a].shape,aa.pose,pa,&*aa.orientation),second=PrimitiveAt(bodies[b].shape,bb.pose,pb,&*bb.orientation);
        const bool firstMesh=first.shape.type==ShapeType::TriangleMesh;const auto& mesh=firstMesh?first:second;
        const auto moving=firstMesh?b:a,stationary=firstMesh?a:b;const auto& body=bodies[moving];
        if(body.shape.type!=ShapeType::Box||body.shape.pivotOffset!=glm::vec3(0)||mesh.shape.asset->twoSided)return false;
        const auto& from=ImpactPoseAt(moving,start).pose;const auto& fixed=ImpactPoseAt(stationary,start).pose;
        const RigidMotionSegment* segment=nullptr;
        for(const auto& entry:body.motion.Segments())if(entry.begin<=start&&entry.end>=finish)segment=&entry;
        if(body.HasMotion()&&!segment)return false;
        const auto& bound=RemainingImpactBound(moving,start,finish);const auto rotation=PrimitiveRotation(mesh),inverse=glm::transpose(rotation);const auto origin=PrimitiveCenter(mesh);
        glm::dvec3 lo(INFINITY),hi(-INFINITY);
        for(unsigned corner=0;corner<8;++corner){const glm::dvec3 world(corner&1?bound.max.x:bound.min.x,corner&2?bound.max.y:bound.min.y,corner&4?bound.max.z:bound.min.z);const auto local=inverse*(world-origin);lo=glm::min(lo,local);hi=glm::max(hi,local);}
        for(unsigned k=0;k<3;++k){lo[k]=std::nextafter(lo[k],-INFINITY);hi[k]=std::nextafter(hi[k],INFINITY);}
        std::vector<uint32_t> candidates;mesh.shape.asset->Candidates(lo,hi,candidates);
        const auto offset=origin-glm::dvec3(fixed.position);
        using contact_geometry::Iv;
        for(const auto feature:candidates){const auto& face=mesh.shape.asset->faces[feature];const auto normal=rotation*face.normal;
            const auto center=rigid_motion_bounds::TryRelativeCentreProjection(segment,nullptr,from,fixed,start,finish,normal);if(!center)return false;
            const auto vertex=offset+rotation*mesh.shape.asset->vertices[face.vertices[0]];Iv plane(0);for(unsigned k=0;k<3;++k)plane=plane+Iv(vertex[k])*Iv(normal[k]);
            const double guard=1e-10*(1+glm::length(vertex)+glm::length(glm::dvec3(from.position)-glm::dvec3(fixed.position)));
            if(center->lo-plane.hi<=guard)return false;
        }
        return true;
    }
'''+s[at:];s=s.replace('const bool meshContinuation=meshAdvance&&!MeshContactFeatures(', 'const bool meshContinuation=meshAdvance&&!MeshFrontSafe(a,b,pa,pb,start,finish)&&!MeshContactFeatures(')
s=s.replace('MeshPair(key[0],key[1],key[2],key[3])?ImpactTime(key[0],key[1],key[2],key[3],now,dt):', 'MeshPair(key[0],key[1],key[2],key[3])&&!MeshFrontSafe(key[0],key[1],key[2],key[3],now,dt)?ImpactTime(key[0],key[1],key[2],key[3],now,dt):')
s=s.replace('for(const auto& key:supportPairs)if(MeshPair(key[0],key[1],key[2],key[3]))schedule(key);', 'for(const auto& key:supportPairs)if(MeshPair(key[0],key[1],key[2],key[3])&&!MeshFrontSafe(key[0],key[1],key[2],key[3],now,dt))schedule(key);')
# Native morphologies alone determine this dispatch; avoid constructing/copying Shape values merely to inspect a kind.
a=s.index('    bool MeshPair(');b=s.index('    bool MeshFrontSafe(',a);s=s[:a]+'''    bool MeshPair(unsigned a,unsigned b,int pa,int pb) const {
        const auto type=[](const Shape& shape,int part){return shape.type==ShapeType::CompoundBoxes?shape.boxes[std::size_t(part)].type:shape.type;};
        const auto first=type(bodies[a].shape,pa),second=type(bodies[b].shape,pb);const auto poly=[](ShapeType t){return t==ShapeType::Box||t==ShapeType::ConvexHull;};
        return (first==ShapeType::TriangleMesh&&!bodies[a].HasMotion()&&poly(second))||(second==ShapeType::TriangleMesh&&!bodies[b].HasMotion()&&poly(first));
    }
'''+s[b:];p.write_text(s)
