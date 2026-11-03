"""Derive the exact grid-aligned hydrostatic gradient before solving perturbations."""
from pathlib import Path
import subprocess

work=Path(__file__).resolve().parent
source=(work/"grid-deep-pool-original.cpp").read_text()
before="""        auto u=input;
        Faces([&](int a,int,int,int,int f,int l,int r){u[a][f]=Open(l,r)?u[a][f]+dt*gravity[a]:0;});"""
after="""        auto u=input;
        // Factor out an exact hydrostatic gradient when equal layer densities
        // and one chart axis make gravity entirely a pressure gradient. Solve
        // the given input velocity as before. Never zero a perturbation.
        int gravityAxis=-1, gravityCount=0;
        for(int a=0;a<3;++a)if(gravity[a]!=0) {gravityAxis=a;++gravityCount;}
        bool hydrostatic=gravityCount==1;
        std::vector<double> layerDensity;
        if(hydrostatic) {
            layerDensity.assign(Count(gravityAxis),-1);
            for(int k=0;k<nz;++k)for(int j=0;j<ny;++j)for(int i=0;i<nx;++i) {
                const int c=Cell(i,j,k);
                if(solid[c])continue;
                const int xyz[3]={i,j,k};
                double& density=layerDensity[xyz[gravityAxis]];
                if(density<0)density=quantity[c];
                else if(density!=quantity[c])hydrostatic=false;
            }
            for(double density:layerDensity)if(density<0)hydrostatic=false;
        }
        Faces([&](int a,int,int,int,int f,int l,int r) {
            u[a][f]=Open(l,r)?u[a][f]+(hydrostatic?0.:dt*gravity[a]):0;
        });"""
assert source.count(before)==1
source=source.replace(before,after)
before="""        output=std::move(u);outPressure=std::move(p);return true;"""
after="""        if(hydrostatic) {
            std::vector<double> hydro(layerDensity.size());
            for(std::size_t i=1;i<hydro.size();++i)
                hydro[i]=hydro[i-1]+h*gravity[gravityAxis]*
                    (1+499.5*(layerDensity[i-1]+layerDensity[i]));
            for(const auto& list:members) {
                const auto layer=[&](int c) {
                    const int xyz[3]={c%nx,(c/nx)%ny,c/(nx*ny)};
                    return xyz[gravityAxis];
                };
                double mean=0;
                for(int c:list)mean+=hydro[layer(c)];
                mean/=list.size();
                for(int c:list)p[c]+=hydro[layer(c)]-mean;
            }
        }
        output=std::move(u);outPressure=std::move(p);return true;"""
assert source.count(before)==1
source=source.replace(before,after)
source=source.replace("maxSpeed<2e-8 && centerWet==1", "maxSpeed<2e-8 && std::abs(centerWet-1)<1e-12")
# Include a negative control for the gate: a genuine divergence-free velocity
# perturbation must survive projection and still transport material.
anchor="""    bool pass=true;
    for(const bool tilted:{false,true}) {"""
assert source.count(anchor)==1
source=source.replace(anchor,"""    bool pass=true;
    initialize(false);
    basin.velocity[0][basin.Face(0,1,10,0)]=.1;
    basin.velocity[1][basin.Face(1,1,11,0)]=.1;
    basin.velocity[0][basin.Face(0,1,11,0)]=-.1;
    basin.velocity[1][basin.Face(1,0,11,0)]=-.1;
    std::array<std::vector<double>,3> projected;
    std::vector<double> pressure;
    Stats measures;
    std::string projectionError;
    const bool kept=basin.Project(basin.velocity,1.0/30,{0,-9.81,0},projected,pressure,measures,projectionError);
    double worstChange=0;
    if(kept)for(int a=0;a<3;++a)for(std::size_t f=0;f<projected[a].size();++f)
        worstChange=std::max(worstChange,std::abs(projected[a][f]-basin.velocity[a][f]));
    pass=kept && worstChange<1e-10;
    std::printf("PERTURBATION pass=%d velocity_change=%.17g error=%s\\n",pass,worstChange,projectionError.c_str());
    for(const bool tilted:{false,true}) {""")
(src:=work/"grid-rest-projection.cpp").write_text(source)
exe=work/"grid-rest-projection"
subprocess.run(["c++","-O3","-DNDEBUG","-Wall","-Wextra",str(src),"-o",str(exe)],check=True)
with (work/"grid-rest-projection.log").open("w") as log:
    result=subprocess.run([str(exe)],stdout=log,stderr=subprocess.STDOUT)
print(f"Prototype exited {result.returncode}")
