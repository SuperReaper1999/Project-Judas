// Included by Renderer.cpp. Weather owns no physics, input, clock, or GL context.
// Every GPU position is camera-relative; the prescribed absolute field is reduced
// in binary64 before upload, so an M85 chart shift does not move the weather.
namespace {
const char* kWeatherFullscreen=R"GLSL(#version 330 core
out vec2 uv;
void main(){vec2 p=vec2((gl_VertexID<<1)&2,gl_VertexID&2);uv=p;gl_Position=vec4(p*2.0-1.0,0,1);}
)GLSL";
const char* kWeatherCloudFragment=R"GLSL(#version 330 core
in vec2 uv;out vec4 result;
uniform sampler2D sceneDepth;
uniform sampler3D noiseVolume;
uniform mat4 inverseProjection,inverseView;
uniform vec3 eye,up,north,east,wind,noisePhase,sun,sunColour,tornado,tornadoUp;
uniform vec4 layer,amounts; // base, thickness, camera altitude, planet radius; coverage,density,rain,snow
uniform vec3 funnel; // strength, radius, height
uniform float fog,flash,time;
uniform int steps;
uniform bool radial;
float noise3(vec3 p){return textureLod(noiseVolume,p/64.0,0.0).r;}
float cloudNoise(vec3 p,float cell,float footprint){
 float lod=clamp(log2(max(1.0,footprint/cell)),0.0,6.0);
 return textureLod(noiseVolume,p/(64.0*cell),lod).r;
}
float altitude(vec3 p){
 if(!radial)return layer.z+dot(p,up);
 // (|r+p|^2-R^2)/(|r+p|+R), avoiding Earth-radius subtraction.
 float r=layer.w+layer.z;
 return (dot(p,p)+2.0*r*dot(p,up)+layer.z*(r+layer.w))/(length(p+up*r)+layer.w);
}
float density(vec3 p,bool detail,float footprint){
 float y=(altitude(p)-layer.x)/layer.y;
 if(y<=0.0||y>=1.0)return 0.0;
 vec3 q=noisePhase+p;
 float weather=cloudNoise(q,4096.0,footprint)*.65+cloudNoise(q,1024.0,footprint)*.35;
 float shape=cloudNoise(q,256.0,footprint)*.62+cloudNoise(q,128.0,footprint)*.25;
 if(detail)shape+=cloudNoise(q,64.0,footprint)*.13;else shape+=.065;
 float cover=amounts.x;
 float d=max(0.0,shape+weather*.35-(.99-cover*.68));
 // Once a pixel spans entire small clouds, their sampled shape is replaced
 // continuously by a filtered regional weather cover, not aliased detail or
 // the constant average of the noise volume's last mip.
 float distant=smoothstep(256.0,2048.0,footprint);
 if(distant>0.0){
  float regional=cloudNoise(q,65536.0,footprint)*.65+cloudNoise(q,16384.0,footprint)*.35;
  regional=clamp((regional-.22)*1.9,0.0,1.0);
  float coverMask=smoothstep(1.0-cover-.12,1.0-cover+.12,regional);
  d=mix(d,coverMask*.32,distant);
 }
 float bottom=smoothstep(0.0,.16,y),top=1.0-smoothstep(.5,1.0,y);
 return d*bottom*top*amounts.y*2.8;
}
vec2 sphere(vec3 ray,float height){
 float r=layer.w+layer.z,b=dot(up,ray)*r;
 float c=(layer.z-height)*(2.0*layer.w+layer.z+height),disc=b*b-c;
 if(disc<=0.0)return vec2(1,-1);
 float root=sqrt(disc),q=-b-(b>=0.0?root:-root);
 if(abs(q)<1e-5)return vec2(-b-root,-b+root);
 float a=q,z=c/q;return vec2(min(a,z),max(a,z));
}
vec2 cloudInterval(vec3 ray){
 if(radial){
  vec2 outer=sphere(ray,layer.x+layer.y);
  if(outer.y<=0.0)return vec2(1,-1);
  float a=max(0.0,outer.x),b=outer.y;vec2 inner=sphere(ray,layer.x);
  if(layer.z<layer.x){if(inner.y>0.0)a=max(a,inner.y);}
  else if(inner.x>0.0)b=min(b,inner.x);
  vec2 ground=sphere(ray,0.0);
  if(ground.x>0.0)b=min(b,ground.x);
  return vec2(a,b);
 }
 float vertical=dot(ray,up);
 if(abs(vertical)<1e-5)return layer.z>=layer.x&&layer.z<=layer.x+layer.y?vec2(0,100000):vec2(1,-1);
 float a=(layer.x-layer.z)/vertical,b=(layer.x+layer.y-layer.z)/vertical;
 return vec2(max(0.0,min(a,b)),max(a,b));
}
vec3 rayDirection(){vec4 p=inverseProjection*vec4(uv*2.0-1.0,1,1);return normalize(mat3(inverseView)*p.xyz);}
float opaqueDistance(vec3 ray){
 // Low-resolution ray centres lie BETWEEN full-resolution depth texel
 // centres. Nearest depth would therefore borrow a neighbouring ray: at
 // orbital range its half-pixel surface gradient can exceed the cloud layer.
 // Projected depth is affine over a triangle, so interpolate it at THIS ray.
 ivec2 size=textureSize(sceneDepth,0);vec2 pixel=uv*vec2(size)-.5;
 ivec2 base=ivec2(floor(pixel));vec2 f=fract(pixel);
 float a=texelFetch(sceneDepth,clamp(base,ivec2(0),size-1),0).r;
 float b=texelFetch(sceneDepth,clamp(base+ivec2(1,0),ivec2(0),size-1),0).r;
 float c=texelFetch(sceneDepth,clamp(base+ivec2(0,1),ivec2(0),size-1),0).r;
 float d=texelFetch(sceneDepth,clamp(base+ivec2(1,1),ivec2(0),size-1),0).r;
 float closest=min(min(a,b),min(c,d)),farthest=max(max(a,b),max(c,d));
 if(closest>=1.0)return 1e8;
 float depth=mix(mix(a,b,f.x),mix(c,d,f.x),f.y);
 // Do not interpolate a roof/wall into the distant background. A genuine
 // depth discontinuity conservatively uses its nearest occluder instead.
 vec4 front=inverseProjection*vec4(uv*2.0-1.0,closest*2.0-1.0,1);
 vec4 back=inverseProjection*vec4(uv*2.0-1.0,farthest*2.0-1.0,1);
 if(farthest>=1.0||abs(front.w)>abs(back.w)*1.1)depth=closest;
 vec4 p=inverseProjection*vec4(uv*2.0-1.0,depth*2.0-1.0,1);
 return max(0.0,dot(mat3(inverseView)*(p.xyz/p.w),ray));
}
float funnelDensity(vec3 p){
 vec3 d=p-tornado;float h=dot(d,tornadoUp)/max(1.0,funnel.z);
 if(h<0.0||h>1.15)return 0.0;
 vec3 r=d-tornadoUp*dot(d,tornadoUp);
 vec3 right=normalize(abs(tornadoUp.x)<.8?cross(tornadoUp,vec3(1,0,0)):cross(tornadoUp,vec3(0,0,1)));
 vec3 forward=cross(right,tornadoUp);
 float radius=funnel.y*(.14+.86*pow(h,1.3));
 vec2 sway=vec2(sin(h*6.0+time*.7),cos(h*4.0-time*.4))*funnel.y*h*.18;
 vec2 xy=vec2(dot(r,right),dot(r,forward))-sway;
 float angle=atan(xy.y,xy.x)+time*3.2-h*14.0;
 float ribs=.8+.2*sin(angle*7.0+length(xy)*.17);
 float n=.55+.45*noise3(vec3(xy*.08,h*30.0+time));
 float core=(1.0-smoothstep(.48,1.1,length(xy)/max(radius,1.0)))*ribs*n*(1.0-smoothstep(.86,1.07,h));
 float dust=(1.0-smoothstep(.35,1.8,length(xy)/funnel.y))*(1.0-smoothstep(0.0,.055,h))*.4*n;
 float top=(h-.96)*funnel.z/max(1.0,funnel.y*.9);
 float crown=exp(-top*top)*(1.0-smoothstep(.45,3.0,length(xy)/funnel.y))*.38*n;
 return max(max(core,dust),crown)*funnel.x*1.3;
}
vec2 funnelInterval(vec3 ray){
 vec3 o=-tornado;float oy=dot(o,tornadoUp),dy=dot(ray,tornadoUp);
 vec3 op=o-tornadoUp*oy,dp=ray-tornadoUp*dy;
 float radius=funnel.y*3.0,a=dot(dp,dp),b=dot(op,dp),c=dot(op,op)-radius*radius;
 vec2 interval=vec2(0,1e8);
 if(a<1e-7){if(c>0.0)return vec2(1,-1);}
 else{float disc=b*b-a*c;if(disc<=0.0)return vec2(1,-1);float root=sqrt(disc);interval=vec2((-b-root)/a,(-b+root)/a);}
 if(abs(dy)<1e-7){if(oy<0.0||oy>funnel.z*1.15)return vec2(1,-1);}
 else{float x=-oy/dy,y=(funnel.z*1.15-oy)/dy;interval.x=max(interval.x,min(x,y));interval.y=min(interval.y,max(x,y));}
 return interval;
}
void main(){
 vec3 ray=rayDirection();float pixelCone=max(length(dFdx(ray)),length(dFdy(ray)));
 float limit=opaqueDistance(ray),transmission=1.0;vec3 colour=vec3(0);
 vec4 nearPoint=inverseProjection*vec4(uv*2.0-1.0,-1,1);
 float nearDistance=max(0.0,dot(mat3(inverseView)*(nearPoint.xyz/nearPoint.w),ray));
 vec2 interval=cloudInterval(ray);interval.x=max(interval.x,nearDistance);interval.y=min(min(interval.y,limit),interval.x+150000.0);
 if(amounts.x>.002&&interval.y>interval.x){
  float stride=(interval.y-interval.x)/float(steps);
  // Fixed spatial dither: no temporal history or cross-camera reprojection.
  float jitter=fract(sin(dot(floor(gl_FragCoord.xy),vec2(12.9898,78.233)))*43758.5453);
  for(int i=0;i<64;++i){if(i>=steps||transmission<.015)break;
   float t=interval.x+(float(i)+jitter)*stride;vec3 p=ray*t;
   float footprint=max(1.0,t*pixelCone);float d=density(p,true,footprint);if(d<.002)continue;
   float shadow=0.0;
   for(int j=1;j<=3;++j)shadow+=density(p+sun*(float(j)*layer.y*.18),false,footprint);
   float direct=exp(-shadow*1.35);
   float phase=.55+.45*pow(max(dot(ray,sun),0.0),10.0);
   float storm=clamp(amounts.z*.65+funnel.x*.2,0.0,.8);
   vec3 illumination=mix(vec3(.38,.46,.57),vec3(.1,.13,.18),storm);
   illumination+=sunColour*(.35+phase*.9)*direct;
   illumination+=vec3(.9,1.05,1.3)*flash;
   float opacity=1.0-exp(-d*stride*.0038);
   colour+=transmission*opacity*illumination;transmission*=1.0-opacity;
  }
 }
 if(funnel.x>.001){
  vec3 centre=tornado+tornadoUp*funnel.z*.5;vec2 fi=funnelInterval(ray);
  float a=max(nearDistance,fi.x),end=min(limit,fi.y);
   if(end>a){vec3 fc=vec3(0);float ft=1.0;int count=int(clamp(ceil((end-a)/max(1.0,funnel.y*.07)),24.0,64.0));float dt=(end-a)/float(count);
    for(int i=0;i<64;++i){if(i>=count)break;vec3 p=ray*(a+(float(i)+.5)*dt);float d=funnelDensity(p);float opacity=1.0-exp(-d*dt*.3);
     vec3 normal=(p-centre)/max(.001,length(p-centre));
     vec3 lit=vec3(.085,.10,.12)+vec3(.10,.095,.08)*max(dot(normal,sun),0.0)+flash*vec3(.8,.9,1.0);
     fc+=ft*opacity*lit;ft*=1.0-opacity;if(ft<.015)break;
    }
    // The funnel is below its cloud layer, therefore in front of that layer.
    if(interval.x<a&&interval.y>interval.x)colour+=transmission*fc;else colour=fc+colour*ft;
    transmission*=ft;
   }
 }
 result=vec4(colour,1.0-transmission);
}
)GLSL";
const char* kWeatherCompositeFragment=R"GLSL(#version 330 core
in vec2 uv;out vec4 result;
uniform sampler2D clouds,sceneDepth;
uniform mat4 inverseProjection,inverseView;
uniform vec3 up,north,east,sun,sunColour,tornado,tornadoUp,lightning;
uniform vec4 layer,amounts;
uniform float fog,flash,time;
uniform bool radial,modern;
vec3 rayDirection(){vec4 p=inverseProjection*vec4(uv*2.0-1.0,1,1);return normalize(mat3(inverseView)*p.xyz);}
float distanceAt(vec2 at){float depth=texture(sceneDepth,at).r;if(depth>=1.0)return 1e8;vec4 p=inverseProjection*vec4(at*2.0-1.0,depth*2.0-1.0,1);return length(p.xyz/p.w);}
vec2 raySegment(vec3 ray,vec3 a,vec3 b){
 vec3 e=b-a;float de=dot(ray,e),da=dot(ray,a),ee=dot(e,e),ea=dot(e,a);
 float t=clamp((de*da-ea)/max(.00001,ee-de*de),0.0,1.0);
 vec3 p=a+e*t;float along=max(0.0,dot(ray,p));return vec2(length(ray*along-p),along);
}
vec3 sky(vec3 ray){
 float elevation=dot(ray,up),storm=clamp(amounts.z*.8+amounts.x*amounts.y*.35,0.0,.9);
 vec3 zenith=mix(vec3(.09,.32,.68),vec3(.08,.11,.17),storm);
 vec3 horizon=mix(vec3(.64,.78,.9),vec3(.27,.31,.36),storm);
 vec3 s=mix(horizon,zenith,smoothstep(-.05,.75,elevation));
 float mu=max(0.0,dot(ray,sun));
 s+=vec3(1.0,.8,.47)*pow(mu,160.0)*(.65-storm*.45);
 s+=sunColour*smoothstep(.9997,.99994,mu)*4.0*(1.0-storm*.9);
 if(radial){
  float radius=layer.w+layer.z,b=dot(up,ray)*radius;
  float closest=sqrt(max(0.0,radius*radius-b*b))-layer.w;
  float atmosphere=exp(-max(layer.z,0.0)/55000.0);
  if(b<0.0)atmosphere=max(atmosphere,exp(-max(closest,0.0)/16000.0)*.8);
  s*=clamp(atmosphere,0.0,1.0);
 }
 return s+flash*vec3(.4,.47,.6);
}
void main(){
 float depth=texture(sceneDepth,uv).r,dist=distanceAt(uv);vec3 ray=rayDirection();bool background=depth>=1.0;
 vec3 atmosphere=sky(ray);
 // Four depth-weighted taps keep low-resolution clouds out of near silhouettes.
 vec2 texel=1.0/vec2(textureSize(clouds,0));vec2 base=floor(uv/texel-.5)*texel+texel*.5;
 vec4 cloud=vec4(0);float sum=0.0;
 for(int y=0;y<2;++y)for(int x=0;x<2;++x){vec2 p=base+vec2(x,y)*texel;float sd=distanceAt(p);float weight=1.0/(1.0+abs(sd-dist)/max(2.0,dist*.02));vec2 w=max(vec2(0),vec2(1)-abs((p-uv)/texel));weight*=w.x*w.y;cloud+=texture(clouds,p)*weight;sum+=weight;}
 cloud=sum>.000001?cloud/sum:texture(clouds,uv);
 float density=fog*.026+amounts.z*.0008+amounts.w*.003;
 if(radial)density*=exp(-max(layer.z,0.0)/10000.0);
 float fogOpacity=background?0.0:1.0-exp(-min(dist,30000.0)*density);
 vec3 fogColour=mix(vec3(.56,.65,.72),vec3(.24,.28,.34),amounts.z*.7)+flash*vec3(.5,.55,.65);
 vec3 colour=cloud.rgb+fogColour*fogOpacity*(1.0-cloud.a);
 float opacity=cloud.a+fogOpacity*(1.0-cloud.a);
 if(background){colour=cloud.rgb+atmosphere*(1.0-cloud.a);opacity=1.0;}
 // Analytic branching lightning. Its geometry remains in the weather frame.
 if(flash>.002){
  float d=1e6;vec3 prev=lightning;float height=max(250.0,layer.x);
  for(int i=1;i<=9;++i){float f=float(i)/9.0;vec3 now=lightning+up*(f*max(250.0,layer.x));
   now+=(east*sin(float(i)*18.21)+north*cos(float(i)*9.4))*38.0*f*(1.0-f);
   vec2 hit=raySegment(ray,prev,now);if(hit.y<dist+.5)d=min(d,hit.x);prev=now;
  }
  vec3 branch=lightning+up*height*.65;
  for(int i=0;i<3;++i){vec3 next=branch+east*(23.0+float(i)*8.0)-up*height*.11+north*sin(float(i)*9.2)*10.0;vec2 hit=raySegment(ray,branch,next);if(hit.y<dist+.5)d=min(d,hit.x*1.5);branch=next;}
  float core=exp(-d*d*4.0),glow=exp(-d*.6)*.15,bolt=(core+glow)*flash;
  colour+=vec3(.7,.85,1.2)*(core*7.0+glow)*flash;opacity=max(opacity,min(1.0,bolt));
 }
 if(!modern&&opacity>.000001)colour=pow(max(colour/opacity,vec3(0)),vec3(1.0/2.2))*opacity;
 result=vec4(colour,clamp(opacity,0.0,1.0));
}
)GLSL";
const char* kWeatherPrecipitationVertex=R"GLSL(#version 330 core
uniform mat4 view,projection;
uniform vec3 eye,up,north,east,wind,particlePhase,tornado,tornadoUp;
uniform vec4 layer,amounts;
uniform vec3 funnel;
uniform float time;
uniform int mode;
uniform sampler2D shelterMap;
uniform vec3 shelterCenter,shelterUp,shelterRight,shelterForward;
uniform float shelterExtent;
uniform bool sheltered;
out vec2 texcoord;out vec3 relativePosition;out float intensity;
float hash(float n){return fract(sin(n*127.1+311.7)*43758.5453);}
void main(){
 float id=float(gl_InstanceID),h=hash(id+17.0);vec3 p;
 vec2 corners[6]=vec2[6](vec2(-1,-1),vec2(1,-1),vec2(1,1),vec2(-1,-1),vec2(1,1),vec2(-1,1));
 vec2 c=corners[gl_VertexID];texcoord=c;
 mat3 inverseView=transpose(mat3(view));vec3 right=inverseView[0],vertical=inverseView[1];
 if(mode==4){
  p=east*(hash(id+4.0)*30.0-15.0)+north*(hash(id+9.0)*30.0-15.0);
  vec3 sp=p-shelterCenter;vec2 suv=vec2(dot(sp,shelterRight),dot(sp,shelterForward))/shelterExtent+.5;
  float roof=sheltered&&all(greaterThanEqual(suv,vec2(0)))&&all(lessThanEqual(suv,vec2(1)))?texture(shelterMap,suv).r:-10000.0;
  float age=fract(time*1.6+h);p+=shelterUp*(roof-dot(sp,shelterUp)+.035);
  p+=(east*c.x+north*c.y)*(.025+age*.19);intensity=roof> -9000.0?amounts.z*(1.0-age)*.5:0.0;
 }else if(mode==3){
  float z=hash(id+32.0),angle=-time*(1.7+h*2.0)+h*6.28318-z*14.0;
  vec3 ru=normalize(abs(tornadoUp.x)<.8?cross(tornadoUp,vec3(1,0,0)):cross(tornadoUp,vec3(0,0,1))),rv=cross(ru,tornadoUp);
  float radius=funnel.y*(.25+.75*z)*(1.05+hash(id+9.0)*.4);
  p=tornado+tornadoUp*z*funnel.z*.8+(ru*cos(angle)+rv*sin(angle))*radius;
  p+=(right*c.x+vertical*c.y)*(.15+h*.65);intensity=funnel.x*.85;
 }else{
  float extent=mode==2?44.0:40.0,height=mode==2?22.0:32.0,speed=mode==2?2.6:22.0;
  vec3 location=vec3(hash(id+1.0),hash(id+4.0),hash(id+9.0));
  location.y=mod(location.y*height+particlePhase.y,height);
  float fallAge=(height-location.y)/speed;
  // Instantaneous wind bends one bounded fall trajectory; it is never
  // multiplied by the entire weather lifetime (which would amplify gusts).
  location.x=mod(location.x*extent+particlePhase.x+wind.x*fallAge,extent)-extent*.5;
  location.z=mod(location.z*extent+particlePhase.z+wind.z*fallAge,extent)-extent*.5;
  location.y-=height*.35;
  p=east*location.x+up*location.y+north*location.z;
  if(mode==2){p+=east*sin(time*1.4+h*20.0)*.45+north*cos(time+h*40.0)*.4;p+=(right*c.x+vertical*c.y)*(.045+h*.065);intensity=amounts.w*.9;}
  else{vec3 fall=normalize(-up*22.0+east*wind.x+north*wind.z);p+=right*c.x*.011+fall*c.y*(.22+h*.38);intensity=amounts.z*.42;}
 }
 relativePosition=p;gl_Position=projection*view*vec4(eye+p,1);
}
)GLSL";
const char* kWeatherPrecipitationFragment=R"GLSL(#version 330 core
in vec2 texcoord;in vec3 relativePosition;in float intensity;out vec4 result;
uniform sampler2D shelterMap,sceneDepth;
uniform vec3 shelterCenter,shelterUp,shelterRight,shelterForward;
uniform vec2 viewport;
uniform float shelterExtent,flash;
uniform bool sheltered,modern;
uniform int mode;
void main(){
 float shape=1.0;if(mode==4){shape=1.0-smoothstep(.1,.28,abs(length(texcoord)-.68));}else if(mode==2){shape=1.0-smoothstep(.35,1.0,length(texcoord));}else if(mode==1){shape=(1.0-abs(texcoord.x))*(1.0-abs(texcoord.y)*.5);}
 if(shape<=.01)discard;
 if(mode!=3&&mode!=4&&sheltered){vec3 p=relativePosition-shelterCenter;vec2 uv=vec2(dot(p,shelterRight),dot(p,shelterForward))/shelterExtent+.5;
  if(all(greaterThanEqual(uv,vec2(0)))&&all(lessThanEqual(uv,vec2(1)))){
   float roof=texture(shelterMap,uv).r;if(dot(p,shelterUp)<roof+.08)discard;
  }
 }
 // Hardware depth handles geometry; this fades intersection edges cheaply.
 float depth=texture(sceneDepth,gl_FragCoord.xy/viewport).r;
 if(gl_FragCoord.z>depth+.0000005)discard;
 vec3 colour=mode==3?vec3(.21,.17,.12):(mode==2?vec3(.85,.9,.98):vec3(.48,.65,.8));
 colour+=flash*.5;if(!modern)colour=pow(colour,vec3(1.0/2.2));
 result=vec4(colour,intensity*shape);
}
)GLSL";
}

bool Renderer::InitWeather(){
 if(m_weather.failed)return false;
 if(m_weather.clouds&&m_weather.composite&&m_weather.precipitation)return true;
 auto make=[&](const char* vs,const char* fs,GLuint& program){GLuint v=0,f=0;if(!CompileShader(GL_VERTEX_SHADER,vs,v))return false;if(!CompileShader(GL_FRAGMENT_SHADER,fs,f)){glDeleteShader(v);return false;}bool ok=LinkProgram(v,f,program);glDeleteShader(v);glDeleteShader(f);return ok;};
 if(!make(kWeatherFullscreen,kWeatherCloudFragment,m_weather.clouds)||!make(kWeatherFullscreen,kWeatherCompositeFragment,m_weather.composite)||!make(kWeatherPrecipitationVertex,kWeatherPrecipitationFragment,m_weather.precipitation)){
  m_weather.failed=true;m_weather.error="weather shader compilation failed";return false;
 }
 glGenVertexArrays(1,&m_weather.vao);
 std::vector<unsigned char> noise(64*64*64);uint32_t random=0x74925613u;
 for(auto& value:noise){random^=random<<13;random^=random>>17;random^=random<<5;value=static_cast<unsigned char>(random>>24);}
 glGenTextures(1,&m_weather.noise);glActiveTexture(GL_TEXTURE13);glBindTexture(GL_TEXTURE_3D,m_weather.noise);glBindSampler(13,0);
 glTexImage3D(GL_TEXTURE_3D,0,GL_R8,64,64,64,0,GL_RED,GL_UNSIGNED_BYTE,noise.data());
 glGenerateMipmap(GL_TEXTURE_3D);glTexParameteri(GL_TEXTURE_3D,GL_TEXTURE_MIN_FILTER,GL_LINEAR_MIPMAP_LINEAR);glTexParameteri(GL_TEXTURE_3D,GL_TEXTURE_MAG_FILTER,GL_LINEAR);
 glTexParameteri(GL_TEXTURE_3D,GL_TEXTURE_WRAP_S,GL_REPEAT);glTexParameteri(GL_TEXTURE_3D,GL_TEXTURE_WRAP_T,GL_REPEAT);glTexParameteri(GL_TEXTURE_3D,GL_TEXTURE_WRAP_R,GL_REPEAT);
 glGenTextures(1,&m_weather.shelterTexture);glActiveTexture(GL_TEXTURE3);glBindTexture(GL_TEXTURE_2D,m_weather.shelterTexture);
 glTexImage2D(GL_TEXTURE_2D,0,GL_R32F,16,16,0,GL_RED,GL_FLOAT,nullptr);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_NEAREST);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_NEAREST);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_S,GL_CLAMP_TO_EDGE);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_T,GL_CLAMP_TO_EDGE);
 glActiveTexture(GL_TEXTURE0);return true;
}
void Renderer::ClearWeather(){
 const bool wasEnabled=m_weather.enabled;
 m_weather.enabled=false;m_weather.sheltered=false;m_weather.particles=0;
 if(wasEnabled&&m_shaderProgram){glUseProgram(m_shaderProgram);glUniform1i(glGetUniformLocation(m_shaderProgram,"uWeatherEnabled"),0);}
}
void Renderer::SetWeather(const WeatherSample& sample,glm::dvec3 absoluteEye,unsigned quality){
 if(!sample.active||quality==0){ClearWeather();return;}
 m_weather.enabled=InitWeather();if(!m_weather.enabled)return;
 auto& w=m_weather;const auto& p=sample.profile;w.quality=std::min(quality,2u);w.absoluteEye=absoluteEye;w.eye=glm::vec3(glm::inverse(m_view)[3]);
 w.up=glm::vec3(sample.up);if(glm::length(w.up)<.5f)w.up={0,1,0};w.up=glm::normalize(w.up);
 glm::dvec3 north=sample.frame.north-sample.up*glm::dot(sample.frame.north,sample.up);if(glm::length(north)<.01)north=glm::cross(sample.up,glm::dvec3(1,0,0));if(glm::length(north)<.01)north=glm::cross(sample.up,glm::dvec3(0,0,1));
 w.north=glm::vec3(glm::normalize(north));w.east=glm::normalize(glm::cross(w.north,w.up));
 w.wind=glm::vec3(sample.wind);w.radial=sample.frame.radial;w.radius=float(sample.frame.radius);w.altitude=float(sample.altitude);
 w.coverage=float(p.cloudCoverage);w.density=float(p.cloudDensity);w.base=float(p.cloudBase);w.thickness=float(p.cloudThickness);w.rain=float(p.rain);w.snow=float(p.snow);w.fog=float(p.fog);w.flash=float(sample.lightningFlash);
 w.absoluteTime=sample.time;w.time=float(std::fmod(sample.time,1024.0));w.tornado=glm::vec3(sample.tornadoPosition-absoluteEye);w.tornadoUp=glm::vec3(WeatherUp(sample.frame,sample.tornadoPosition));w.tornadoStrength=float(p.tornado);w.tornadoRadius=float(p.tornadoRadius);w.tornadoHeight=float(p.tornadoHeight);w.lightning=glm::vec3(sample.lightningPosition-absoluteEye);
 const glm::dvec3 anchored=absoluteEye-sample.cloudOffset;
 // Every octave is a power-of-two fraction of this common texture period.
 // Modulo reduction therefore cannot pop the field at an absolute wrap.
 for(int i=0;i<3;++i)w.noisePhase[i]=float(std::fmod(anchored[i],4194304.0));
 // Particle positions live in a small tangent chart but are anchored to absolute
 // coordinates. Modulo is done in double before upload, never on Earth floats.
 w.particlePhase={0,0,0}; // Per-representation phases are populated at draw time.
 glUseProgram(m_shaderProgram);auto loc=[&](const char* n){return glGetUniformLocation(m_shaderProgram,n);};
 glUniform1i(loc("uWeatherEnabled"),1);glUniform3fv(loc("uWeatherEye"),1,glm::value_ptr(w.eye));glUniform3fv(loc("uWeatherUp"),1,glm::value_ptr(w.up));glUniform3fv(loc("uWeatherPhase"),1,glm::value_ptr(w.noisePhase));glUniform4f(loc("uWeatherAmounts"),w.coverage,w.density,w.rain,w.snow);glUniform4f(loc("uWeatherLayer"),w.base,w.thickness,w.altitude,w.radius);glUniform1f(loc("uWeatherFlash"),w.flash);
 glActiveTexture(GL_TEXTURE13);glBindTexture(GL_TEXTURE_3D,w.noise);glBindSampler(13,0);glActiveTexture(GL_TEXTURE0);
}
void Renderer::SetWeatherShelter(const WeatherShelterGrid& grid){
 auto& w=m_weather;w.sheltered=grid.valid;if(!grid.valid)return;
 w.shelterAbsolute=grid.center;w.shelterCenter=glm::vec3(grid.center-w.absoluteEye);w.shelterUp=grid.up;w.shelterRight=grid.right;w.shelterForward=grid.forward;w.shelterExtent=grid.extent;w.shelterHeights=grid.heights;
 if(!w.shelterTexture)return;
 glActiveTexture(GL_TEXTURE3);glBindSampler(3,0);glBindTexture(GL_TEXTURE_2D,w.shelterTexture);glTexSubImage2D(GL_TEXTURE_2D,0,0,0,16,16,GL_RED,GL_FLOAT,w.shelterHeights.data());glActiveTexture(GL_TEXTURE0);
}
bool Renderer::ResizeWeather(int width,int height,int cw,int ch){
 auto& w=m_weather;
 if(width<1||height<1||uint64_t(width)*uint64_t(height)>16777216){w.error="weather viewport exceeds the 16-megapixel scratch budget";return false;}
 auto install=[&](const WeatherGPU::Scratch& target){w.width=target.width;w.height=target.height;w.cloudWidth=target.cloudWidth;w.cloudHeight=target.cloudHeight;w.framebuffer=target.framebuffer;w.colour=target.colour;w.depth=target.depth;};
 for(auto& target:w.targets)if(target.width==width&&target.height==height&&target.cloudWidth==cw&&target.cloudHeight==ch){target.used=++w.targetClock;install(target);return true;}
 GLint draw=0,read=0;glGetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING,&draw);glGetIntegerv(GL_READ_FRAMEBUFFER_BINDING,&read);
 WeatherGPU::Scratch target;target.width=width;target.height=height;target.cloudWidth=cw;target.cloudHeight=ch;target.used=++w.targetClock;
 glGenTextures(1,&target.colour);glBindTexture(GL_TEXTURE_2D,target.colour);glTexImage2D(GL_TEXTURE_2D,0,GL_RGBA16F,cw,ch,0,GL_RGBA,GL_FLOAT,nullptr);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_LINEAR);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_LINEAR);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_S,GL_CLAMP_TO_EDGE);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_T,GL_CLAMP_TO_EDGE);
 glGenTextures(1,&target.depth);glBindTexture(GL_TEXTURE_2D,target.depth);glTexImage2D(GL_TEXTURE_2D,0,GL_DEPTH_COMPONENT24,width,height,0,GL_DEPTH_COMPONENT,GL_UNSIGNED_INT,nullptr);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_NEAREST);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_NEAREST);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_S,GL_CLAMP_TO_EDGE);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_T,GL_CLAMP_TO_EDGE);
 glGenFramebuffers(1,&target.framebuffer);glBindFramebuffer(GL_FRAMEBUFFER,target.framebuffer);glFramebufferTexture2D(GL_FRAMEBUFFER,GL_COLOR_ATTACHMENT0,GL_TEXTURE_2D,target.colour,0);
 bool ok=glCheckFramebufferStatus(GL_FRAMEBUFFER)==GL_FRAMEBUFFER_COMPLETE;
 glBindFramebuffer(GL_DRAW_FRAMEBUFFER,GLuint(draw));glBindFramebuffer(GL_READ_FRAMEBUFFER,GLuint(read));
 if(!ok){glDeleteFramebuffers(1,&target.framebuffer);glDeleteTextures(1,&target.colour);glDeleteTextures(1,&target.depth);w.error="weather framebuffer incomplete";w.failed=true;return false;}
 // Two scratch sizes cover main plus a secondary view without resizing
 // every pass. Additional dimensions evict LRU storage, never grow the cache.
 if(w.targets.size()==2){auto old=std::min_element(w.targets.begin(),w.targets.end(),[](const auto& a,const auto& b){return a.used<b.used;});glDeleteFramebuffers(1,&old->framebuffer);glDeleteTextures(1,&old->colour);glDeleteTextures(1,&old->depth);*old=target;}
 else w.targets.push_back(target);
 ++w.allocations;install(target);return true;
}
void Renderer::WeatherUniforms(GLuint program){
 auto& w=m_weather;glUseProgram(program);auto loc=[&](const char* n){return glGetUniformLocation(program,n);};
 auto vec=[&](const char* n,glm::vec3 v){glUniform3fv(loc(n),1,glm::value_ptr(v));};
 glm::mat4 ip=glm::inverse(m_projection),iv=glm::inverse(m_view);
 glUniformMatrix4fv(loc("inverseProjection"),1,GL_FALSE,glm::value_ptr(ip));glUniformMatrix4fv(loc("inverseView"),1,GL_FALSE,glm::value_ptr(iv));glUniformMatrix4fv(loc("view"),1,GL_FALSE,glm::value_ptr(m_view));glUniformMatrix4fv(loc("projection"),1,GL_FALSE,glm::value_ptr(m_projection));
 vec("eye",w.eye);vec("up",w.up);vec("north",w.north);vec("east",w.east);vec("noisePhase",w.noisePhase);vec("particlePhase",w.particlePhase);vec("sun",m_weatherSun);vec("sunColour",m_weatherSunColour);vec("tornado",w.tornado);vec("tornadoUp",w.tornadoUp);vec("lightning",w.lightning);
 // Precipitation wind is expressed in its tangent basis; cloud noise remains in
 // world axes. This projection works identically on a wall or radial surface.
 vec("wind",glm::vec3(glm::dot(w.wind,w.east),glm::dot(w.wind,w.up),glm::dot(w.wind,w.north)));
 glUniform4f(loc("layer"),w.base,w.thickness,w.altitude,w.radius);glUniform4f(loc("amounts"),w.coverage,w.density,w.rain,w.snow);glUniform3f(loc("funnel"),w.tornadoStrength,w.tornadoRadius,w.tornadoHeight);glUniform1f(loc("fog"),w.fog);glUniform1f(loc("flash"),w.flash);glUniform1f(loc("time"),w.time);glUniform1i(loc("radial"),w.radial);glUniform1i(loc("modern"),m_linearRendering);glUniform1i(loc("steps"),int(w.steps));
 glActiveTexture(GL_TEXTURE0);glBindSampler(0,0);glBindTexture(GL_TEXTURE_2D,w.depth);glUniform1i(loc("sceneDepth"),0);
 glActiveTexture(GL_TEXTURE1);glBindSampler(1,0);glBindTexture(GL_TEXTURE_3D,w.noise);glUniform1i(loc("noiseVolume"),1);
 glActiveTexture(GL_TEXTURE2);glBindSampler(2,0);glBindTexture(GL_TEXTURE_2D,w.colour);glUniform1i(loc("clouds"),2);
 glActiveTexture(GL_TEXTURE3);glBindSampler(3,0);glBindTexture(GL_TEXTURE_2D,w.shelterTexture);glUniform1i(loc("shelterMap"),3);glUniform1i(loc("sheltered"),w.sheltered);vec("shelterCenter",w.shelterCenter);vec("shelterUp",w.shelterUp);vec("shelterRight",w.shelterRight);vec("shelterForward",w.shelterForward);glUniform1f(loc("shelterExtent"),w.shelterExtent);glUniform2f(loc("viewport"),float(w.width),float(w.height));glActiveTexture(GL_TEXTURE0);
}
void Renderer::DrawWeather(bool secondary){
 auto& w=m_weather;if(!w.enabled||m_shadowPassActive)return;
 JUDAS_PROFILE_SCOPE("Weather submission");FlushMaterialBlends();
 GLint viewport[4],draw=0,read=0,program=0,vao=0;glGetIntegerv(GL_VIEWPORT,viewport);glGetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING,&draw);glGetIntegerv(GL_READ_FRAMEBUFFER_BINDING,&read);glGetIntegerv(GL_CURRENT_PROGRAM,&program);glGetIntegerv(GL_VERTEX_ARRAY_BINDING,&vao);
 const int divisor=secondary?8:(w.quality==1?4:2),cw=std::max(1,std::min(secondary?240:960,viewport[2]/divisor)),ch=std::max(1,std::min(secondary?135:540,viewport[3]/divisor));
 if(viewport[2]<1||viewport[3]<1||!ResizeWeather(viewport[2],viewport[3],cw,ch)){ClearWeather();return;}
 GLboolean depth=glIsEnabled(GL_DEPTH_TEST),blend=glIsEnabled(GL_BLEND),cull=glIsEnabled(GL_CULL_FACE),scissor=glIsEnabled(GL_SCISSOR_TEST),depthMask;glGetBooleanv(GL_DEPTH_WRITEMASK,&depthMask);
 GLint sr,dr,sa,da;glGetIntegerv(GL_BLEND_SRC_RGB,&sr);glGetIntegerv(GL_BLEND_DST_RGB,&dr);glGetIntegerv(GL_BLEND_SRC_ALPHA,&sa);glGetIntegerv(GL_BLEND_DST_ALPHA,&da);
 glActiveTexture(GL_TEXTURE0);glBindSampler(0,0);glBindTexture(GL_TEXTURE_2D,w.depth);glBindFramebuffer(GL_READ_FRAMEBUFFER,GLuint(draw));glCopyTexSubImage2D(GL_TEXTURE_2D,0,0,0,viewport[0],viewport[1],viewport[2],viewport[3]);
 glDisable(GL_DEPTH_TEST);glDisable(GL_BLEND);glDisable(GL_CULL_FACE);glDisable(GL_SCISSOR_TEST);glDepthMask(GL_FALSE);glBindVertexArray(w.vao);
 w.steps=secondary?16:(w.quality==1?28:44);w.particles=0;
 {
  RendererProfileScope gpu(*this,"Weather clouds",secondary?1:0);glBindFramebuffer(GL_DRAW_FRAMEBUFFER,w.framebuffer);glViewport(0,0,cw,ch);WeatherUniforms(w.clouds);glDrawArrays(GL_TRIANGLES,0,3);++m_stats.drawCalls;++m_stats.triangles;
 }
 glBindFramebuffer(GL_DRAW_FRAMEBUFFER,GLuint(draw));glViewport(viewport[0],viewport[1],viewport[2],viewport[3]);
 {
  RendererProfileScope gpu(*this,"Weather sky fog",secondary?1:0);WeatherUniforms(w.composite);glEnable(GL_BLEND);glBlendFunc(GL_ONE,GL_ONE_MINUS_SRC_ALPHA);glDrawArrays(GL_TRIANGLES,0,3);++m_stats.drawCalls;++m_stats.triangles;
 }
 if(!secondary&&(w.rain>.001f||w.snow>.001f||w.tornadoStrength>.001f)&&w.altitude<15000){
  RendererProfileScope gpu(*this,"Weather precipitation");WeatherUniforms(w.precipitation);glEnable(GL_DEPTH_TEST);glBlendFunc(GL_SRC_ALPHA,GL_ONE_MINUS_SRC_ALPHA);
  auto particles=[&](int mode,unsigned count){
   const double extent=mode==2?44.:40.,height=mode==2?22.:32.,speed=mode==2?2.6:22.;
   glm::vec3 phase(float(-std::fmod(glm::dot(w.absoluteEye,glm::dvec3(w.east)),extent)),float(std::fmod(-glm::dot(w.absoluteEye,glm::dvec3(w.up))-speed*w.absoluteTime,height)),float(-std::fmod(glm::dot(w.absoluteEye,glm::dvec3(w.north)),extent)));
   glUniform3fv(glGetUniformLocation(w.precipitation,"particlePhase"),1,glm::value_ptr(phase));glUniform1i(glGetUniformLocation(w.precipitation,"mode"),mode);glDrawArraysInstanced(GL_TRIANGLES,0,6,GLsizei(count));w.particles+=count;++m_stats.drawCalls;m_stats.triangles+=count*2;
  };
  if(w.rain>.001f)particles(1,unsigned((w.quality==1?3500:7000)*w.rain));
  if(w.snow>.001f)particles(2,unsigned((w.quality==1?1800:4000)*w.snow));
  if(w.rain>.001f&&w.sheltered)particles(4,w.quality==1?140:280);
  if(w.tornadoStrength>.001f)particles(3,w.quality==1?180:360);
 }
 glDepthMask(depthMask);if(depth)glEnable(GL_DEPTH_TEST);else glDisable(GL_DEPTH_TEST);if(blend)glEnable(GL_BLEND);else glDisable(GL_BLEND);if(cull)glEnable(GL_CULL_FACE);else glDisable(GL_CULL_FACE);if(scissor)glEnable(GL_SCISSOR_TEST);else glDisable(GL_SCISSOR_TEST);glBlendFuncSeparate(sr,dr,sa,da);
 glBindFramebuffer(GL_READ_FRAMEBUFFER,GLuint(read));glBindFramebuffer(GL_DRAW_FRAMEBUFFER,GLuint(draw));glViewport(viewport[0],viewport[1],viewport[2],viewport[3]);glBindVertexArray(GLuint(vao));glUseProgram(GLuint(program));glActiveTexture(GL_TEXTURE0);
}
Renderer::WeatherDiagnostics Renderer::WeatherStats()const{
 const auto& w=m_weather;WeatherDiagnostics d;d.enabled=w.enabled;d.ready=w.clouds&&!w.failed;d.error=w.error;d.cloudWidth=unsigned(w.cloudWidth);d.cloudHeight=unsigned(w.cloudHeight);d.marchSteps=w.steps;d.particles=w.particles;d.allocations=w.allocations;d.cachedTargets=unsigned(w.targets.size());
 d.bytes=(w.noise?299593:0)+(w.shelterTexture?16*16*4:0);for(const auto& t:w.targets)d.bytes+=size_t(t.width)*t.height*4+size_t(t.cloudWidth)*t.cloudHeight*8;return d;
}
void Renderer::ShutdownWeather(){
 auto& w=m_weather;for(auto program:{w.clouds,w.composite,w.precipitation})if(program)glDeleteProgram(program);for(auto texture:{w.noise,w.shelterTexture})if(texture)glDeleteTextures(1,&texture);for(auto& t:w.targets){glDeleteFramebuffers(1,&t.framebuffer);glDeleteTextures(1,&t.colour);glDeleteTextures(1,&t.depth);}if(w.vao)glDeleteVertexArrays(1,&w.vao);w=WeatherGPU{};
}
