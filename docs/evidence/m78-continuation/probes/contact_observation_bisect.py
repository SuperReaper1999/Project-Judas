#!/usr/bin/env python3
"""Isolated completed-contact observation diagnostic. Never edits production."""
import pathlib,shlex,subprocess,json,hashlib,shutil,time,sys
r=pathlib.Path.cwd();b=r/'.cache/m78-followup/release';ev=r/'docs/evidence/m78-continuation'
out=r/'.cache/m78-continuation/variants'
h=lambda p:hashlib.sha256(p.read_bytes()).hexdigest()
flags={}
for line in (b/'CMakeFiles/judas_engine.dir/flags.make').read_text().splitlines():
 if ' = ' in line:
  k,v=line.split(' = ',1);flags[k]=shlex.split(v)
def replace_once(s,old,new):
 assert s.count(old)==1,(old[:90],s.count(old))
 return s.replace(old,new)
def run(name,base_name):
 base=out/base_name;folder=out/name;folder.mkdir(exist_ok=False)
 for header in base.rglob('*'):
  if header.is_file() and header.suffix in ['.h','.hpp']:
   dest=folder/header.relative_to(base);dest.parent.mkdir(parents=True,exist_ok=True);shutil.copy2(header,dest)
 source=(base/'PhysicsWorld.cpp').read_text()
 source=replace_once(source,'    std::vector<CachedContact> contactCache;','''    std::vector<CachedContact> contactCache;
    // Diagnostic: completed solver observations are not warm-start storage.
    // Island velocity changes may invalidate warm starts without erasing
    // untouched load-bearing contacts from the completed physical step.
    std::vector<CachedContact> completedContactObservations;''')
 source=replace_once(source,'    std::swap(w.contactCache, w.pendingCache);','''    std::swap(w.contactCache, w.pendingCache);
    w.completedContactObservations=w.contactCache;''')
 start=source.index('bool PhysicsWorld::ReadBodyContactSample(');end=source.index('const std::vector<PhysicsWorld::TouchEvent>&',start)
 source=source[:start]+source[start:end].replace('m_impl->contactCache','m_impl->completedContactObservations')+source[end:]
 source=replace_once(source,'            std::vector<ImpactContact> rows;\n            std::vector<std::array<unsigned,4>> eventPairs;','''            std::vector<ImpactContact> rows;
            std::vector<ContactKey> observationKeys;
            std::vector<std::array<unsigned,4>> eventPairs;''')
 source=replace_once(source,'                            rows.push_back({&a.rigidBody,&b.rigidBody,m.points[k],','''                            observationKeys.push_back({pair.first,pair.second,a.generation,b.generation,pa,pb});
                            rows.push_back({&a.rigidBody,&b.rigidBody,m.points[k],''')
 marker=source.index('            std::vector<ContactKey> observationKeys;')
 prefix=source[:marker];source=source[marker:]
 source=replace_once(source,'            if(!result.energyBudgetSatisfied) throw std::runtime_error("inelastic impact energy guard failed");','''            if(!result.energyBudgetSatisfied) throw std::runtime_error("inelastic impact energy guard failed");
            // Replace only primitive groups for which actual TOI geometry
            // was solved. Never replay warm impulses or duplicate repeated
            // TOI loads. Unaffected stance groups retain their completed rows.
            completedContactObservations.erase(std::remove_if(completedContactObservations.begin(),completedContactObservations.end(),
                [&](const CachedContact& cached){return std::any_of(observationKeys.begin(),observationKeys.end(),
                    [&](const ContactKey& key){return cached.key.SameBodies(key);});}),completedContactObservations.end());
            const auto& observed=impactSolver.Constraints();
            if(observed.size()!=observationKeys.size())throw std::runtime_error("diagnostic impact observation mapping mismatch");
            for(std::size_t i=0;i<observed.size();++i){
                const auto& row=observed[i];const auto& key=observationKeys[i];
                if(row.normalImpulse<=0)continue;
                completedContactObservations.push_back({key,row.localAnchorA,row.normal,row.normalImpulse,row.tangentImpulse,
                    float(row.signedSeparation),bodies[key.slotA].contactObservationRevision,bodies[key.slotB].contactObservationRevision});
            }
            std::stable_sort(completedContactObservations.begin(),completedContactObservations.end(),
                [](const CachedContact& a,const CachedContact& b){return a.key<b.key;});''')
 source=prefix+source
 # Object authority changes discard observations as well as warm starts.
 source=replace_once(source,'    auto& cache=m_impl->contactCache;cache.erase(std::remove_if(cache.begin(),cache.end(),uses),cache.end());','''    auto& cache=m_impl->contactCache;cache.erase(std::remove_if(cache.begin(),cache.end(),uses),cache.end());
    auto& observations=m_impl->completedContactObservations;observations.erase(std::remove_if(observations.begin(),observations.end(),uses),observations.end());''')
 (folder/'PhysicsWorld.cpp').write_text(source)
 archive=folder/'libjudas_engine.a';shutil.copy2(base/'libjudas_engine.a',archive)
 obj=folder/'PhysicsWorld.cpp.o';cmd=['c++',*flags.get('CXX_DEFINES',[]),'-I'+str(folder),*flags.get('CXX_INCLUDES',[]),*flags.get('CXX_FLAGS',[]),'-c',str(folder/'PhysicsWorld.cpp'),'-o',str(obj)]
 records=[]
 with (ev/(name+'-compile.log')).open('xb') as log:code=subprocess.run(cmd,stdout=log,stderr=subprocess.STDOUT).returncode
 records.append({'command':cmd,'exit':code});assert code==0,name
 subprocess.run(['ar','r',str(archive),str(obj)],check=True);subprocess.run(['ranlib',str(archive)],check=True)
 target='judas_physical_balance_controller_tests';link=shlex.split((b/'CMakeFiles'/ (target+'.dir')/'link.txt').read_text())
 link=[str(archive) if x=='libjudas_engine.a' else str(folder/'controller') if x==target else x for x in link if not x.startswith('-Wl,--dependency-file')]
 subprocess.run(link,cwd=b,check=True);start=time.perf_counter()
 with (ev/(name+'-controller.log')).open('xb') as log:code=subprocess.run([str(folder/'controller')],cwd=r,stdout=log,stderr=subprocess.STDOUT).returncode
 (ev/(name+'-bisect.json')).write_text(json.dumps({'name':name,'base':base_name,'sourceSHA256':h(folder/'PhysicsWorld.cpp'),'builds':records,'engineSHA256':h(archive),'exeSHA256':h(folder/'controller'),'exitCode':code,'seconds':time.perf_counter()-start,'fixtureSHA256':h(r/'tests/PhysicalBalanceControllerTests.cpp'),'scope':'Diagnostic completed-contact observation variant; no production/fixture changes'},indent=2)+'\n')
 print(name,code,flush=True)
for arg in sys.argv[1:]:
 name,base=arg.split(':');run(name,base)
