from pathlib import Path
folder=Path('.cache/prealpha-liquid-rotation/capacity-investigation');s=(folder/'LiquidSurface.density.cpp').read_text()
s=s.replace('return c[0]+t*(c[1]+t*(c[2]+t*c[3]));','for(unsigned degree=3;degree>0;--degree)for(unsigned k=0;k<degree;++k)c[k]+=t*(c[k+1]-c[k]);return c[0];',1)
s=s.replace('return (c[1]+t*(2*c[2]+3*t*c[3]))/width;','std::array<double,3> d{c[1]-c[0],c[2]-c[1],c[3]-c[2]};for(unsigned degree=2;degree>0;--degree)for(unsigned k=0;k<degree;++k)d[k]+=t*(d[k+1]-d[k]);return 3*d[0]/width;',1)
s=s.replace('b.capacityPolynomials.push_back({double(cumulative),double(width*c[0]),double(width*(c[1]-c[0])),double(width*(c[0]-2*c[1]+c[2])/3)});cumulative+=width*(c[0]+c[1]+c[2])/3;', 'long double first=cumulative+width*c[0]/3,second=cumulative+width*(c[0]+c[1])/3;long double next=cumulative+width*(c[0]+c[1]+c[2])/3;b.capacityPolynomials.push_back({double(cumulative),double(first),double(second),double(next)});cumulative=next;')
(folder/'LiquidSurface.bernstein.cpp').write_text(s)
s=(folder/'runtime-density.cpp').read_text().replace('#include "LiquidSurface.density.cpp"','#include "LiquidSurface.bernstein.cpp"');(folder/'runtime-bernstein.cpp').write_text(s)
