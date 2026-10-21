from pathlib import Path
import tarfile,shutil
root=Path.cwd();out=root/'.cache/contact-redesign-20261021/causal-old';out.mkdir(exist_ok=True);inc=out/'include';inc.mkdir(exist_ok=True)
with tarfile.open(root/'.cache/event-architecture-20261021/final-snapshot/source-inputs.tar.gz') as a:a.extractall(inc,filter='data')
shutil.copy2(root/'.cache/impact-response-20261021/CaseIO.h',out/'CaseIO.h')
s=(root/'.cache/impact-response-20261021/WorldImpactEvents.cpp').read_text().replace('.cache/impact-response-20261021/event-', '.cache/contact-redesign-20261021/causal-old/event-')
where='            diagnostic["seed_rows"] = DiagnosisJSON::array();'
s=s.replace(where,'''            diagnostic["support_pairs"] = supportPairs;
            diagnostic["spinning_pairs"] = spinningPairs;
            diagnostic["row_keys"] = DiagnosisJSON::array();
            diagnostic["row_allowance"] = DiagnosisJSON::array();
            for (const auto& key:observationKeys) {
                diagnostic["row_keys"].push_back({key.slotA,key.slotB,key.partA,key.partB});
                diagnostic["row_allowance"].push_back(std::min(ContactSolver::kPenetrationSlop,
                    float(SpinPenetrationTarget({key.slotA,key.slotB,unsigned(key.partA),unsigned(key.partB)}))));
            }
'''+where)
(out/'WorldImpactEvents.cpp').write_text(s)
s=(root/'.cache/impact-response-20261021/build.py').read_text();s=s.replace("o=Path(__file__).resolve().parent", "o=Path(__file__).resolve().parent")
s=s.replace("'-I'+str(root/'.cache/dense-root-cause-20261020/include/src/physics'),'-I'+str(root/'.cache/dense-root-cause-20261020/include/src')", "'-I'+str(o/'include/src/physics'),'-I'+str(o/'include/src')")
(out/'build.py').write_text(s)
