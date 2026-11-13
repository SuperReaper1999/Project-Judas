// Included by Renderer.cpp. Vulkan-native shadow and mesh passes.
//
// When the Vulkan backend is active the scene's mesh draws (opaque, instanced,
// skinned, blended and shadow) are issued through the explicit interface in
// vulkan/VulkanGfx.h: pipelines created at startup, an explicit depth pass per
// shadow chart, a binding group naming every texture and buffer, and draws.
// No OpenGL-named call is made for them. The OpenGL versions of the same code
// (DrawMesh's submit, BindMaterial, BindTerrain*, EndMeshBatch, Begin/End
// ShadowPass) are untouched and remain the path of the OpenGL backend.
//
// Still shared with code that has not been migrated (see docs/VULKAN_NATIVE.md):
// scene-wide shader parameters (lighting, weather, water, terrain flags) are
// written by their own passes into the same parameter block; the frame's
// colour target and viewport are chosen by frame code; resources are created by
// the existing resource code and referenced here by handle.
#include "../vulkan/VulkanGfx.h"
#include <unordered_map>

namespace Gfx = JudasVulkan::Gfx;

struct Renderer::NativeMeshPass {
    struct ShaderSet {
        Gfx::Shader* shader=nullptr;
        unsigned program=0;
        int uInstanced=-1,uSkinned=-1,uModel=-1,uNormalMatrix=-1,uColor=-1,uLightViewProj=-1;
        int tBase=-1,tShadow=-1,tPointShadow=-1,tLightGrid=-1,tWeather=-1,tWater=-1,tTerrainDetail=-1,tTerrainHeights=-1;
        int tMaps[4]={-1,-1,-1,-1},tEnv[3]={-1,-1,-1};
        int bView=-1,bLights=-1,bBones=-1;
        // [blended][skinned][instanced]
        Gfx::Pipeline* pipeline[2][2][2]{};
        Gfx::Bindings bindings;
        std::unordered_map<std::string,int> params;
        // Parameter index by name, resolved once per name.
        int Param(const char* name){
            const auto found=params.find(name);if(found!=params.end())return found->second;
            const int index=Gfx::Param(shader,name);params.emplace(name,index);return index;
        }
    } main,shadow;
    Gfx::VertexLayout* layout[2][2]{}; // [skinned][instanced]
    Gfx::Transient instances;          // the batch being submitted
};

bool Renderer::InitNativeMeshPass(){
    m_nativeMesh.reset();
    if(!JudasVulkan::Active())return true;
    if(const char* policy=std::getenv("JUDAS_VULKAN_NATIVE_MESH");policy&&std::strcmp(policy,"0")==0)return true;
    auto native=std::make_unique<NativeMeshPass>();
    auto adopt=[&](NativeMeshPass::ShaderSet& set,unsigned program){
        set.program=program;set.shader=Gfx::AdoptShader(program);if(!set.shader)return false;
        set.uInstanced=Gfx::Param(set.shader,"uInstanced");set.uSkinned=Gfx::Param(set.shader,"uSkinned");
        set.uModel=Gfx::Param(set.shader,"uModel");set.uNormalMatrix=Gfx::Param(set.shader,"uNormalMatrix");
        set.uColor=Gfx::Param(set.shader,"uColor");set.uLightViewProj=Gfx::Param(set.shader,"uLightViewProj");
        set.tBase=Gfx::TextureSlot(set.shader,"uTexture");set.tShadow=Gfx::TextureSlot(set.shader,"uShadowMaps");
        set.tPointShadow=Gfx::TextureSlot(set.shader,"uPointShadowMaps");set.tLightGrid=Gfx::TextureSlot(set.shader,"uLightGrid");
        set.tWeather=Gfx::TextureSlot(set.shader,"uWeatherNoise");set.tWater=Gfx::TextureSlot(set.shader,"uWaterPaths");
        set.tTerrainDetail=Gfx::TextureSlot(set.shader,"uTerrainDetailNormal");set.tTerrainHeights=Gfx::TextureSlot(set.shader,"uTerrainHeights");
        const char* maps[]={"uMR","uNormal","uOcclusion","uEmissive"};for(int i=0;i<4;++i)set.tMaps[i]=Gfx::TextureSlot(set.shader,maps[i]);
        const char* env[]={"uEnvDiffuse","uEnvSpecular","uBRDF"};for(int i=0;i<3;++i)set.tEnv[i]=Gfx::TextureSlot(set.shader,env[i]);
        set.bView=Gfx::BufferSlot(set.shader,"JudasView");set.bLights=Gfx::BufferSlot(set.shader,"JudasLightLists");
        set.bBones=Gfx::BufferSlot(set.shader,"uBones");
        return true;
    };
    if(!adopt(native->main,m_shaderProgram)||!adopt(native->shadow,m_shadowShaderProgram)){
        std::fprintf(stderr,"[Vulkan] native mesh pass unavailable: the scene shaders could not be adopted\n");return true;
    }
    // Mesh vertex streams: 0 = MeshVertex, 1 = MeshSkinVertex, 2 = MeshInstance.
    // A mesh without a skin stream, or a draw that is not instanced, reads
    // constants there, exactly as an OpenGL attribute without an array does.
    for(int skinned=0;skinned<2;++skinned)for(int instanced=0;instanced<2;++instanced){
        Gfx::VertexLayoutDesc d;
        auto add=[&](unsigned location,unsigned stream,Gfx::Format format,std::size_t offset){auto& a=d.attributes[d.attributeCount++];a.location=std::uint8_t(location);a.stream=std::uint8_t(stream);a.format=format;a.offset=std::uint16_t(offset);};
        d.streamCount=4;
        d.streams[0]={Gfx::StreamKind::PerVertex,std::uint16_t(sizeof(MeshVertex))};
        add(0,0,Gfx::Format::Float3,offsetof(MeshVertex,position));add(1,0,Gfx::Format::Float3,offsetof(MeshVertex,normal));
        add(2,0,Gfx::Format::Float2,offsetof(MeshVertex,uv));add(8,0,Gfx::Format::Float2,offsetof(MeshVertex,uv1));
        add(5,0,Gfx::Format::Float4,offsetof(MeshVertex,tangent));
        if(skinned){
            d.streams[1]={Gfx::StreamKind::PerVertex,std::uint16_t(sizeof(MeshSkinVertex))};
            add(3,1,Gfx::Format::UInt4,offsetof(MeshSkinVertex,joints));add(4,1,Gfx::Format::Float4,offsetof(MeshSkinVertex,weights));
            add(6,1,Gfx::Format::UInt4,offsetof(MeshSkinVertex,joints1));add(7,1,Gfx::Format::Float4,offsetof(MeshSkinVertex,weights1));
        }else{
            d.streams[1]={Gfx::StreamKind::ConstantFloat,0};d.streams[3]={Gfx::StreamKind::ConstantUInt,0};
            add(3,3,Gfx::Format::UInt4,0);add(4,1,Gfx::Format::Float4,0);add(6,3,Gfx::Format::UInt4,0);add(7,1,Gfx::Format::Float4,0);
        }
        if(skinned)d.streams[3]={Gfx::StreamKind::ConstantUInt,0};
        if(instanced){
            d.streams[2]={Gfx::StreamKind::PerInstance,std::uint16_t(sizeof(Instance))};
            for(unsigned c=0;c<4;++c)add(9+c,2,Gfx::Format::Float4,sizeof(glm::vec4)*c);
            add(13,2,Gfx::Format::Float4,offsetof(Instance,tint));
        }else{
            d.streams[2]={Gfx::StreamKind::ConstantFloat,0};
            for(unsigned c=0;c<5;++c)add(9+c,2,Gfx::Format::Float4,0);
        }
        native->layout[skinned][instanced]=Gfx::CreateVertexLayout(d);
    }
    // Every pipeline the mesh passes can use is created here, for the target
    // formats the engine renders meshes into, so none is compiled mid-frame.
    for(int skinned=0;skinned<2;++skinned)for(int instanced=0;instanced<2;++instanced){
        for(int blended=0;blended<2;++blended){
            auto* p=Gfx::CreatePipeline({native->main.shader,native->layout[skinned][instanced],blended?Gfx::Blend::Alpha:Gfx::Blend::Opaque});
            native->main.pipeline[blended][skinned][instanced]=p;
            Gfx::Prewarm(p,Gfx::TargetFormat::Rgba8,true);Gfx::Prewarm(p,Gfx::TargetFormat::Rgba16F,true);
        }
        auto* p=Gfx::CreatePipeline({native->shadow.shader,native->layout[skinned][instanced],Gfx::Blend::Opaque});
        native->shadow.pipeline[0][skinned][instanced]=native->shadow.pipeline[1][skinned][instanced]=p;
        Gfx::Prewarm(p,Gfx::TargetFormat::None,true);
    }
    m_nativeMesh=std::move(native);
    std::fprintf(stderr,"[Graphics] Vulkan-native shadow and mesh passes enabled\n");
    return true;
}

void Renderer::NativeBeginShadowPass(int slot,const glm::mat4& lightViewProjection){
    Gfx::PassDesc pass;pass.depthTexture=m_shadowArray;pass.depthLayer=unsigned(slot);pass.clearDepth=true;pass.clearDepthValue=1.0f;
    if(!Gfx::BeginPass(pass))std::fprintf(stderr,"Shadow chart %d: the depth layer is not renderable\n",slot);
    Gfx::SetMat4(m_nativeMesh->shadow.shader,m_nativeMesh->shadow.uLightViewProj,glm::value_ptr(lightViewProjection));
}

// Twin of Renderer::BindMaterial (RendererMaterials.inl): the same material
// decisions, expressed as parameter writes and binding-group entries. Keep
// the two in step when material rules change. Returns whether to cull.
bool Renderer::NativeBindMaterial(const GpuMaterial* gpu, const MaterialBinding& binding,
                                  TextureHandle generated, float alpha, bool shadow) {
 auto& set=shadow?m_nativeMesh->shadow:m_nativeMesh->main;auto& bound=set.bindings;
 ++m_stats.materialApplications;
 MaterialDefinition legacy; legacy.model=MaterialModel::Legacy;
 auto m=ApplyMaterialOverride(gpu?gpu->definition:legacy,binding.overrides);
 JudasRender::MaterialUniformBatch uniforms;
 uniforms.Int("uAlphaMode",int(m.alpha));
 uniforms.Float("uAlphaCutoff",m.alphaCutoff);
 uniforms.Int("uFlipV",m.flipV);
 const glm::vec4 uv(m.uvScale,m.uvOffset);
 uniforms.Vec4("uUVTransform",glm::value_ptr(uv));
 const char* uvSets[]={"uMapUVSet[0]","uMapUVSet[1]","uMapUVSet[2]","uMapUVSet[3]","uMapUVSet[4]"};
 const char* uvTransforms[]={"uMapUVTransform[0]","uMapUVTransform[1]","uMapUVTransform[2]","uMapUVTransform[3]","uMapUVTransform[4]"};
 const char* uvRotations[]={"uMapUVRotation[0]","uMapUVRotation[1]","uMapUVRotation[2]","uMapUVRotation[3]","uMapUVRotation[4]"};
 for(unsigned i=0;i<5;++i){
  const auto& map=m.maps[i]; const glm::vec4 transform(map.scale,map.offset);
  uniforms.Int(uvSets[i],map.uvSet);
  uniforms.Vec4(uvTransforms[i],glm::value_ptr(transform));
  uniforms.Float(uvRotations[i],map.rotation);
 }
 auto mapTexture=[&](unsigned index){return binding.textureOverrides[index]?binding.textures[index]:(gpu?gpu->textures[index]:TextureHandle{});};
 TextureHandle base=binding.textureOverrides[0]?binding.textures[0]:(generated.IsValid()?generated:mapTexture(0));
 if(!shadow&&m.model!=MaterialModel::Legacy)base=ColourTexture(base);
 // Resolve colour views before binding unit zero: an owner-thread upload can
 // temporarily change texture state. Texture/sampler binding is never cached.
 auto emissionTexture=mapTexture(4);
 if(!shadow&&m.model!=MaterialModel::Legacy)emissionTexture=ColourTexture(emissionTexture);
 if(m_activeTarget.IsValid()&&base.IsValid()&&base.id==RenderTargetTexture(m_activeTarget).id){base={};++m_stats.feedbackFallbacks;}
 if(set.tBase>=0)bound.textures[set.tBase]={ResolveTexture(base),gpu&&(binding.textureOverrides[0]||!generated.IsValid())?gpu->samplers[0]:0};
 if(shadow){
  uniforms.Int("uTexture",0);
  uniforms.Float("uAlphaFactor",m.baseColor.a*alpha);
 } else {
  uniforms.Int("uMaterialModel",int(m.model));
  uniforms.Int("uModern",m_linearRendering);
  uniforms.Int("uBaseLinear",ValidTexture(base)&&(m_textures[base.id].sceneLinear||m_textures[base.id].srgb));
  uniforms.Vec4("uBaseFactor",glm::value_ptr(m.baseColor));
  uniforms.Float("uMetallic",m.metallic); uniforms.Float("uRoughness",m.roughness);
  uniforms.Float("uNormalStrength",m.normalStrength); uniforms.Float("uOcclusionStrength",m.occlusionStrength);
  uniforms.Vec3("uEmission",glm::value_ptr(m.emissive)); uniforms.Float("uEmissionIntensity",m.emissiveIntensity);
  uniforms.Int("uEmissiveLinear",ValidTexture(emissionTexture)&&(m_textures[emissionTexture.id].srgb||m_textures[emissionTexture.id].sceneLinear));
  const char* names[]={"uMR","uNormal","uOcclusion","uEmissive"};
  const char* present[]={"uHasMR","uHasNormal","uHasOcclusion","uHasEmissive"};
  for(unsigned i=1;i<5;++i){
   const auto texture=i==4?emissionTexture:mapTexture(i);
   if(set.tMaps[i-1]>=0)bound.textures[set.tMaps[i-1]]={ResolveTexture(texture),gpu?gpu->samplers[i]:0};
   uniforms.Int(names[i-1],int(4+i)); uniforms.Int(present[i-1],texture.IsValid());
  }
  const GpuEnvironment* env=m_environment.IsValid()&&m_environment.id<m_environments.size()&&m_environments[m_environment.id].alive?&m_environments[m_environment.id]:nullptr;
  uniforms.Int("uEnvironmentEnabled",env!=nullptr);
  uniforms.Float("uEnvironmentIntensity",m_environmentIntensity);
  uniforms.Float("uEnvLevels",env?float(env->levels):1);
  const auto inverse=glm::mat3_cast(glm::conjugate(m_environmentRotation));
  uniforms.Mat3("uEnvironmentInverse",glm::value_ptr(inverse));
  const char* envNames[]={"uEnvDiffuse","uEnvSpecular","uBRDF"};
  const GLuint textures[]={env?env->diffuse:0,env?env->specular:0,env?env->brdf:0};
  for(unsigned i=0;i<3;++i){
   if(set.tEnv[i]>=0)bound.textures[set.tEnv[i]]={textures[i],0};
   uniforms.Int(envNames[i],int(9+i));
  }
 }
 // Only material-owned uniforms are reused. Other writers (notably terrain)
 // explicitly invalidate this small per-program state before changing them.
 // Same reuse rule as the OpenGL path, sharing its per-program cache: only
 // material-owned parameters are skipped when unchanged.
 auto& state=m_materialUniformState[shadow?1:0];
 if(state.valid&&state.program==set.program&&state.values.Same(uniforms))++m_stats.materialUniformReuses;
 else {
  state.valid=false;
  uniforms.ForEach([&](const char* name,JudasRender::MaterialUniformBatch::Kind kind,GLint integer,const GLfloat* values){
   const int param=set.Param(name);if(param<0)return;
   using Kind=JudasRender::MaterialUniformBatch::Kind;
   switch(kind){
   case Kind::Integer:Gfx::SetInt(set.shader,param,integer);break;
   case Kind::Scalar:Gfx::SetFloats(set.shader,param,values,1);break;
   case Kind::Vector3:Gfx::SetFloats(set.shader,param,values,3);break;
   case Kind::Vector4:Gfx::SetFloats(set.shader,param,values,4);break;
   case Kind::Matrix3:Gfx::SetMat3(set.shader,param,values);break;
   }
  });
  state.values=uniforms;state.program=set.program;state.valid=true;++m_stats.materialUniformPublishes;
 }
 return !(m.model==MaterialModel::Legacy||m.doubleSided); // cull back faces
}


// Twin of Renderer::BindTerrainMaterial (render/TerrainPass.cpp).
void Renderer::NativeBindTerrainMaterial(){
    auto& set=m_nativeMesh->main;auto& bound=set.bindings;
    if(!m_terrainMaterial){if(set.tTerrainDetail>=0)bound.textures[set.tTerrainDetail]={};return;}
    InvalidateMaterialUniformState();
    Gfx::SetInt(set.shader,set.Param("uTerrain"),1);
    if(set.tBase>=0)bound.textures[set.tBase]={ResolveTexture(m_terrainWeights),0};
    for(const char* name:{"uHasNormal","uHasMR","uHasOcclusion","uHasEmissive"})Gfx::SetInt(set.shader,set.Param(name),0);
    for(unsigned i=0;i<4;++i)if(set.tMaps[i]>=0)bound.textures[set.tMaps[i]]={ResolveTexture(m_terrainLayers[i]),0};
    Gfx::SetFloats(set.shader,set.Param("uTerrainWeightRect"),glm::value_ptr(m_terrainWeightRect),4);
    Gfx::SetFloats(set.shader,set.Param("uTerrainColours[0]"),glm::value_ptr(m_terrainColours[0]),3,4);
    Gfx::SetFloats(set.shader,set.Param("uTerrainTileMetres"),glm::value_ptr(m_terrainDetail.tileMetres),4);
    Gfx::SetFloats(set.shader,set.Param("uTerrainRoughness"),glm::value_ptr(m_terrainDetail.roughness),4);
    Gfx::SetFloats(set.shader,set.Param("uTerrainNormalMetres"),&m_terrainDetail.normalMetres,1);
    Gfx::SetFloats(set.shader,set.Param("uTerrainNormalStrength"),&m_terrainDetail.normalStrength,1);
    Gfx::SetInt(set.shader,set.Param("uTerrainHasNormal"),m_terrainNormal.IsValid());
    if(set.tTerrainDetail>=0)bound.textures[set.tTerrainDetail]={ResolveTexture(m_terrainNormal),0};
    Gfx::SetMat4(set.shader,set.Param("uTerrainFromWorld"),glm::value_ptr(m_terrainFromWorld));
}
// Twin of Renderer::BindTerrainGeometry (render/TerrainPass.cpp).
void Renderer::NativeBindTerrainGeometry(bool shadow){
    auto& set=shadow?m_nativeMesh->shadow:m_nativeMesh->main;auto& bound=set.bindings;
    if(!m_terrainGrid){if(set.tTerrainHeights>=0)bound.textures[set.tTerrainHeights]={};return;}
    Gfx::SetInt(set.shader,set.Param("uTerrainGrid"),1);
    Gfx::SetInt(set.shader,set.Param("uTerrainPreparedPage"),m_terrainPreparedPage);
    Gfx::SetFloats(set.shader,set.Param("uTerrainSamples"),glm::value_ptr(m_terrainSampleRect),4);
    Gfx::SetFloats(set.shader,set.Param("uTerrainEdges"),glm::value_ptr(m_terrainEdgeStrides),4);
    Gfx::SetFloats(set.shader,set.Param("uTerrainGridCells"),glm::value_ptr(m_terrainGridCells),2);
    Gfx::SetFloats(set.shader,set.Param("uTerrainDimensions"),glm::value_ptr(m_terrainDimensions),2);
    Gfx::SetFloats(set.shader,set.Param("uTerrainCoverage"),glm::value_ptr(m_terrainCoverage),2);
    Gfx::SetFloats(set.shader,set.Param("uTerrainSpacing"),&m_terrainSpacing,1);
    if(set.tTerrainHeights>=0)bound.textures[set.tTerrainHeights]={ResolveTexture(m_terrainHeights),0};
}

// The Vulkan form of DrawMesh's submission: per-draw parameters, the binding
// group and one explicit draw per visible part. `materialAt`, `bindingAt` and
// `alphaAt` are DrawMesh's own selectors, so part and material choice is the
// engine's, not duplicated here.
template<class MaterialAt,class BindingAt,class AlphaAt>
void Renderer::NativeSubmitMesh(GpuMesh& gpuMesh,const std::vector<MeshPrimitive>& primitives,unsigned lod,const glm::mat4& model,
                                const std::vector<glm::mat4>& palette,TextureHandle texture,const glm::vec3& tintColor,float alpha,
                                const std::vector<std::string>* hiddenParts,MaterialAt&& materialAt,BindingAt&& bindingAt,AlphaAt&& alphaAt){
    const bool shadow=m_shadowPassActive;
    auto& native=*m_nativeMesh;auto& set=shadow?native.shadow:native.main;auto& bound=set.bindings;
    const bool skinnedMesh=gpuMesh.skinVbo!=0,instanced=m_submittingBatch;
    Gfx::SetInt(set.shader,set.uInstanced,instanced);
    Gfx::SetInt(set.shader,set.uSkinned,!palette.empty());
    Gfx::SetMat4(set.shader,set.uModel,glm::value_ptr(model));
    if(set.bBones>=0){
        bound.buffers[set.bBones]={};
        if(!palette.empty()){
            JUDAS_PROFILE_SCOPE("Pose palette upload");
            const auto bytes=std::uint32_t(palette.size()*sizeof(glm::mat4));
            auto table=Gfx::AllocateTransient(bytes);std::memcpy(table.data,palette.data(),bytes);
            bound.buffers[set.bBones].transient=table;
        }
    }
    if(!shadow){
        const glm::mat3 normalMatrix=glm::inverseTranspose(glm::mat3(model));
        Gfx::SetMat3(set.shader,set.uNormalMatrix,glm::value_ptr(normalMatrix));
        const glm::vec4 colour(tintColor,alpha);Gfx::SetFloats(set.shader,set.uColor,glm::value_ptr(colour),4);
        std::string viewError;const auto viewBefore=m_render->viewUniforms.Stats();
        if(!m_render->viewUniforms.Bind(m_shaderProgram,viewError)){std::fprintf(stderr,"View upload failed: %s\n",viewError.c_str());return;}
        const auto viewAfter=m_render->viewUniforms.Stats();
        m_stats.uploadedBytes+=viewAfter.bytesUploaded-viewBefore.bytesUploaded;
        m_stats.uploadCalls+=viewAfter.uploads-viewBefore.uploads;
        if(set.bView>=0){bound.buffers[set.bView]={};bound.buffers[set.bView].buffer=m_render->viewUniforms.Buffer();bound.buffers[set.bView].size=sizeof(JudasRender::ViewUniformData);}
        if(set.bLights>=0){bound.buffers[set.bLights]={};if(m_render->lightStorageReady)bound.buffers[set.bLights].buffer=m_render->lightLists.GraphicsBuffer();}
        if(set.tShadow>=0)bound.textures[set.tShadow]={m_shadowArray,0};
        if(set.tPointShadow>=0)bound.textures[set.tPointShadow]={m_pointShadowView,0};
        if(set.tLightGrid>=0)bound.textures[set.tLightGrid]={m_lightTexture,0};
        if(set.tWeather>=0)bound.textures[set.tWeather]={m_weather.noise,0};
        if(set.tWater>=0)bound.textures[set.tWater]={m_waterPathTexture,0};
    }
    const bool blendedPass=!shadow&&m_nativeTransparent;
    const std::size_t parts=std::max(std::size_t(1),primitives.size());
    for(std::size_t i=0;i<parts;++i){
        if(hiddenParts&&!primitives.empty()&&std::find(hiddenParts->begin(),hiddenParts->end(),primitives[i].part)!=hiddenParts->end())continue;
        auto* m=materialAt(i);const bool isBlend=alphaAt(i,m)==MaterialAlpha::Blend;
        if((shadow&&isBlend)||(!shadow&&isBlend!=m_flushingBlends))continue;
        const bool cull=NativeBindMaterial(m,bindingAt(i),texture,alpha,shadow);
        if(!shadow)NativeBindTerrainMaterial();
        NativeBindTerrainGeometry(shadow);
        glm::mat4 orientation(1);
        if(i<gpuMesh.partOrientation.size()&&!palette.empty()){orientation=glm::mat4(0);auto& w=gpuMesh.partOrientation[i];for(int k=0;k<4;++k)orientation+=palette.at(w.joints[k])*w.weights[k]+palette.at(w.joints1[k])*w.weights1[k];}
        const bool reversed=instanced?m_activeBatchReversed:glm::determinant(glm::mat3(model*orientation))<0;
        unsigned first=0,count=unsigned(gpuMesh.ebo?gpuMesh.indexCount:gpuMesh.vertexCount);
        if(!primitives.empty()){first=primitives[i].first;count=primitives[i].count;}
        Gfx::DrawDesc draw;
        draw.pipeline=set.pipeline[blendedPass?1:0][skinnedMesh?1:0][instanced?1:0];
        draw.bindings=&bound;
        draw.streams[0].buffer=gpuMesh.vbo;draw.streams[1].buffer=gpuMesh.skinVbo;
        if(instanced)draw.streams[2].transient=native.instances;
        draw.indexBuffer=gpuMesh.ebo;draw.first=first;draw.count=count;draw.instances=instanced?m_activeInstances:1;
        draw.cull=cull;draw.frontCounterClockwise=!reversed;draw.depthTest=true;draw.depthWrite=!blendedPass;
        Gfx::Draw(draw);
        if(instanced){++m_stats.instancedDraws;m_stats.instances+=m_activeInstances;}
        if(lod)m_stats.lodReducedInstances+=(instanced?m_activeInstances:1);
        ++m_stats.drawCalls;m_stats.triangles+=count/3*(instanced?m_activeInstances:1);
    }
}
