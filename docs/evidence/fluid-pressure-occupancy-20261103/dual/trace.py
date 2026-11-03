"""Compare density predicted by a linear row with exact post-advection geometry."""
from pathlib import Path
import subprocess

work=Path(__file__).resolve().parent
step=work/"step.txt"
saved=step.read_text()
end="""            if(contactImpulses)for(std::size_t r=0;r<supports.size();++r) {"""
assert saved.count(end)==1
trace="""            std::vector<glm::vec3> proposed(count);
            for(std::size_t i=0;i<count;++i)proposed[i]=starts[i]+substepTime*m_particles[i].velocity;
            const auto& initialNeighbors=BuildNeighbors(starts,radius,neighborWorkspace);
            std::vector<double> predicted(count);
            for(std::size_t i=0;i<count;++i) {
                double rate=0;
                for(std::size_t j:initialNeighbors[i]) {
                    const auto gradient=(double(m_particles[j].mass)/restDensity)*
                        ExactPoly6Gradient(starts[i]-starts[j],kernel);
                    rate+=glm::dot(gradient,glm::dvec3(m_particles[i].velocity)-glm::dvec3(m_particles[j].velocity));
                }
                predicted[i]=double(DensityAt(i,starts,m_particles,initialNeighbors,kernel))/restDensity+substepTime*rate;
            }
            const auto& proposalNeighbors=BuildNeighbors(proposed,radius,neighborWorkspace);
            double maxLinearViolation=0,maxNonlinearError=0,maxSolidDensityChange=0;
            double meanLinearViolation=0,meanNonlinearError=0,meanSolidDensityChange=0;
            std::vector<double> exact(count);
            for(std::size_t i=0;i<count;++i) {
                exact[i]=double(DensityAt(i,proposed,m_particles,proposalNeighbors,kernel))/restDensity;
                const double violation=std::max(0.,predicted[i]-1);
                const double err=std::abs(exact[i]-predicted[i]);
                maxLinearViolation=std::max(maxLinearViolation,violation);
                meanLinearViolation+=violation/count;
                maxNonlinearError=std::max(maxNonlinearError,err);
                meanNonlinearError+=err/count;
            }
            const auto& finalPressureNeighbors=BuildNeighbors(positions,radius,neighborWorkspace);
            double maxExactViolation=0;
            std::size_t repairs=0;
            for(std::size_t i=0;i<count;++i) {
                const double final=double(DensityAt(i,positions,m_particles,finalPressureNeighbors,kernel))/restDensity;
                maxExactViolation=std::max(maxExactViolation,final-1);
                const double err=std::abs(final-exact[i]);
                maxSolidDensityChange=std::max(maxSolidDensityChange,err);
                meanSolidDensityChange+=err/count;
                repairs+=positions[i]!=proposed[i];
            }
            std::fprintf(stderr,"DEFECT %zu %.9g %.9g %.9g %.9g %.9g %.9g %.9g %zu\\n",
                calls,maxLinearViolation,maxNonlinearError,maxSolidDensityChange,maxExactViolation,
                meanLinearViolation,meanNonlinearError,meanSolidDensityChange,repairs);
"""
try:
    step.write_text(saved.replace(end,trace+end))
    for name in ("pgs64","pgs512"):
        subprocess.run(["python3",str(work/"build.py"),name,"half_025"],check=True)
        (work/f"{name}-half_025/warmup.log").rename(work/f"{name}-density-defect.log")
        for suffix in (".cpp",".o"):
            (work/(name+suffix)).rename(work/(name+"-trace"+suffix))
finally:
    step.write_text(saved)
