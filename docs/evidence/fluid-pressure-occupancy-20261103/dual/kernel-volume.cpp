// Evaluate the entire reconstructed field rather than only particle centers.
#include <algorithm>
#include <cmath>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

struct Point { double x,y,z,mass; };

int main(int argc,char** argv) {
    if(argc!=3)return 2;
    std::ifstream in(argv[1]);
    if(!in)return 3;
    std::string line;
    std::getline(in,line);
    std::vector<Point> particles;
    while(std::getline(in,line)) {
        std::replace(line.begin(),line.end(),',',' ');
        std::istringstream row(line);
        Point p{};
        int id;
        row>>id>>p.mass>>p.x>>p.y>>p.z;
        if(!row)return 4;
        particles.push_back(p);
    }
    const int side=std::stoi(argv[2]);
    if(side<10 || side>150)return 2;
    const int height=std::lround(side*6/2.2);
    const double dx=2.2/side,dy=6.0/height;
    const double h=.42,h2=h*h;
    const double coefficient=315/(64*std::acos(-1.)*std::pow(h,9));
    std::vector<double> density(std::size_t(side)*side*height);
    for(const auto& p:particles) {
        const auto begin=[](double p,double dx,double h,double min) {
            return std::max(0,int(std::ceil((p-h-min)/dx-.5)));
        };
        const auto end=[](double p,double dx,double h,double min,int max) {
            return std::min(max,int(std::floor((p+h-min)/dx+.5)));
        };
        for(int j=begin(p.y,dy,h,0);j<end(p.y,dy,h,0,height);++j) {
            const double ddy=(j+.5)*dy-p.y;
            for(int k=begin(p.z,dx,h,-1.1);k<end(p.z,dx,h,-1.1,side);++k) {
                const double ddz=(k+.5)*dx-1.1-p.z;
                const double remaining=h2-ddy*ddy-ddz*ddz;
                if(remaining<=0)continue;
                for(int i=begin(p.x,dx,h,-1.1);i<end(p.x,dx,h,-1.1,side);++i) {
                    const double ddx=(i+.5)*dx-1.1-p.x;
                    const double q=remaining-ddx*ddx;
                    if(q>0)density[(std::size_t(j)*side+k)*side+i]+=coefficient*q*q*q*p.mass;
                }
            }
        }
    }
    double mass=0,first=0,aboveNominalMass=0,maximum=0;
    for(int j=0;j<height;++j)for(int k=0;k<side;++k)for(int i=0;i<side;++i) {
        const double rho=density[(std::size_t(j)*side+k)*side+i];
        const double dm=rho*dx*dx*dy;
        mass+=dm;
        first+=dm*(j+.5)*dy;
        aboveNominalMass+=std::max(0.,rho-1000.)*dx*dx*dy;
        maximum=std::max(maximum,rho);
    }
    std::cout<<std::setprecision(12)<<"lateral_samples="<<side
             <<" in_basin_mass="<<mass<<" out_of_basin_kernel_mass="<<20000-mass
             <<" in_basin_centroid="<<first/mass<<" max_field_density="<<maximum
             <<" in_basin_mass_above_nominal_local_density="<<aboveNominalMass<<'\n';
}
