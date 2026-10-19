#!/usr/bin/env python3
"""Prepare/compile isolated sphere filters against exact copied current SATv2 inputs."""
import argparse,datetime,hashlib,json,shlex,shutil,subprocess
from pathlib import Path

OWNED=Path(__file__).resolve().parent
ROOT=OWNED.parents[2]
BUILD=ROOT/'build'
p=argparse.ArgumentParser();p.add_argument('--compile',action='store_true');a=p.parse_args()
def sha(path):return hashlib.sha256(path.read_bytes()).hexdigest()
if not a.compile:
    for name in ['Contacts.cpp','Contacts.h','ContactSatFilter.h','ContactGeometryInternal.h','ContactPreparedGeometry.h']:
        target=OWNED/(name if name!='Contacts.cpp' else 'Contacts.input.cpp')
        if target.exists():raise SystemExit(f'refusing to replace prepared input {target}')
        shutil.copy2(ROOT/'src'/name,target)
    source=(OWNED/'Contacts.input.cpp').read_text()
    source=source.replace('#include "ContactSatFilter.h"','#include "ContactSatFilter.h"\n#include "SpherePredicateFilter.h"',1)
    insertion='''    // Same legacy sign classification; successful certificates guarantee the
    // original Iv branch was already strictly resolved. In particular they
    // never skip an old Exact construction which selects response-gap math.
    template<class F> int SphereFilteredSign(int certificate,F legacy) {
        if(certificate!=SpherePredicateFilter::Unresolved) {
            ++diagnostics.predicates;
            ++diagnostics.intervalResolved;
            return certificate;
        }
        return legacy();
    }

'''
    source=source.replace('    struct Context {',insertion+'    struct Context {',1)
    old='''        const Iv r=Iv(double(ra))+Iv(double(rb));
        const Iv pred=SpherePredicate(c.Interval(),r);
        const int sign=Sign(pred,[&]{
            return SpherePredicate(c.E(),Exact(double(ra))+Exact(double(rb)));
        });
        if (sign>0) {
            const int beyond=Sign(SpherePredicate(c.Interval(),r+Iv(double(margin))),[&]{
                return SpherePredicate(c.E(),Exact(double(ra))+Exact(double(rb))+Exact(double(margin)));
            });'''
    new='''        const SpherePredicateFilter filter(c.pa,c.pb,c.number);
        const Iv r=Iv(double(ra))+Iv(double(rb));
        const int sign=SphereFilteredSign(
            filter.Sphere(SpherePredicate(c.number,double(ra)+double(rb)),ra,rb),[&]{
                return Sign(SpherePredicate(c.Interval(),r),[&]{
                    return SpherePredicate(c.E(),Exact(double(ra))+Exact(double(rb)));
                });
            });
        if (sign>0) {
            const int beyond=SphereFilteredSign(
                filter.Sphere(SpherePredicate(c.number,double(ra)+double(rb)+double(margin)),ra,rb,margin),[&]{
                    return Sign(SpherePredicate(c.Interval(),r+Iv(double(margin))),[&]{
                        return SpherePredicate(c.E(),Exact(double(ra))+Exact(double(rb))+Exact(double(margin)));
                    });
                });'''
    if source.count(old)!=1:raise SystemExit('sphere pair insertion differs from expected input')
    source=source.replace(old,new,1)
    old='''        const Iv pred=SphereBoxPredicate(c.Interval(),half,Iv(double(radius)));
        const int sign=Sign(pred,[&]{
            return SphereBoxPredicate(c.E(),half,Exact(double(radius)));
        });
        if (sign>0){
            const int beyond=Sign(SphereBoxPredicate(c.Interval(),half,Iv(double(radius))+Iv(double(margin))),[&]{
                return SphereBoxPredicate(c.E(),half,Exact(double(radius))+Exact(double(margin)));
            });'''
    new='''        const SpherePredicateFilter filter(c.pa,c.pb,c.number);
        const int sign=SphereFilteredSign(
            filter.SphereBox(SphereBoxPredicate(c.number,half,double(radius)),radius,half),[&]{
                return Sign(SphereBoxPredicate(c.Interval(),half,Iv(double(radius))),[&]{
                    return SphereBoxPredicate(c.E(),half,Exact(double(radius)));
                });
            });
        if (sign>0){
            const int beyond=SphereFilteredSign(
                filter.SphereBox(SphereBoxPredicate(c.number,half,double(radius)+double(margin)),radius,half,margin),[&]{
                    return Sign(SphereBoxPredicate(c.Interval(),half,Iv(double(radius))+Iv(double(margin))),[&]{
                        return SphereBoxPredicate(c.E(),half,Exact(double(radius))+Exact(double(margin)));
                    });
                });'''
    if source.count(old)!=1:raise SystemExit('sphere box insertion differs from expected input')
    source=source.replace(old,new,1)
    (OWNED/'Contacts.cpp').write_text(source)
    shutil.copy2(ROOT/'.cache/dense-scalability-20261019/geometry-prototype/v2/Parity.cpp',OWNED/'Parity.cpp')
    inputs={str(path.relative_to(OWNED)):{'sha256':sha(path),'bytes':path.stat().st_size}for path in OWNED.iterdir()if path.is_file()and path.suffix in ['.h','.cpp','.txt']}
    (OWNED/'PREPARED.json').write_text(json.dumps({'utc':datetime.datetime.now(datetime.timezone.utc).isoformat(),'source_head':subprocess.check_output(['git','rev-parse','HEAD'],cwd=ROOT,text=True).strip(),'inputs':inputs,'scope':'isolated sphere certificate research; no production edit'},indent=2)+'\n')
    print('Prepared exact inputs and sphere-only changes; not compiled or tested.')
    raise SystemExit(0)

receipt={'utc':datetime.datetime.now(datetime.timezone.utc).isoformat(),'commands':[],'artifacts':{}}
def run(argv,cwd=ROOT):
    item={'argv':list(map(str,argv)),'cwd':str(cwd)};receipt['commands'].append(item)
    r=subprocess.run(argv,cwd=cwd,text=True,stdout=subprocess.PIPE,stderr=subprocess.STDOUT)
    item.update({'exit':r.returncode,'output':r.stdout});(OWNED/'BUILD.json').write_text(json.dumps(receipt,indent=2)+'\n')
    if r.returncode:print(r.stdout);raise SystemExit(r.returncode)
flags={}
for line in (BUILD/'CMakeFiles/judas_engine.dir/flags.make').read_text().splitlines():
    if ' = ' in line:key,value=line.split(' = ',1);flags[key]=shlex.split(value)
common=['/usr/bin/c++',*flags['CXX_DEFINES'],'-I'+str(OWNED),*flags['CXX_INCLUDES'],*flags['CXX_FLAGS']]
run([*common,str(OWNED/'SphereFilterTests.cpp'),'-o',str(OWNED/'sphere-filter-tests')])
run([*common,'-c',str(OWNED/'Contacts.cpp'),'-o',str(OWNED/'Contacts.cpp.o')])
run([*common,str(OWNED/'Parity.cpp'),str(OWNED/'Contacts.cpp.o'),'-o',str(OWNED/'parity-spheres')])
archive=OWNED/'libjudas_engine.a';shutil.copy2(ROOT/'.cache/dense-scalability-20261019/baseline-e30/libjudas_engine.a',archive)
run(['ar','r',str(archive),str(OWNED/'Contacts.cpp.o')])
link=shlex.split((BUILD/'CMakeFiles/judas_physics_stress_live_benchmark.dir/link.txt').read_text());link=[x for x in link if not x.startswith('-Wl,--dependency-file=')]
link[link.index('-o')+1]=str(OWNED/'judas_physics_stress_live_benchmark');link=[str(archive)if x=='libjudas_engine.a'else x for x in link];run(link,BUILD)
for path in OWNED.iterdir():
    if path.is_file()and path.name not in ['BUILD.json']:
        receipt['artifacts'][path.name]={'sha256':sha(path),'bytes':path.stat().st_size}
receipt['finished_utc']=datetime.datetime.now(datetime.timezone.utc).isoformat();(OWNED/'BUILD.json').write_text(json.dumps(receipt,indent=2)+'\n')
print(json.dumps({'binary':str(OWNED/'judas_physics_stress_live_benchmark'),'sha256':sha(OWNED/'judas_physics_stress_live_benchmark')},indent=2))
