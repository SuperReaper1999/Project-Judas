"""Run the existing conservative-volume prototype on the deep-pool dimensions."""
from pathlib import Path
import subprocess

work=Path(__file__).resolve().parent
original=work.parent/"fluid-volume-grid-20261102/volume_grid.cpp"
source=original.read_text()
source=source[:source.index("int main(int argc,char** argv)")]
source+="""
int main() {
    Grid basin(11,30,11,.2);
    constexpr double height=20.0/(2.2*2.2);
    const auto initialize=[&](bool tilted) {
        std::fill(basin.water.begin(),basin.water.end(),0);
        for(int k=0;k<basin.nz;++k)for(int j=0;j<basin.ny;++j)for(int i=0;i<basin.nx;++i) {
            const double displacement=tilted ? .2*(i-5)/5.0 : 0.;
            basin.water[basin.Cell(i,j,k)]=std::clamp((height+displacement-j*basin.h)/basin.h,0.,1.);
        }
        for(auto& u:basin.velocity)std::fill(u.begin(),u.end(),0.);
    };
    const auto wetMoment=[&]() {
        // Piecewise horizontal partial fill used for the independent rest
        // comparison. This is a diagnostic, not an added transport operator.
        double volume=0,firstMoment=0;
        for(int k=0;k<basin.nz;++k)for(int j=0;j<basin.ny;++j)for(int i=0;i<basin.nx;++i) {
            const double fill=basin.water[basin.Cell(i,j,k)];
            const double amount=fill*basin.h*basin.h*basin.h;
            volume+=amount;
            firstMoment+=amount*(j+.5*fill)*basin.h;
        }
        return firstMoment/volume;
    };
    const auto peak=[&]() {
        double speed=0;
        for(const auto& u:basin.velocity)for(double v:u)speed=std::max(speed,std::abs(v));
        return speed;
    };
    bool pass=true;
    for(const bool tilted:{false,true}) {
        initialize(tilted);
        std::vector<double> timing;
        double maxVolumeError=0,maxSpeed=0,maxDiv=0;
        for(int step=1;step<=120;++step) {
            std::string error;
            if(!basin.Step(1.0/30,{0,-9.81,0},error)) {
                std::printf("FAIL tilted=%d step=%d reason=%s\\n",tilted,step,error.c_str());
                pass=false;
                break;
            }
            timing.push_back(basin.stats.total);
            maxVolumeError=std::max(maxVolumeError,std::abs(basin.Volume()-20));
            maxSpeed=std::max(maxSpeed,peak());
            maxDiv=std::max(maxDiv,basin.stats.maxDiv);
            std::printf("%d,%d,%.17g,%.17g,%.17g,%.17g,%.9g\\n",tilted,step,basin.Volume(),
                        wetMoment(),peak(),basin.stats.maxDiv,basin.stats.total);
        }
        if(timing.empty())continue;
        const double momentError=std::abs(wetMoment()-height/2);
        const double centerWet=basin.Sample(basin.water,{1.1,wetMoment(),1.1});
        const bool observed=timing.size()==120 && maxVolumeError<1e-8 &&
            (tilted || (momentError<1e-7 && maxSpeed<2e-8 && centerWet==1));
        std::printf("SUMMARY tilted=%d pass=%d steps=%zu volume_error=%.17g com=%.17g moment_error=%.17g "
                    "max_speed=%.17g max_divergence=%.17g center_wet=%.17g median_ms=%.9g p95_ms=%.9g worst_ms=%.9g\\n",
                    tilted,observed,timing.size(),maxVolumeError,wetMoment(),momentError,maxSpeed,maxDiv,
                    centerWet,Percentile(timing,.5),Percentile(timing,.95),Percentile(timing,1));
        pass=pass&&observed;
    }
    return pass?0:1;
}
"""
path=work/"grid-deep-pool.cpp"
path.write_text(source)
exe=work/"grid-deep-pool"
subprocess.run(["c++","-O3","-DNDEBUG","-Wall","-Wextra",str(path),"-o",str(exe)],check=True)
with (work/"grid-deep-pool.log").open("w") as log:
    result=subprocess.run([str(exe)],stdout=log,stderr=subprocess.STDOUT)
print(f"Prototype exited {result.returncode}")
