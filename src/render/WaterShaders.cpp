#include "WaterShaders.h"
namespace JudasRender::WaterShaders {
const char* const SurfaceVertex=R"GLSL(#version 330 core
layout(location=0) in vec3 position;
layout(location=1) in vec3 normal;
layout(location=2) in vec2 coordinate;
layout(location=5) in vec4 referenceData;
uniform mat4 vp;
uniform mat3 basis;
uniform vec3 origin;
uniform ivec3 cells;
uniform float spacing,seconds;
uniform sampler2D field;
out vec3 world,baseNormal;
out vec2 uv;
out float activity;
flat out int closingFace;
vec4 intervalAt(ivec3 p){int i=(p.z*cells.y+p.y)*cells.x+p.x;return texelFetch(field,ivec2(i%1024,i/1024),0);}
vec4 weights(float t){float a=1.-t;return vec4(a*a*a,3.*t*t*t-6.*t*t+4.,-3.*t*t*t+3.*t*t+3.*t+1.,t*t*t)/6.;}
vec4 derivatives(float t){return vec4(-.5*(1.-t)*(1.-t),1.5*t*t-2.*t,-1.5*t*t+t+.5,.5*t*t);}
void main(){
    uv=coordinate;float referenceHeight=referenceData.w;
    closingFace=referenceData.z>.5?1:0;
    vec2 p=uv/spacing-.5;ivec2 first=ivec2(floor(p));vec2 phase=fract(p);
    vec4 wx=weights(phase.x),wz=weights(phase.y),dx=derivatives(phase.x),dz=derivatives(phase.y);
    float referenceBottom=referenceData.y;
    float sum=0.,bottom=0.,total=0.;vec2 gradient=vec2(0),weightGradient=vec2(0);float speed=0.;
    for(int b=0;b<4;++b)for(int a=0;a<4;++a){
        ivec2 q=clamp(first+ivec2(a-1,b-1),ivec2(0),cells.xz-1);
        vec4 interval=vec4(0);float distance=1.e30;
        for(int yy=0;yy<cells.y;){
            vec4 candidate=intervalAt(ivec3(q.x,yy,q.y));
            if(candidate.z<=0.){yy=max(yy+1,int(candidate.w));continue;}
            if(candidate.x>referenceHeight)break;
            if(candidate.y>referenceBottom&&abs(candidate.y-referenceHeight)<distance){interval=candidate;distance=abs(candidate.y-referenceHeight);}
            yy=max(yy+1,int(ceil(candidate.y/spacing-.0001)));
        }
        if(interval.z<=0.)continue;
        float w=wx[a]*wz[b];vec2 d=vec2(dx[a]*wz[b],wx[a]*dz[b]);
        sum+=w*interval.y;total+=w;gradient+=d*interval.y;weightGradient+=d;speed+=w*interval.w;
        bottom+=w*interval.x;
    }
    float height=referenceHeight;
    baseNormal=normal;
    if(total>.000001){height=mix(bottom/total,sum/total,referenceData.x);gradient=(gradient*total-sum*weightGradient)/(total*total*spacing);if(referenceData.z<.5)baseNormal=normalize(basis*vec3(-gradient.x,1,-gradient.y));}
    activity=clamp(speed/max(total,.000001),0.,2.);
    float t=seconds;
    float wave=.008*sin(dot(uv,vec2(1.7,1.1))-t*1.5)+.005*sin(dot(uv,vec2(-2.3,2.8))+t*1.8);
    wave*=smoothstep(0.,.2,height-bottom/max(total,.000001));
    world=origin+basis*vec3(uv.x,height+wave*referenceData.x,uv.y);
    gl_Position=vp*vec4(world,1);
}
)GLSL";
const char* const SurfaceFragment=R"GLSL(#version 330 core
in vec3 world,baseNormal;
in vec2 uv;
in float activity;
flat in int closingFace;
out vec4 result;
uniform sampler2D sceneColour,sceneDepth;
uniform mat4 vp,inverseVP,view;
uniform mat3 basis;
uniform vec3 eye,sun,sunColour,waterColour;
uniform vec3 skyColour,ambientColour;
uniform sampler2D environmentMap;
uniform mat3 environmentInverse;
uniform bool hasEnvironment;
uniform float environmentIntensity;
uniform vec2 resolution,viewportOrigin,captureScale;
uniform float seconds,opacity;
uniform bool linearOutput;
vec3 decode(vec3 c){return mix(c/12.92,pow((c+.055)/1.055,vec3(2.4)),step(vec3(.04045),c));}
vec3 encode(vec3 c){return mix(c*12.92,1.055*pow(max(c,vec3(0)),vec3(1./2.4))-.055,step(vec3(.0031308),c));}
vec3 image(vec2 p){vec3 c=texture(sceneColour,p*captureScale).rgb;return linearOutput?c:decode(c);}
vec3 unproject(vec2 p,float depth){vec4 q=inverseVP*vec4(p*2.-1.,depth*2.-1.,1);return q.xyz/q.w;}
vec3 sky(vec3 direction){
    if(hasEnvironment){vec3 d=environmentInverse*direction;vec2 p=vec2(atan(d.z,d.x)/6.2831853,acos(clamp(d.y,-1.,1.))/3.14159265);return textureLod(environmentMap,p,.5).rgb*environmentIntensity;}
    float h=clamp(dot(direction,basis[1])*.5+.5,0.,1.);
    vec3 colour=decode(skyColour)*mix(.7,1.1,pow(h,.7));
    return colour+sunColour*pow(max(dot(direction,sun),0.),350.)*2.;
}
void main(){
    vec2 screen=(gl_FragCoord.xy-viewportOrigin)/resolution;
    if(closingFace!=0){
        float opaqueDepth=texture(sceneDepth,screen*captureScale).r;
        if(opaqueDepth<.99999){
            vec3 opaque=unproject(screen,opaqueDepth);
            // A pool wall may coincide with the finite-domain closing face.
            // Its existing opaque depth owns that interface. Reject the hidden
            // liquid face instead of depth-biasing either surface or reflecting
            // the wall back onto itself through near-identical depth samples.
            float separation=dot(opaque-world,normalize(world-eye));
            if(separation<=max(.002,length(world-eye)*.00001))discard;
        }
    }
    // Continuous analytic detail, independently sampled per fragment. Geometry
    // subdivision is therefore not the resolution of the specular highlights.
    // Instantaneous flow modulates detail strength, never accumulated phase.
    // Multiplying changing speed by absolute time would flicker in long runs.
    vec2 p=uv;
    vec2 slope=vec2(0);
    slope+=.055*cos(dot(p,vec2(3.7,2.4))-seconds*1.8)*vec2(.84,.54);
    slope+=.035*cos(dot(p,vec2(-6.1,8.4))+seconds*2.3)*vec2(-.59,.81);
    slope+=.018*cos(dot(p,vec2(14.7,9.2))-seconds*3.1)*vec2(.85,.53);
    slope+=.008*cos(dot(p,vec2(-28.1,32.8))+seconds*4.3)*vec2(-.65,.76);
    slope*=1.+min(activity,1.)*.4;
    vec3 n=normalize(baseNormal-basis[0]*slope.x-basis[2]*slope.y);
    bool below=!gl_FrontFacing;
    if(below)n=-n;
    vec3 v=normalize(eye-world);
    float nv=clamp(dot(n,v),0.,1.);
    float fresnel=.0204+.9796*pow(1.-nv,5.);
    if(below&&1.333*1.333*(1.-nv*nv)>1.)fresnel=1.;
    vec3 screenNormal=mat3(view)*n;
    vec2 refracted=clamp(screen+screenNormal.xy*.009,vec2(.002),vec2(.998));
    float surfaceDepth=gl_FragCoord.z;
    // Foreground geometry must never be pulled through the water silhouette.
    if(texture(sceneDepth,refracted*captureScale).r<surfaceDepth-.00001)refracted=screen;
    float depth=texture(sceneDepth,refracted*captureScale).r;
    vec3 behind=unproject(refracted,depth);
    float thickness=clamp(length(behind-world),0.,25.);
    vec3 absorption=vec3(.36,.115,.065)*(opacity*1.2);
    vec3 transmission=exp(-absorption*thickness);
    float lightLevel=clamp(dot(ambientColour,vec3(.2126,.7152,.0722))*1.7+dot(sunColour,vec3(.2126,.7152,.0722))*.25,.02,2.);
    vec3 scatter=mix(vec3(.012,.12,.145),decode(waterColour),.35)*lightLevel;
    vec3 transmitted=image(refracted);
    if(!below)transmitted=transmitted*transmission+scatter*(1.-transmission);
    vec3 reflectionDirection=reflect(-v,n),reflection=sky(reflectionDirection);
    // Bounded screen-space reflection; offscreen rays use the sky fallback.
    // Start beyond the local surface to avoid reflecting the same depth texel.
    for(int i=0;i<10;++i){
        float distance=.28+float(i*i)*.11;
        vec3 samplePosition=world+reflectionDirection*distance;
        vec4 clip=vp*vec4(samplePosition,1);
        if(clip.w<=0.)break;
        vec2 q=clip.xy/clip.w*.5+.5;
        if(any(lessThan(q,vec2(.005)))||any(greaterThan(q,vec2(.995))))break;
        float d=texture(sceneDepth,q*captureScale).r;
        if(d>.99999)continue;
        vec3 hit=unproject(q,d);
        float along=dot(hit-world,reflectionDirection);
        float miss=length((hit-world)-reflectionDirection*along);
        if(along>0.&&miss<.12+distance*.035&&length(hit-samplePosition)<.45){
            float edge=smoothstep(0.,.08,min(min(q.x,q.y),min(1.-q.x,1.-q.y)));
            reflection=mix(reflection,image(q),edge);break;
        }
    }
    if(below)reflection=mix(reflection,scatter,.65);
    vec3 halfVector=normalize(v+sun);
    float highlight=pow(max(dot(n,halfVector),0.),320.)*.85;
    vec3 colour=mix(transmitted,reflection,fresnel)+sunColour*highlight;
    float shore=1.-smoothstep(.015,.13,thickness);
    float foam=shore*(.5+.5*sin(uv.x*19.+sin(uv.y*17.)+seconds*2.));
    if(!below)colour=mix(colour,vec3(.55,.66,.64),foam*.45);
    colour=clamp(colour,vec3(0),vec3(60000));
    result=vec4(linearOutput?colour:encode(colour),1);
}
)GLSL";
const char* const FullscreenVertex=R"GLSL(#version 330 core
out vec2 uv;
void main(){vec2 p=vec2((gl_VertexID<<1)&2,gl_VertexID&2);uv=p;gl_Position=vec4(p*2.-1.,0,1);}
)GLSL";
const char* const UnderwaterFragment=R"GLSL(#version 330 core
in vec2 uv;out vec4 result;
uniform sampler2D sceneDepth,sceneColour;
uniform bool linearOutput;
uniform sampler2D field;
uniform mat4 inverseVP;
uniform mat3 inverseBasis;
uniform vec3 origin,extent;
uniform vec3 ambientColour,sunColour,waterColour;
uniform float opacity;
uniform ivec3 cells;
uniform float spacing,seconds;
uniform vec2 captureScale;
vec3 local(vec3 p){return inverseBasis*(p-origin);}
vec3 unproject(float depth){vec4 p=inverseVP*vec4(uv*2.-1.,depth*2.-1.,1);return p.xyz/p.w;}
vec4 intervalAt(ivec3 p){int i=(p.z*cells.y+p.y)*cells.x+p.x;return texelFetch(field,ivec2(i%1024,i/1024),0);}
float mediumDepth(vec3 p){
    if(any(lessThan(p,vec3(0)))||p.x>=extent.x||p.z>=extent.z||p.y>=extent.y+.02)return -1.;
    vec2 coordinate=p.xz/spacing-.5;
    ivec2 first=ivec2(floor(coordinate));vec2 f=fract(coordinate);
    int y=int(floor(p.y/spacing));float lower=0.,upper=0.,weight=0.;
    for(int z=0;z<2;++z)for(int x=0;x<2;++x){
        ivec2 q=clamp(first+ivec2(x,z),ivec2(0),cells.xz-1);
        vec4 interval=vec4(0);
        for(int dy=-1;dy<=1;++dy){int yy=y+dy;if(yy<0||yy>=cells.y)continue;
            vec4 candidate=intervalAt(ivec3(q.x,yy,q.y));
            if(candidate.z>0.&&p.y>=candidate.x-.01&&p.y<=candidate.y+.08){interval=candidate;break;}}
        if(interval.z<=0.)continue;
        float w=(x==0?1.-f.x:f.x)*(z==0?1.-f.y:f.y);
        lower+=w*interval.x;upper+=w*interval.y;weight+=w;
    }
    float wave=.008*sin(dot(p.xz,vec2(1.7,1.1))-seconds*1.5)+.005*sin(dot(p.xz,vec2(-2.3,2.8))+seconds*1.8);
    wave*=smoothstep(0.,.2,(upper-lower)/max(weight,.000001));
    return weight>.3?min(p.y-lower/weight,upper/weight+wave-p.y):-1.;
}
bool wet(vec3 p){return mediumDepth(p)>=0.;}
vec3 decode(vec3 c){return mix(c/12.92,pow((c+.055)/1.055,vec3(2.4)),step(vec3(.04045),c));}
vec3 encode(vec3 c){return mix(c*12.92,1.055*pow(max(c,vec3(0)),vec3(1./2.4))-.055,step(vec3(.0031308),c));}
void main(){
    vec3 background=texture(sceneColour,uv*captureScale).rgb;
    result=vec4(background,1);
    vec3 start=local(unproject(0.)),end=local(unproject(texture(sceneDepth,uv*captureScale).r));
    vec3 ray=end-start;float lengthRay=length(ray);if(lengthRay<.0001){return;}
    float limit=min(lengthRay,35.);vec3 direction=ray/lengthRay;
    // Only the medium adjacent to the near plane needs this post-process.
    // Distant submerged geometry is coloured by the refracting surface pass.
    float immersion=smoothstep(-.012,.012,mediumDepth(start));
    if(immersion<=0.)return;
    // Clip to the region before marching: an upward sky ray should integrate
    // centimetres to the surface, not span the camera's entire far plane.
    for(int axis=0;axis<3;++axis)if(abs(direction[axis])>.000001){
        float exit=((direction[axis]>0.?extent[axis]:0.)-start[axis])/direction[axis];
        limit=min(limit,max(exit,0.));
    }
    float travel=limit;
    for(int i=1;i<=12;++i){float t=limit*float(i)/12.;
        if(!wet(start+direction*t)){
            float lo=limit*float(i-1)/12.,hi=t;
            for(int j=0;j<8;++j){float mid=(lo+hi)*.5;if(wet(start+direction*mid))lo=mid;else hi=mid;}
            travel=(lo+hi)*.5;break;
        }
    }
    // A subtle moving depth-space caustic cue belongs only to visible submerged
    // geometry. It is visual light redistribution, never a physical force.
    float caustic=0.;
    if(texture(sceneDepth,uv*captureScale).r<.9999){
        vec2 p=end.xz*3.2;
        float a=sin(p.x+sin(p.y*1.31+seconds*.8));
        float b=cos(p.y+sin(p.x*1.1-seconds*.5));
        caustic=pow(max(0.,1.-abs(a+b)*.5),9.)*exp(-travel*.25);
    }
    vec3 transmission=exp(-vec3(.38,.15,.085)*travel*(opacity*1.2));
    vec3 source=linearOutput?background:decode(background);
    float lightLevel=clamp(dot(ambientColour,vec3(.2126,.7152,.0722))*1.7+dot(sunColour,vec3(.2126,.7152,.0722))*.25,.02,2.);
    vec3 scatter=mix(vec3(.012,.12,.145),decode(waterColour),.35)*lightLevel;
    vec3 filtered=source*transmission+scatter*(1.-transmission);
    filtered+=vec3(.014,.025,.022)*caustic;
    vec3 colour=mix(source,filtered,immersion);
    result=vec4(linearOutput?colour:encode(colour),1);
}
)GLSL";
}
