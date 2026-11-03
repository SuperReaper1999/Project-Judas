#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <limits>
#include <sstream>
#include <string>
#include <vector>

struct Particle {double m,x,y,z;};
int main(int argc,char**argv) {
    if(argc!=6)return 2;
    std::ifstream input(argv[1]);
    const double cx=std::stod(argv[2]),cy=std::stod(argv[3]),cz=std::stod(argv[4]);
    const double dx=std::stod(argv[5]);
    const double reach=.42;
    std::vector<Particle> all;
    std::string row;std::getline(input,row);
    double poolMass=0, poolMassY=0;
    while(std::getline(input,row)) {
        std::replace(row.begin(),row.end(),',',' ');
        std::istringstream s(row); Particle p;std::size_t id;
        if(!(s>>id>>p.m>>p.x>>p.y>>p.z))return 1;
        poolMass+=p.m;poolMassY+=p.m*p.y;
        all.push_back(p);
    }
    double localMass=0,capsuleMass=0,low=100,high=-100,minimumLateral=100;
    std::vector<Particle> local;
    std::array<std::size_t,9> localCenters{};
    for(auto p:all) {
        p.x-=cx;p.y-=cy;p.z-=cz;
        const double lateral2=p.x*p.x+p.z*p.z;
        if(std::abs(p.x)<=.3 && std::abs(p.z)<=.3) {
            low=std::min(low,p.y);high=std::max(high,p.y);
            if(std::abs(p.y)<.9) {
                localMass+=p.m;
                ++localCenters[static_cast<std::size_t>((p.y+.9)/.2)];
            }
        }
        if(lateral2+std::pow(std::max(0.,std::abs(p.y)-.6),2)<=.09)capsuleMass+=p.m;
        if(std::abs(p.y)<.9)minimumLateral=std::min(minimumLateral,std::sqrt(lateral2));
        if(std::abs(p.x)<.3+reach && std::abs(p.y)<.9+reach && std::abs(p.z)<.3+reach)
            local.push_back(p);
    }
    std::cout<<std::setprecision(9)<<"mass="<<poolMass<<" particle_count="<<all.size()<<" centroid_y="<<poolMassY/poolMass
             <<" local_box_mass="<<localMass<<" capsule_mass="<<capsuleMass<<" box_target_mass=648"
             <<" local_y_extent="<<cy+low<<","<<cy+high<<" closest_horizontal="<<minimumLateral<<'\n';
    struct Coverage {
        std::size_t n=0,cube=0,sphere=0;
        double density=0,minDensity=std::numeric_limits<double>::infinity(),nearest=0,maxNearest=0;
        void Add(double cheb,double distance,double kernelDensity) {
            ++n;cube+=cheb<=.1;sphere+=distance<=.2*std::cbrt(3/(4*std::acos(-1.)));
            density+=kernelDensity;minDensity=std::min(minDensity,kernelDensity);
            nearest+=distance;maxNearest=std::max(maxNearest,distance);
        }
        void Print()const {
            std::cout<<' '<<n<<' '<<double(cube)/n<<' '<<double(sphere)/n<<' '
                     <<density/n<<' '<<minDensity<<' '<<nearest/n<<' '<<maxNearest;
        }
    };
    Coverage box,cap;std::array<Coverage,9> layers{};
    const double h2=reach*reach, poly6=315/(64*std::acos(-1.)*std::pow(reach,9));
    for(int y=0;y<std::lround(1.8/dx);++y) {
        const double yy=-.9+(y+.5)*dx;
        for(int z=0;z<std::lround(.6/dx);++z) for(int x=0;x<std::lround(.6/dx);++x) {
            const double xx=-.3+(x+.5)*dx,zz=-.3+(z+.5)*dx;
            double nearest2=1, cheb=1, density=0;
            for(const auto& p:local) {
                const double ax=std::abs(p.x-xx),ay=std::abs(p.y-yy),az=std::abs(p.z-zz);
                const double r2=ax*ax+ay*ay+az*az;
                nearest2=std::min(nearest2,r2);cheb=std::min(cheb,std::max({ax,ay,az}));
                if(r2<h2){const double d=h2-r2;density+=p.m*d*d*d;}
            }
            density*=poly6;
            const double nearest=std::sqrt(nearest2);
            box.Add(cheb,nearest,density);
            layers[static_cast<std::size_t>((yy+.9)/.2)].Add(cheb,nearest,density);
            if(xx*xx+zz*zz+std::pow(std::max(0.,std::abs(yy)-.6),2)<=.09)
                cap.Add(cheb,nearest,density);
        }
    }
    std::vector<double> neighbors, densityAtCenters;
    std::size_t closerQuarterSpacing=0,closerHundredthSpacing=0;
    for(const auto& p:local) {
        if(std::abs(p.x)>.3 || std::abs(p.y)>.9 || std::abs(p.z)>.3)continue;
        double d2=1,density=0;
        for(const auto& q:local) {
            const double x=p.x-q.x,y=p.y-q.y,z=p.z-q.z;
            const double r2=x*x+y*y+z*z;
            if(r2<h2){const double d=h2-r2;density+=q.m*d*d*d;}
            if(&p!=&q)d2=std::min(d2,r2);
        }
        const double d=std::sqrt(d2);
        neighbors.push_back(d);densityAtCenters.push_back(poly6*density);
        closerQuarterSpacing+=d<.05;closerHundredthSpacing+=d<.002;
    }
    std::sort(neighbors.begin(),neighbors.end());std::sort(densityAtCenters.begin(),densityAtCenters.end());
    std::cout<<"local_nn_min/median/max="<<neighbors.front()<<','<<neighbors[neighbors.size()/2]<<','<<neighbors.back()
             <<" local_center_density_min/median/max="<<densityAtCenters.front()<<','<<densityAtCenters[densityAtCenters.size()/2]<<','<<densityAtCenters.back()
             <<" local_nn_below_.05="<<closerQuarterSpacing<<'/'<<neighbors.size()
             <<" local_nn_below_.002="<<closerHundredthSpacing<<'/'<<neighbors.size()<<'\n';
    std::cout<<"volume_or_slice_y_center sample_count cube_fraction equal_volume_sphere_fraction mean_kernel_density min_kernel_density nearest_avg nearest_max\nbox";
    box.Print();std::cout<<"\ncapsule";cap.Print();std::cout<<'\n';
    for(std::size_t i=0;i<layers.size();++i) {
        std::cout<<cy-.8+.2*i;layers[i].Print();std::cout<<" particle_centers="<<localCenters[i]<<'\n';
    }
}
