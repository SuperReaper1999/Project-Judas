from pathlib import Path
import json,shlex,subprocess,hashlib,sys
p=Path('/tmp/m87-impact-quality');root=Path('/home/conner/Documents/GitHub/Project-Judas')
s=(p/'ImpactSolver-diagnostic.cpp').read_text()
s=s.replace('#include <cstdint>\n','#include <cstdint>\n#include "FrozenImpact.h"\n#include <memory>\n')
anchor='constexpr std::uint64_t kM87MaximumResidualRows=32768;'
s=s.replace(anchor,anchor+'''\nstd::string gM87CapturePath;
unsigned gM87CaptureFromStep=150,gM87CaptureMinimumDynamic=20;
bool gM87Captured=false;
std::vector<RigidBody*> gM87CaptureBodies;
std::unique_ptr<M87Frozen::Snapshot> M87BeforeCapture(const std::vector<ImpactContact>& contacts,float dt,bool forced,JointSolver* joints){
    if(gM87Captured||gM87CapturePath.empty()||gM87DiagnosticStep<gM87CaptureFromStep||(joints&&!joints->Empty()))return nullptr;
    gM87CaptureBodies.clear();
    auto index=[&](RigidBody* body){auto i=std::find(gM87CaptureBodies.begin(),gM87CaptureBodies.end(),body);if(i!=gM87CaptureBodies.end())return std::size_t(i-gM87CaptureBodies.begin());gM87CaptureBodies.push_back(body);return gM87CaptureBodies.size()-1;};
    for(const auto& contact:contacts){index(contact.a);index(contact.b);}
    unsigned dynamics=0;for(auto* body:gM87CaptureBodies)dynamics+=!body->IsStatic();if(dynamics<gM87CaptureMinimumDynamic)return nullptr;
    auto frozen=std::make_unique<M87Frozen::Snapshot>();frozen->step=gM87DiagnosticStep;frozen->iterations=gM87ImpactIterations;frozen->dt=M87Frozen::Bits(dt);frozen->forceInelastic=forced;
    for(auto* body:gM87CaptureBodies)frozen->before.push_back(M87Frozen::Pack(*body));
    for(const auto& contact:contacts)frozen->contacts.push_back(M87Frozen::Pack(contact,index(contact.a),index(contact.b)));
    return frozen;
}
void M87AfterCapture(std::unique_ptr<M87Frozen::Snapshot> frozen,const std::vector<ContactConstraint>& rows,const ImpactResult& result){
    if(!frozen||!result.energyBudgetSatisfied)return;
    for(auto* body:gM87CaptureBodies)frozen->after.push_back(M87Frozen::Pack(*body));
    for(const auto& row:rows)frozen->rows.push_back(M87Frozen::Pack(row));
    M87Frozen::Write(gM87CapturePath,*frozen);gM87Captured=true;
    std::fprintf(stderr,"FROZEN impact captured step=%u event=%llu bodies=%zu contacts=%zu rows=%zu path=%s\\n",gM87DiagnosticStep,static_cast<unsigned long long>(gM87ResidualEvents),frozen->before.size(),frozen->contacts.size(),rows.size(),gM87CapturePath.c_str());
}
''',1)
anchor='extern "C" void JudasM87ImpactDiagnosticStep(unsigned nextStep){gM87DiagnosticStep=nextStep;}'
s=s.replace(anchor,anchor+'''\nextern "C" void JudasM87ConfigureImpactCapture(const char* path,unsigned fromStep,unsigned minimumDynamic){gM87CapturePath=path?path:"";gM87CaptureFromStep=fromStep;gM87CaptureMinimumDynamic=minimumDynamic;gM87Captured=false;}
''',1)
anchor='    // Including every connected contact is deliberately conservative:'
s=s.replace(anchor,'    auto m87Frozen=M87BeforeCapture(contacts,dt,forceInelastic,joints);\n'+anchor,1)
anchor='    if(gM87ResidualCsv)M87RecordResidual(m_solver.Constraints(),result,forceInelastic);'
s=s.replace(anchor,anchor+'\n    M87AfterCapture(std::move(m87Frozen),m_solver.Constraints(),result);',1)
(p/'ImpactSolver-capture.cpp').write_text(s)
helper=(p/'live-diagnostic.cpp').read_text()
helper=helper.replace('using Clock=','extern "C" void JudasM87ConfigureImpactCapture(const char*,unsigned,unsigned);\nusing Clock=',1)
anchor=' const auto residualPath=output+"/impact-residuals.csv";'
helper=helper.replace(anchor,anchor+'''\n const char* capturePath=std::getenv("M87_CAPTURE_IMPACT");
 const unsigned captureFrom=std::getenv("M87_CAPTURE_FROM_STEP")?unsigned(std::stoul(std::getenv("M87_CAPTURE_FROM_STEP"))):150;
 const unsigned captureBodies=std::getenv("M87_CAPTURE_MIN_BODIES")?unsigned(std::stoul(std::getenv("M87_CAPTURE_MIN_BODIES"))):20;
 JudasM87ConfigureImpactCapture(capturePath,captureFrom,captureBodies);
''',1)
helper=helper.replace('if(residualCsv)JudasM87ImpactDiagnosticStep(step+1);','if(residualCsv||capturePath)JudasM87ImpactDiagnosticStep(step+1);')
(p/'live-capture.cpp').write_text(helper)
commands=json.loads((p/'build-commands.json').read_text())
commands=[[v.replace('ImpactSolver-diagnostic','ImpactSolver-capture').replace('live-diagnostic','live-capture') for v in command]for command in commands]
(p/'build-capture-commands.json').write_text(json.dumps(commands,indent=2)+'\n')
files=[p/'FrozenImpact.h',p/'ImpactSolver-capture.cpp',p/'live-capture.cpp',root/'build/libjudas_engine.a',root/'src/ImpactSolver.cpp',root/'src/ContactSolver.cpp',root/'src/ContactSolver.h']
(p/'capture-source-fingerprints.json').write_text(json.dumps({str(f):hashlib.sha256(f.read_bytes()).hexdigest() for f in files},indent=2)+'\n')
if '--build' in sys.argv:
 for i,command in enumerate(commands):
  print('BUILD CAPTURE',i,shlex.join(command),flush=True);subprocess.run(command,cwd=root/'build',check=True)
else:print('Sources generated only; --build to compile/link capture variant.')
