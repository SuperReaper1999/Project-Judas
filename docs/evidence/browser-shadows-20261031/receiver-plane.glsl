float ComputeShadowFactor(int slot, vec3 worldPosition, float ndotl) {
    if(slot<0||slot>=9)return 1.0;
    vec4 clip=uLightSpaceMatrix[slot]*vec4(worldPosition,1.0);
    vec3 projected=(clip.xyz/clip.w)*0.5+0.5;
    vec3 dx=dFdx(projected),dy=dFdy(projected);
    float determinant=dx.x*dy.y-dx.y*dy.x;
    vec2 slope=determinant!=0.0?vec2(dx.z*dy.y-dy.z*dx.y,dx.x*dy.z-dy.x*dx.z)/determinant:vec2(0.0);
    if(clip.w<=0.0||any(lessThan(projected,vec3(0.0)))||any(greaterThan(projected,vec3(1.0))))return 1.0;
    ivec2 size=textureSize(uShadowMaps,0).xy;
    ivec2 center=ivec2(floor(projected.xy*vec2(size)));
    float bias=max(0.006*(1.0-ndotl),0.0015),lit=0.0;
    for(int x=-1;x<=1;++x)for(int y=-1;y<=1;++y){
        ivec2 pixel=clamp(center+ivec2(x,y),ivec2(0),size-1);
        vec2 offset=(vec2(pixel)+0.5)/vec2(size)-projected.xy;
        float receiver=projected.z+dot(slope,offset);
        float stored=texelFetch(uShadowMaps,ivec3(pixel,slot),0).r;
        lit+=receiver-bias<=stored?1.0:0.0;
    }
    return lit/9.0;
}
