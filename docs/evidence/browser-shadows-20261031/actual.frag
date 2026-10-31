#version 300 es
precision highp float;
precision highp int;
precision highp sampler2D;
precision highp sampler3D;
precision highp sampler2DShadow;
precision highp sampler2DArray;
in vec3 vWorldNormal;
in vec4 vWorldTangent;
in vec3 vWorldPos;
in vec2 vUV;
in vec2 vUV1;



out vec4 FragColor;

uniform sampler2D uTexture;
in vec4 vTint;
uniform bool uBasicWater;
uniform vec3 uBasicWaterUp;
uniform bool uWaterEnabled;
uniform sampler2D uWaterPaths;
uniform mat4 uWaterViewProjection;
uniform mat4 uWaterInverseViewProjection;
uniform vec3 uLightDirection;  // world-space, normalized, points FROM the surface TOWARD the light
uniform vec3 uLightColor;
uniform vec3 uAmbientColor;

// Prescribed weather shares its cloud map and wind phase with the sky pass.
// This is an inexpensive projected cloud shadow and artistic surface response,
// not a replacement for authored physical materials or accumulated snow mass.
uniform bool uWeatherEnabled;
uniform sampler3D uWeatherNoise;
uniform vec3 uWeatherEye,uWeatherUp,uWeatherPhase;
uniform vec4 uWeatherAmounts; // coverage,density,rain,snow
uniform vec4 uWeatherLayer; // base,thickness,eye altitude,radial radius
uniform float uWeatherFlash;
float spotWeight(float outerCos,float innerCos,float angle){return innerCos<=outerCos+1e-7?step(outerCos,angle):smoothstep(outerCos,innerCos,angle);}
float weatherCloud(vec3 p){return texture(uWeatherNoise,p/262144.0).r*.65+texture(uWeatherNoise,p/65536.0).r*.35;}

// M92: one depth array; world-position projection selects the assigned layer.
uniform sampler2DArray uShadowMaps;
uniform mat4 uLightSpaceMatrix[9];
uniform sampler2D uLightGrid;
uniform bool uHasDynamicLights;
uniform vec4 uLightViewport;
vec4 lightTexel(int i){return texelFetch(uLightGrid,ivec2(i%1024,i/1024),0);}
ivec2 lightTile(){vec4 meta=lightTexel(0);ivec2 n=ivec2(meta.xy);ivec2 tile=clamp(ivec2((gl_FragCoord.xy-uLightViewport.xy)/uLightViewport.zw*meta.xy),ivec2(0),n-1);return ivec2(tile.y*n.x+tile.x,int(meta.z));}
// Milestone 15: samples `shadowMap` at `lightSpacePos` (already multiplied
// by that light's own view*projection in the vertex shader) and returns
// how LIT this fragment is (1.0 = fully lit, 0.0 = fully shadowed) — a
// simple 3x3 percentage-closer-filter (9 taps) for a soft, non-aliased
// edge, the minimal sensible technique this milestone's brief allows (see
// docs/ARCHITECTURE.md, "Milestone 15," for why nothing fancier was
// built). A slope-scaled bias (steeper-facing surfaces need a larger
// bias) avoids most shadow-acne self-shadowing while limiting peter-
// panning; a fragment whose projected position falls outside the shadow
// map's own [0,1] coverage (or beyond its far plane) is treated as fully
// lit — there is no occluder DATA there, not evidence of no occluder, but
// this is the same "no light reaches unclaimed space" honesty this
// engine already applies elsewhere (see docs/ARCHITECTURE.md's gravity-
// context law) rather than guessing.
float ComputeShadowFactor(int slot, vec3 worldPosition, float ndotl) {
    if(slot<0||slot>=9)return 1.0;
    vec4 lightSpacePos=uLightSpaceMatrix[slot]*vec4(worldPosition,1);
    if(lightSpacePos.w<=0.0)return 1.0;
    vec3 projCoords = lightSpacePos.xyz / lightSpacePos.w;
    projCoords = projCoords * 0.5 + 0.5;  // NDC [-1,1] -> texture/depth [0,1]

    if (projCoords.x < 0.0 || projCoords.x > 1.0 || projCoords.y < 0.0 || projCoords.y > 1.0 ||
        projCoords.z < -1.0 || projCoords.z > 1.0) {
        return 1.0;
    }

    float bias = max(0.006 * (1.0 - ndotl), 0.0015);
    float currentDepth = projCoords.z - bias;

    vec2 texelSize = 1.0 / vec2(textureSize(uShadowMaps, 0).xy);
    float litSum = 0.0;
    for (int x = -1; x <= 1; ++x) {
        for (int y = -1; y <= 1; ++y) {
            float closestDepth = texture(uShadowMaps, vec3(projCoords.xy + vec2(x, y) * texelSize,float(slot))).r;
            litSum += currentDepth <= closestDepth ? 1.0 : 0.0;
        }
    }
    return litSum / 9.0;
}


uniform int uMaterialModel,uAlphaMode;
uniform float uAlphaCutoff,uMetallic,uRoughness,uNormalStrength,uOcclusionStrength,uEmissionIntensity;
uniform bool uFlipV,uModern,uEnvironmentEnabled,uBaseLinear;
uniform vec4 uBaseFactor,uUVTransform;
uniform int uMapUVSet[5];uniform vec4 uMapUVTransform[5];uniform float uMapUVRotation[5];
vec2 mapUV(int i){vec2 p=(uMapUVSet[i]==1?vUV1:vUV)*uMapUVTransform[i].xy;float c=cos(uMapUVRotation[i]),s=sin(uMapUVRotation[i]);p=mat2(c,s,-s,c)*p+uMapUVTransform[i].zw;p=p*uUVTransform.xy+uUVTransform.zw;if(uFlipV)p.y=1.0-p.y;return p;}
uniform vec3 uEmission,uCameraPosition;
uniform sampler2D uMR,uNormal,uOcclusion,uEmissive,uEnvDiffuse,uEnvSpecular,uBRDF;
uniform bool uHasMR,uHasNormal,uHasOcclusion,uHasEmissive,uEmissiveLinear;
uniform float uEnvironmentIntensity,uEnvLevels;
uniform mat3 uEnvironmentInverse;
const float PI=3.14159265358979323846;
vec3 decodeSRGB(vec3 c){return mix(c/12.92,pow((c+0.055)/1.055,vec3(2.4)),step(vec3(0.04045),c));}
vec2 envUV(vec3 d){return vec2(atan(d.z,d.x)/(2.0*PI),acos(clamp(d.y,-1.0,1.0))/PI);}
vec3 BRDF(vec3 base,float metal,float rough,vec3 n,vec3 v,vec3 l){
 float nl=max(dot(n,l),0.0),nv=max(dot(n,v),0.0001);if(nl<=0.0)return vec3(0.0);
 vec3 h=normalize(v+l);float nh=max(dot(n,h),0.0),vh=max(dot(v,h),0.0);
 float a=max(rough*rough,0.0025),a2=a*a,den=nh*nh*(a2-1.0)+1.0;
 float D=a2/(PI*den*den);
 float visibility=0.5/max(nl*sqrt(nv*nv*(1.0-a2)+a2)+nv*sqrt(nl*nl*(1.0-a2)+a2),0.000001);
 vec3 f0=mix(vec3(0.04),base,metal),f=f0+(vec3(1.0)-f0)*pow(1.0-vh,5.0);
 return ((vec3(1.0)-f)*(1.0-metal)*base/PI+D*visibility*f)*nl;
}
vec3 pbrLight(vec3 base,float metal,float rough,vec3 n,vec3 v,vec2 uv){
 vec3 result=BRDF(base,metal,rough,n,v,uLightDirection)*uLightColor*ComputeShadowFactor(0,vWorldPos,max(dot(n,uLightDirection),0.0));
 ivec2 tileInfo=uHasDynamicLights?lightTile():ivec2(0);vec4 tileHeader=uHasDynamicLights?lightTexel(1+tileInfo.x):vec4(0);
 for(int item=0;item<int(tileHeader.y);++item){int record=tileInfo.y+int(lightTexel(int(tileHeader.x)+item).x)*4;
 vec4 lp=lightTexel(record),lc=lightTexel(record+1),ld=lightTexel(record+2),ls=lightTexel(record+3);
 vec3 lightPosition=lp.xyz,lightColor=lc.xyz,lightDirection=ld.xyz;
 float lightRange=lp.w,innerCos=ld.w,outerCos=ls.x;int isSpot=int(lc.w),shadowSlot=int(ls.y);vec3 to=lightPosition-vWorldPos;float distance=length(to);vec3 l=distance>0.00001?to/distance:n;float f=clamp(distance/max(lightRange,0.0001),0.0,1.0),w=clamp(1.0-f*f*f*f,0.0,1.0);float attenuation=w*w/(distance*distance+1.0);float spot=isSpot!=0?spotWeight(outerCos,innerCos,dot(-l,lightDirection)):1.0;float shadow=1.0;shadow=ComputeShadowFactor(shadowSlot,vWorldPos,max(dot(n,l),0.0));result+=BRDF(base,metal,rough,n,v,l)*lightColor*attenuation*spot*shadow;}
 float ao=uHasOcclusion?mix(1.0,texture(uOcclusion,mapUV(3)).r,uOcclusionStrength):1.0;
 vec3 f0=mix(vec3(0.04),base,metal);
 if(uEnvironmentEnabled){float nv=max(dot(n,v),0.0);vec3 f=f0+(max(vec3(1.0-rough),f0)-f0)*pow(1.0-nv,5.0);vec3 diffuse=texture(uEnvDiffuse,envUV(uEnvironmentInverse*n)).rgb*base*(vec3(1.0)-f)*(1.0-metal);vec3 reflection=reflect(-v,n);vec3 spec=textureLod(uEnvSpecular,envUV(uEnvironmentInverse*reflection),rough*(uEnvLevels-1.0)).rgb;vec2 brdf=texture(uBRDF,vec2(nv,rough)).rg;result+=(diffuse+spec*(f0*brdf.x+brdf.y))*uEnvironmentIntensity*ao;
 }else result+=uAmbientColor*base*(1.0-metal)*ao;
 return result;
}

void main() {
    vec2 materialUV=mapUV(0);
    vec3 normal = normalize(vWorldNormal);if(uMaterialModel!=0&&!gl_FrontFacing)normal=-normal;
    if(uMaterialModel!=0&&uHasNormal){vec3 t=vWorldTangent.xyz-normal*dot(normal,vWorldTangent.xyz);if(dot(t,t)>0.000001){t=normalize(t);vec3 b=cross(normal,t)*vWorldTangent.w;t*=uUVTransform.x<0.0?-1.0:1.0;b*=uUVTransform.y<0.0?-1.0:1.0;vec3 map=texture(uNormal,mapUV(2)).xyz*2.0-1.0;map.xy*=uNormalStrength;normal=normalize(mat3(t,b,normal)*map);}}


    float diffuseFactor = max(dot(normal, uLightDirection), 0.0);
    float dirShadow = ComputeShadowFactor(0, vWorldPos, diffuseFactor);
    vec3 lighting = uAmbientColor + uLightColor * diffuseFactor * dirShadow;

    ivec2 tileInfo=uHasDynamicLights?lightTile():ivec2(0);vec4 tileHeader=uHasDynamicLights?lightTexel(1+tileInfo.x):vec4(0);
 for(int item=0;item<int(tileHeader.y);++item){int record=tileInfo.y+int(lightTexel(int(tileHeader.x)+item).x)*4;
 vec4 lp=lightTexel(record),lc=lightTexel(record+1),ld=lightTexel(record+2),ls=lightTexel(record+3);
 vec3 lightPosition=lp.xyz,lightColor=lc.xyz,lightDirection=ld.xyz;
 float lightRange=lp.w,innerCos=ld.w,outerCos=ls.x;int isSpot=int(lc.w),shadowSlot=int(ls.y);
        vec3 toLight = lightPosition - vWorldPos;
        float distance = length(toLight);
        vec3 lightDir = distance > 1.0e-5 ? toLight / distance : vec3(0.0, 1.0, 0.0);

        // Smooth-windowed inverse-square attenuation (see Renderer.cpp,
        // SetDynamicLights, for the full derivation/citation) — genuinely
        // inverse-square close to the light, smoothly reaches exactly zero
        // at lightRange instead of a hard cliff or a never-zero
        // tail, and the "+1.0" keeps it finite as distance approaches 0.
        float rangeFraction = clamp(distance / max(lightRange, 1.0e-4), 0.0, 1.0);
        float windowed = 1.0 - rangeFraction * rangeFraction * rangeFraction * rangeFraction;
        windowed = clamp(windowed, 0.0, 1.0);
        float attenuation = (windowed * windowed) / (distance * distance + 1.0);

        float spotFactor = 1.0;
        if (isSpot != 0) {
            float cosAngle = dot(-lightDir, lightDirection);
            spotFactor = spotWeight(outerCos, innerCos, cosAngle);
        }

        float lightDiffuse = max(dot(normal, lightDir), 0.0);

        float shadow = 1.0;
        int shadowIndex = shadowSlot;
        shadow=ComputeShadowFactor(shadowIndex,vWorldPos,lightDiffuse);

        lighting += lightColor * lightDiffuse * attenuation * spotFactor * shadow;
    }

    vec4 texColor = texture(uTexture,materialUV);
    vec4 base=vec4((uMaterialModel==0||uBaseLinear)?texColor.rgb:decodeSRGB(texColor.rgb),texColor.a)*uBaseFactor*vTint;
    if(uAlphaMode==1&&base.a<uAlphaCutoff)discard;
    if(uMaterialModel==0){FragColor=vec4(lighting*base.rgb,base.a);if(uModern)FragColor.rgb=decodeSRGB(FragColor.rgb);}
    else {vec3 emission=uEmission*(uHasEmissive?(uEmissiveLinear?texture(uEmissive,mapUV(4)).rgb:decodeSRGB(texture(uEmissive,mapUV(4)).rgb)):vec3(1.0))*uEmissionIntensity;float metal=uMetallic,rough=uRoughness;if(uHasMR){vec4 mr=texture(uMR,mapUV(1));rough*=mr.g;metal*=mr.b;}vec3 v=normalize(uCameraPosition-vWorldPos);FragColor=vec4((uMaterialModel==2?base.rgb:pbrLight(base.rgb,metal,clamp(rough,0.05,1.0),normal,v,materialUV))+emission,uAlphaMode==0?1.0:base.a);}

    if(uBasicWater){
        vec3 n=normalize(gl_FrontFacing?normal:-normal);
        vec3 eye=normalize(uCameraPosition-vWorldPos);
        float fresnel=.035+.965*pow(1.0-clamp(abs(dot(n,eye)),0.0,1.0),5.0);
        vec3 sky=mix(vec3(.17,.34,.43),vec3(.57,.74,.83),clamp(dot(n,uBasicWaterUp)*.5+.5,0.0,1.0));
        float glint=pow(max(0.0,dot(reflect(-uLightDirection,n),eye)),160.0);
        vec3 water=base.rgb*(.35+uAmbientColor+.32*uLightColor*max(0.0,dot(n,uLightDirection)));
        FragColor=vec4(mix(water,sky,fresnel*.8)+uLightColor*glint*.85,
                       mix(base.a,1.0,fresnel*.65));
    }
    if (uWaterEnabled) {
        vec4 clip = uWaterViewProjection * vec4(vWorldPos,1.0);
        vec2 uv = clip.xy / clip.w * .5 + .5;
        vec2 path = texture(uWaterPaths,uv).rg;
        vec4 nearPoint = uWaterInverseViewProjection * vec4(uv*2.0-1.0,-1.0,1.0);
        float distance = length(vWorldPos-nearPoint.xyz/nearPoint.w);
        float waterLength = max(0.0,min(distance,path.y)-path.x);
        vec3 transmission = exp(-vec3(.32,.12,.075)*waterLength);
        FragColor.rgb = FragColor.rgb*transmission + vec3(.025,.18,.24)*(vec3(1.0)-transmission);
    }
    if(uWeatherEnabled){
        vec3 relative=vWorldPos-uWeatherEye;
        float altitude=uWeatherLayer.z+dot(relative,uWeatherUp);
        float sunUp=dot(uLightDirection,uWeatherUp);
        float travel=max(0.0,uWeatherLayer.x-altitude)/max(sunUp,.12);
        float map=weatherCloud(uWeatherPhase+relative+uLightDirection*min(travel,30000.0));
        float shadow=smoothstep(1.0-uWeatherAmounts.x*.82,1.0,map)*min(.72,uWeatherAmounts.y*.65);
        FragColor.rgb*=1.0-shadow;
        float facing=max(0.0,dot(normal,uWeatherUp));
        float wet=uWeatherAmounts.z*(.2+.8*facing);
        FragColor.rgb*=1.0-wet*.28;
        vec3 eyeDirection=normalize(uWeatherEye-vWorldPos);
        FragColor.rgb+=vec3(.08,.1,.12)*wet*pow(max(0.0,dot(reflect(-uLightDirection,normal),eyeDirection)),48.0);
        float snow=uWeatherAmounts.w*smoothstep(.3,.8,facing);
        FragColor.rgb=mix(FragColor.rgb,vec3(.78,.84,.9)*(.4+.6*diffuseFactor),snow*.88);
        FragColor.rgb+=vec3(.38,.43,.52)*uWeatherFlash*(.2+.8*facing);
    }
    if(uModern||uMaterialModel!=0)FragColor.rgb=clamp(FragColor.rgb,vec3(0.0),vec3(60000.0));
    if(!uModern&&uMaterialModel!=0){vec3 c=max(FragColor.rgb,vec3(0.0));if(uMaterialModel==1)c/=1.0+dot(c,vec3(.2126,.7152,.0722));FragColor.rgb=mix(c*12.92,1.055*pow(c,vec3(1.0/2.4))-0.055,step(vec3(.0031308),c));}
}
