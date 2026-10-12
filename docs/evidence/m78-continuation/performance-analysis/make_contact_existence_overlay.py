#!/usr/bin/env python3
"""Prepare diagnostic-only exact existence candidate; no production edit/build."""
import argparse,hashlib,json
from pathlib import Path
ROOT=Path(__file__).resolve().parents[4]
p=argparse.ArgumentParser();p.add_argument('--source-dir',type=Path,default=ROOT/'src');p.add_argument('--output',type=Path,required=True)
a=p.parse_args();source=a.source_dir.resolve();output=a.output.resolve()
if output.exists():raise SystemExit('Preserve prior snapshot: choose a new output directory')
if not output.is_relative_to(ROOT/'docs/evidence/m78-continuation') and not output.is_relative_to(ROOT/'.cache/m78-continuation'):
 raise SystemExit('Output must be an owned continuation directory')
files={n:(source/n).read_text() for n in ['CollisionGeometry.cpp','CollisionGeometry.h','PhysicsWorld.cpp']}
cpp=files['CollisionGeometry.cpp'];begin=cpp.index(' V origin=PrimitiveCenter(b);Poly pa=poly(a,origin);auto test=[&](Poly pb){',cpp.index('ContactManifold CookedContacts('));end=cpp.index(' reduce(contacts,out);return out;',begin)
old=cpp[begin:end]
visitor='''// Shared accepted-witness traversal. A full manifold consumer visits every
// witness in the original deterministic order. An existence-only consumer may
// stop after the first accepted witness; it never changes SAT, clipping, margins,
// sidedness, crease handling or the contacts used by a physical solver.
template<class Consumer>
bool visitCookedPolyContacts(const PrimitivePose& a,const PrimitivePose& b,double margin,Consumer consume) {
 V origin=PrimitiveCenter(b);Poly pa=poly(a,origin);auto test=[&](Poly pb){
  auto s=sat(pa,pb);if(s.gap>margin)return false;
  bool faceWitness=false;
  const auto pairs=witnesses(pa,pb,s,&faceWitness);
  for(auto [x,y]:pairs){
   const double gap=faceWitness?glm::dot(s.normal,x-y):s.gap;
   if(gap<=margin&&consume(x+origin,y+origin,s.normal,gap))return true;
  }
  return false;
 };
 if(b.shape.type==ShapeType::ConvexHull||b.shape.type==ShapeType::Box)return test(poly(b,origin));
 if(b.shape.type==ShapeType::TriangleMesh){V lo(INFINITY),hi(-INFINITY),center(0);for(V p:pa.points){lo=glm::min(lo,p+origin);hi=glm::max(hi,p+origin);center+=p;}center/=double(pa.points.size());for(auto i:candidates(b,lo,hi,margin)){++stats.triangles;Poly pb=triangle(b,i,origin);double side=glm::dot(pb.normals[0],center-pb.points[0]);if(!b.shape.asset->twoSided&&side<-eps)continue;if(side<0){reverseTriangleSide(pb,*b.shape.asset,i);}if(test(std::move(pb)))return true;}return false;}
 throw std::invalid_argument("unsupported cooked contact target");
}
'''
cpp=cpp[:begin]+''' visitCookedPolyContacts(a,b,margin,[&](V x,V y,V normal,double gap){
  contacts.push_back(make(a,b,x,y,normal,gap));return false;
 });
'''+cpp[end:]
anchor='}\nShape Shape::Cooked'
if cpp.count(anchor)!=1:raise SystemExit('Shared helper placement anchor changed')
cpp=cpp.replace(anchor,visitor+'}\nShape Shape::Cooked',1)
anchor='CollisionMassProperties ShapeMassProperties('
if cpp.count(anchor)!=1:raise SystemExit('Existence function placement anchor changed')
exists='''// Boolean truth for CCD observations, not a replacement response manifold.
bool CookedHasContacts(const PrimitivePose& a,const PrimitivePose& b,double margin) {
 if(a.shape.type==ShapeType::TriangleMesh)return CookedHasContacts(b,a,margin);
 // Keep unoptimized sphere/capsule and unsupported cases on the same dispatcher.
 if(a.shape.type==ShapeType::Sphere||a.shape.type==ShapeType::Capsule||
    b.shape.type==ShapeType::Sphere||b.shape.type==ShapeType::Capsule)
  return CookedContacts(a,b,margin).count>0;
 if(!a.shape.asset&&!b.shape.asset)throw std::invalid_argument("CookedContacts called without cooked geometry");
 if(a.shape.type!=ShapeType::Box&&a.shape.type!=ShapeType::ConvexHull)throw std::invalid_argument("unsupported cooked contact pair");
 return visitCookedPolyContacts(a,b,margin,[](V,V,V,double){return true;});
}
'''
cpp=cpp.replace(anchor,exists+anchor,1)
header=files['CollisionGeometry.h'];anchor='ContactManifold CookedContacts(const PrimitivePose&,const PrimitivePose&,double margin);'
if header.count(anchor)!=1:raise SystemExit('Header anchor changed')
header=header.replace(anchor,anchor+'\n// Exact accepted-contact existence; full manifolds remain authoritative for response.\nbool CookedHasContacts(const PrimitivePose&,const PrimitivePose&,double margin);',1)
world=files['PhysicsWorld.cpp'];anchor='    // CCD bounds use the geometric drift generator, not the transported\n'
if world.count(anchor)!=1:raise SystemExit('Physics truth helper placement anchor changed')
truth='''    bool PairTouches(unsigned a,unsigned b,int pa,int pb,double time) {
        const auto& aa=ImpactPoseAt(a,time);const auto& bb=ImpactPoseAt(b,time);
        const auto posa=PrimitiveAt(bodies[a].shape,aa.pose,pa,&*aa.orientation);
        const auto posb=PrimitiveAt(bodies[b].shape,bb.pose,pb,&*bb.orientation);
        if(posa.shape.asset||posb.shape.asset)return CookedHasContacts(posa,posb,0);
        return ComputeContacts(posa,posb,0,&*aa.orientation,&*bb.orientation).count>0;
    }
'''
world=world.replace(anchor,truth+anchor,1)
anchor='const bool touching=PairAt(a,b,pa,pb,at,0).count>0;'
if world.count(anchor)!=1:raise SystemExit('Actual truth anchor changed')
world=world.replace(anchor,'const bool touching=PairTouches(a,b,pa,pb,at);',1)
anchor='hit=hit||PairAt(a,b,pa,pb,now,0).count>0;'
if world.count(anchor)!=1:raise SystemExit('Island truth anchor changed')
world=world.replace(anchor,'hit=hit||PairTouches(a,b,pa,pb,now);',1)
# An independent full-manifold oracle compiled from the pre-refactor snapshot.
# Reuse unchanged geometry/public helpers, but keep original SAT/witness/reduction
# and function control flow in a separate TU with its own anonymous helpers.
original=files['CollisionGeometry.cpp'];prefix=original[:original.index('Shape Shape::Cooked(')]
start=original.index('ContactManifold CookedContacts(');finish=original.index('CollisionMassProperties ShapeMassProperties(',start)
reference=prefix+original[start:finish].replace('CookedContacts','M78OriginalCookedContacts')
output.mkdir(parents=True)
for n,s in files.items():(output/(n+'.original')).write_text(s)
(output/'CollisionGeometry.cpp').write_text(cpp);(output/'CollisionGeometry.h').write_text(header)
(output/'PhysicsWorld.cpp').write_text(world);(output/'OriginalCookedContactOracle.cpp').write_text(reference)
sha=lambda p:hashlib.sha256(p.read_bytes()).hexdigest()
receipt={'status':'UNBUILT ISOLATED PROPOSAL; requires measured dominant cost and parity checks before any production integration','inputDirectory':str(source),'inputSHA256':{n:sha(source/n) for n in files},'outputSHA256':{n:sha(output/n) for n in ['CollisionGeometry.cpp','CollisionGeometry.h','PhysicsWorld.cpp','OriginalCookedContactOracle.cpp']},'scope':'Shared accepted-witness visitor; short circuit Boolean queries only; full manifold consumer retains original visit order and contact construction','preserved':'SAT/witness arithmetic, BVH, margins, sidedness, active-edge policy, full impulse rows, CCD advancement and all iteration/cadence limits','buildRun':False,'productionEdited':False}
(output/'PROPOSAL.json').write_text(json.dumps(receipt,indent=2)+'\n');print(json.dumps(receipt,indent=2))
