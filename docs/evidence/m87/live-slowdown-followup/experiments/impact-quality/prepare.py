from pathlib import Path
import hashlib,json,shlex,subprocess
root=Path('/home/conner/Documents/GitHub/Project-Judas')
base=(root/'src/ImpactSolver.cpp').read_text()
Path('/tmp/m87-impact-quality/ImpactSolver-base.cpp').write_text(base)
s=base.replace('#include <stdexcept>\n','#include <stdexcept>\n#include <cstdio>\n#include <cstdint>\n')
anchor='''namespace {
// Matches'''
replacement='''namespace {
// ISOLATED DIAGNOSIS ONLY: not a production solver-budget change.
int gM87ImpactIterations=ContactSolver::kVelocityIterations;
std::FILE* gM87ResidualCsv=nullptr;
std::uint64_t gM87ResidualEvents=0,gM87ResidualRows=0;
unsigned gM87DiagnosticStep=1,gM87ResidualFromStep=0;
constexpr std::uint64_t kM87MaximumResidualRows=32768;
// Matches'''
assert anchor in s;s=s.replace(anchor,replacement,1)
anchor='''} // namespace

ImpactResult ImpactSolver::Solve'''
replacement='''void M87RecordResidual(const std::vector<ContactConstraint>& rows,const ImpactResult& result,bool forced) {
    ++gM87ResidualEvents;
    if(!gM87ResidualCsv||gM87DiagnosticStep<gM87ResidualFromStep||gM87ResidualRows>=kM87MaximumResidualRows||!result.energyBudgetSatisfied)return;
    double maximumClosing=0,sumClosingSquared=0,maximumProjected=0,sumProjectedSquared=0;
    double maximumPrecise=0,sumPreciseSquared=0,maximumFloatDifference=0;
    unsigned closingRows=0,preciseClosingRows=0,projectedRows=0,zeroMassRows=0;
    for(const auto& row:rows){
        const auto& a=*row.bodyA;const auto& b=*row.bodyB;
        // Match actual binary32 solver expression, not a debug world point.
        const auto relative=a.linearVelocity+glm::cross(a.angularVelocity,row.velocityOffsetA)-
                            (b.linearVelocity+glm::cross(b.angularVelocity,row.velocityOffsetB));
        const float vn=glm::dot(relative,row.normal);
        const double closing=std::max(0.0,double(row.restitutionBias)-double(vn));
        maximumClosing=std::max(maximumClosing,closing);sumClosingSquared+=closing*closing;
        closingRows+=closing>0;
        if(row.normalMass>0){
            const float proposed=std::max(row.normalImpulse+row.normalMass*(row.restitutionBias-vn),0.f);
            const double projected=std::abs(double(proposed)-double(row.normalImpulse))/row.normalMass;
            maximumProjected=std::max(maximumProjected,projected);sumProjectedSquared+=projected*projected;++projectedRows;
        }else ++zeroMassRows;
        // Independent precision check: original precise normal/local anchors.
        const auto offsetA=ContactRotation(a.orientation)*row.localAnchorA;
        const auto offsetB=ContactRotation(b.orientation)*row.localAnchorB;
        const auto physical=glm::dvec3(a.linearVelocity)+glm::cross(glm::dvec3(a.angularVelocity),offsetA)-
                            (glm::dvec3(b.linearVelocity)+glm::cross(glm::dvec3(b.angularVelocity),offsetB));
        const double preciseVn=glm::dot(physical,row.preciseNormal);
        const double preciseClosing=std::max(0.0,double(row.restitutionBias)-preciseVn);
        maximumPrecise=std::max(maximumPrecise,preciseClosing);sumPreciseSquared+=preciseClosing*preciseClosing;
        preciseClosingRows+=preciseClosing>0;maximumFloatDifference=std::max(maximumFloatDifference,std::abs(preciseVn-double(vn)));
    }
    const double count=double(rows.size());
    std::fprintf(gM87ResidualCsv,"%u,%llu,%d,%zu,%zu,%d,%d,%.9g,%d,%.17g,%.17g,%.17g,%.17g,%u,%.17g,%.17g,%.17g,%.17g,%u,%.17g,%u\\n",
        gM87DiagnosticStep,static_cast<unsigned long long>(gM87ResidualEvents),gM87ImpactIterations,rows.size(),result.contacts,
        result.ambiguous,forced,result.effectiveRestitution,result.safetyFallback,result.kineticBefore,result.kineticAfter,
        maximumClosing,count>0?std::sqrt(sumClosingSquared/count):0.,closingRows,
        maximumProjected,projectedRows?std::sqrt(sumProjectedSquared/projectedRows):0.,
        maximumPrecise,count>0?std::sqrt(sumPreciseSquared/count):0.,preciseClosingRows,maximumFloatDifference,zeroMassRows);
    ++gM87ResidualRows;std::fflush(gM87ResidualCsv);
}
} // namespace

extern "C" int JudasM87ConfigureImpactDiagnostic(int iterations,const char* csv,unsigned fromStep){
    if(iterations!=10&&iterations!=20&&iterations!=40&&iterations!=80)return 0;
    gM87ImpactIterations=iterations;gM87ResidualFromStep=fromStep;
    if(gM87ResidualCsv)std::fclose(gM87ResidualCsv);
    gM87ResidualCsv=csv?std::fopen(csv,"w"):nullptr;
    if(csv&&!gM87ResidualCsv)return 0;
    if(gM87ResidualCsv)std::fprintf(gM87ResidualCsv,"step,event,iterations,rows,contacts,ambiguous,forced_inelastic,effective_restitution,safety_fallback,kinetic_before_j,kinetic_after_j,max_solver_closing_mps,rms_solver_closing_mps,solver_closing_rows,max_projected_complementarity_mps,rms_projected_complementarity_mps,max_precise_closing_mps,rms_precise_closing_mps,precise_closing_rows,max_solver_precise_vn_difference_mps,zero_normal_mass_rows\\n");
    return 1;
}
extern "C" void JudasM87ImpactDiagnosticStep(unsigned nextStep){gM87DiagnosticStep=nextStep;}

ImpactResult ImpactSolver::Solve'''
assert anchor in s;s=s.replace(anchor,replacement,1)
s=s.replace('m_solver.SolveVelocities();','m_solver.SolveVelocities(gM87ImpactIterations);')
s=s.replace('i<ContactSolver::kVelocityIterations','i<gM87ImpactIterations')
needle='    return result;\n}';end=s.rfind(needle);assert end>=0
s=s[:end]+'    if(gM87ResidualCsv)M87RecordResidual(m_solver.Constraints(),result,forceInelastic);\n    return result;\n}'+s[end+len(needle):]
Path('/tmp/m87-impact-quality/ImpactSolver-diagnostic.cpp').write_text(s)
helper=Path('/tmp/m87-live-diagnostics/live.cpp').read_text()
Path('/tmp/m87-impact-quality/live-base.cpp').write_text(helper)
helper=helper.replace('using Clock=','extern "C" int JudasM87ConfigureImpactDiagnostic(int,const char*,unsigned);\nextern "C" void JudasM87ImpactDiagnosticStep(unsigned);\nusing Clock=',1)
anchor=''' std::filesystem::create_directories(output);std::ofstream frames'''
replace=''' const int impactIterations=argc>11?std::stoi(argv[11]):10;
 const bool residualCsv=argc>12?std::stoi(argv[12])!=0:false;
 const unsigned residualFromStep=std::getenv("M87_IMPACT_RESIDUAL_FROM_STEP")?unsigned(std::stoul(std::getenv("M87_IMPACT_RESIDUAL_FROM_STEP"))):0;
 std::filesystem::create_directories(output);
 const auto residualPath=output+"/impact-residuals.csv";
 if(!JudasM87ConfigureImpactDiagnostic(impactIterations,residualCsv?residualPath.c_str():nullptr,residualFromStep)){std::cerr<<"invalid impact diagnostic configuration\\n";return 2;}
 std::cerr<<"ISOLATED impact iterations="<<impactIterations<<" residual_csv="<<residualCsv<<" from_step="<<residualFromStep<<"\\n";
 std::ofstream frames'''
assert helper.count(anchor)==1;helper=helper.replace(anchor,replace)
anchor="steps.flush();});"
replace="steps.flush();if(residualCsv)JudasM87ImpactDiagnosticStep(step+1);});"
assert helper.count(anchor)==1;helper=helper.replace(anchor,replace)
Path('/tmp/m87-impact-quality/live-diagnostic.cpp').write_text(helper)
files=[root/'src/ImpactSolver.cpp',root/'src/ContactSolver.cpp',root/'src/ContactSolver.h',root/'src/PhysicsWorld.cpp',root/'src/RigidMotionBounds.h',root/'build/libjudas_engine.a',Path('/tmp/m87-impact-quality/ImpactSolver-diagnostic.cpp'),Path('/tmp/m87-impact-quality/live-diagnostic.cpp')]
Path('/tmp/m87-impact-quality/source-fingerprints.json').write_text(json.dumps({str(p):hashlib.sha256(p.read_bytes()).hexdigest() for p in files},indent=2)+'\n')
flags={line.split('=',1)[0].strip():shlex.split(line.split('=',1)[1]) for line in (root/'build/CMakeFiles/judas_engine.dir/flags.make').read_text().splitlines() if line.startswith('CXX_') and '=' in line}
cc=['/usr/bin/c++']+flags['CXX_DEFINES']+flags['CXX_INCLUDES']+flags['CXX_FLAGS']
commands=[cc+['-c','/tmp/m87-impact-quality/ImpactSolver-diagnostic.cpp','-o','/tmp/m87-impact-quality/ImpactSolver-diagnostic.o'],cc+['-c','/tmp/m87-impact-quality/live-diagnostic.cpp','-o','/tmp/m87-impact-quality/live-diagnostic.o']]
link=shlex.split(Path('/tmp/m87-live-diagnostics/build.sh').read_text().splitlines()[-1])
link=[x.replace('/tmp/m87-live-diagnostics/live.o','/tmp/m87-impact-quality/live-diagnostic.o').replace('/tmp/m87-live-diagnostics/live','/tmp/m87-impact-quality/live-diagnostic') for x in link]
link.insert(2,'/tmp/m87-impact-quality/ImpactSolver-diagnostic.o')
commands.append(link)
Path('/tmp/m87-impact-quality/build-commands.json').write_text(json.dumps(commands,indent=2)+'\n')
for index,cmd in enumerate(commands):
 print('BUILD',index,shlex.join(cmd),flush=True)
 subprocess.run(cmd,cwd=root/'build',check=True)
print('Built isolated diagnostic helper; no workload executed',flush=True)
