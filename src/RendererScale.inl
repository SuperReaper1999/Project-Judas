// Included by Renderer.cpp: context-owned resources, no gameplay or scene policy.
void Renderer::SetDynamicLights(const std::vector<DynamicLight>& lights){
 JUDAS_PROFILE_SCOPE("Light tile selection/upload");
 GLint viewport[4];glGetIntegerv(GL_VIEWPORT,viewport);
 const auto* policy=std::getenv("JUDAS_LIGHT_LISTS");
 const bool storage=m_render->lightStorageVariant;
 const bool gpuBins=storage&&!m_render->lightStorageUnavailable&&((policy&&std::strcmp(policy,"compute")==0)||((!policy||std::strcmp(policy,"auto")==0)&&lights.size()>=128));
 m_lightGrid.Build(lights,m_view,m_projection,unsigned(std::max(1,viewport[2])),unsigned(std::max(1,viewport[3])),!gpuBins);
 glUseProgram(m_shaderProgram);glUniform4f(glGetUniformLocation(m_shaderProgram,"uLightViewport"),float(viewport[0]),float(viewport[1]),float(viewport[2]),float(viewport[3]));
 glUniform1i(glGetUniformLocation(m_shaderProgram,"uHasDynamicLights"),m_lightGrid.visible!=0);
 m_render->lightStorageReady=false;glUniform1i(glGetUniformLocation(m_shaderProgram,"uUseLightStorage"),0);
 m_stats.shadowBytes=ShadowStorageBytes();
 if(!m_lightGrid.visible){m_stats.dynamicLights=0;m_stats.lightRejected+=m_lightGrid.rejected;return;}
 m_stats.dynamicLights=m_lightGrid.visible;m_stats.lightRejected+=m_lightGrid.rejected;m_stats.lightTileOverflow+=m_lightGrid.overflow;m_stats.maxTileLights=std::max(m_stats.maxTileLights,m_lightGrid.maxPerTile);
 if(storage){
  std::string error;
  if(m_render->lightLists.Upload(m_lightGrid,gpuBins,m_capabilities,error)){
   m_render->lightStorageReady=true;glUseProgram(m_shaderProgram);glUniform1i(glGetUniformLocation(m_shaderProgram,"uUseLightStorage"),1);
   m_stats.lightBytes=m_render->lightLists.AllocatedBytes();m_stats.uploadedBytes+=m_lightGrid.texels.size()*sizeof(glm::vec4);++m_stats.uploadCalls;return;
  }
  m_render->lightStorageUnavailable=true;
  std::fprintf(stderr,"Lighting storage fallback: %s\n",error.c_str());
  if(gpuBins){
   m_lightGrid.Build(lights,m_view,m_projection,unsigned(std::max(1,viewport[2])),unsigned(std::max(1,viewport[3])));
   if(m_render->lightLists.Upload(m_lightGrid,false,m_capabilities,error)){m_render->lightStorageReady=true;m_stats.lightBytes=m_render->lightLists.AllocatedBytes();return;}
  }
  throw std::runtime_error("Lighting storage publication failed: "+error);
 }
 const unsigned rows=std::max(1u,unsigned((m_lightGrid.texels.size()+1023)/1024));m_lightGrid.texels.resize(size_t(rows)*1024);
 if(!m_lightTexture)glGenTextures(1,&m_lightTexture);
 glActiveTexture(GL_TEXTURE14);glBindTexture(GL_TEXTURE_2D,m_lightTexture);
 if(rows>m_lightTextureRows){m_lightTextureRows=rows;glTexImage2D(GL_TEXTURE_2D,0,GL_RGBA32F,1024,GLsizei(rows),0,GL_RGBA,GL_FLOAT,nullptr);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_NEAREST);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_NEAREST);}
 glTexSubImage2D(GL_TEXTURE_2D,0,0,0,1024,GLsizei(rows),GL_RGBA,GL_FLOAT,m_lightGrid.texels.data());m_stats.uploadedBytes+=size_t(rows)*1024*sizeof(glm::vec4);++m_stats.uploadCalls;glActiveTexture(GL_TEXTURE0);
 m_stats.lightBytes=size_t(m_lightTextureRows)*1024*sizeof(glm::vec4);
}
void Renderer::BeginMeshBatch(){
 if(m_collectingBatch)return;
 m_render->submission.Clear();m_collectingBatch=true;
}
void Renderer::QueueMeshInstance(MeshHandle mesh,TextureHandle texture,const glm::mat4& model,glm::vec4 tint,unsigned lod){
 const bool reversed=glm::determinant(glm::mat3(model))<0;
 if(m_render->orderedInstances?m_render->submission.AddOrdered(mesh,texture,model,tint,lod,reversed,m_materialBindings):
                       m_render->submission.Add(mesh,texture,model,tint,lod,reversed,m_materialBindings)){
     EndMeshBatch();BeginMeshBatch();
 }
}
void Renderer::EndMeshBatch(){
 if(!m_collectingBatch)return;
 m_collectingBatch=false;
 if(m_render->submission.Batches().empty())return;
 JUDAS_PROFILE_SCOPE("Opaque instance submission");RendererProfileScope gpu(*this,"Opaque instances");
 auto saved=m_materialBindings;auto layer=m_renderLayer;
 // Size persistent pages from the actual pass working set, rather than the
 // largest tiny draw. This avoids forcing reuse waits between material groups.
 std::size_t workingSet=0;
 for(const auto& batch:m_render->submission.Batches()){
     const auto bytes=batch.instances.size()*sizeof(Instance);
     const auto aligned=(workingSet+255)&~std::size_t(255);
     workingSet=bytes>=JudasRender::StreamArena::PageBytes||aligned>JudasRender::StreamArena::PageBytes-bytes?
         JudasRender::StreamArena::PageBytes:aligned+bytes;
 }
 m_render->instanceArena.SetWorkingSetHint(workingSet);
 if(!m_nativeMesh&&!m_instanceBuffer)glGenBuffers(1,&m_instanceBuffer);
 for(const auto& batch:m_render->submission.Batches()){auto* mesh=GetMesh(batch.mesh);if(!mesh)continue;
  const auto bytes=batch.instances.size()*sizeof(Instance);
  if(m_nativeMesh){
      // Vulkan: the instance array is transient frame data bound by offset.
      m_nativeMesh->instances=JudasVulkan::Gfx::AllocateTransient(std::uint32_t(bytes));
      std::memcpy(m_nativeMesh->instances.data,batch.instances.data(),bytes);
      m_stats.instanceBytes=std::max(m_stats.instanceBytes,bytes);m_stats.uploadedBytes+=bytes;++m_stats.uploadCalls;
      m_materialBindings=batch.bindings;m_submittingBatch=true;m_activeInstances=unsigned(batch.instances.size());m_activeBatchReversed=batch.reversed;m_activeBatchLod=batch.lod;
      DrawMesh(batch.mesh,{0,0,0},{1,0,0,0},{1,1,1},batch.texture,{1,1,1},1);
      m_submittingBatch=false;continue;
  }
  JudasRender::StreamSlice slice;std::string error;
  if(!m_render->instanceArena.Upload(batch.instances.data(),bytes,m_render->persistentUploads&&m_capabilities.modernTier&&m_capabilities.bufferStorage,slice,error)){
      // Preserve the draw if an optional streaming facility fails. Orphaned
      // storage has the same ownership and ordering as the portable renderer.
      std::fprintf(stderr,"Instance arena fallback; persistent uploads disabled for this renderer: %s\n",error.c_str());
      m_render->persistentUploads=false;
      glBindBuffer(GL_ARRAY_BUFFER,m_instanceBuffer);
      glBufferData(GL_ARRAY_BUFFER,GLsizeiptr(bytes),batch.instances.data(),GL_STREAM_DRAW);
      slice={m_instanceBuffer,0};
  }
  glBindVertexArray(mesh->vao);glBindBuffer(GL_ARRAY_BUFFER,slice.buffer);
  m_stats.instanceBytes=std::max(m_stats.instanceBytes,bytes);
  m_stats.uploadedBytes+=bytes;++m_stats.uploadCalls;
  for(unsigned c=0;c<4;++c){glEnableVertexAttribArray(9+c);glVertexAttribPointer(9+c,4,GL_FLOAT,GL_FALSE,sizeof(Instance),reinterpret_cast<void*>(slice.offset+sizeof(glm::vec4)*c));glVertexAttribDivisor(9+c,1);}
  glEnableVertexAttribArray(13);glVertexAttribPointer(13,4,GL_FLOAT,GL_FALSE,sizeof(Instance),reinterpret_cast<void*>(slice.offset+offsetof(Instance,tint)));glVertexAttribDivisor(13,1);
  m_materialBindings=batch.bindings;m_submittingBatch=true;m_activeInstances=unsigned(batch.instances.size());m_activeBatchReversed=batch.reversed;m_activeBatchLod=batch.lod;
  DrawMesh(batch.mesh,{0,0,0},{1,0,0,0},{1,1,1},batch.texture,{1,1,1},1);
  m_submittingBatch=false;glBindVertexArray(mesh->vao);for(unsigned c=9;c<=13;++c){glDisableVertexAttribArray(c);glVertexAttribDivisor(c,0);}
 }
 if(!m_nativeMesh)glBindVertexArray(0);
 m_materialBindings=std::move(saved);m_renderLayer=layer;m_render->submission.Clear();
}

bool Renderer::SetGraphicsQuality(const GraphicsQuality& quality, std::string& error, glm::ivec2 outputSize) {
 if(!SupportsGraphicsQuality()||m_shadowPassActive||m_activeTarget.IsValid()){
  error="Graphics quality requires the shared renderer at a frame boundary";return false;
 }
 if(std::max(quality.shadowResolution,quality.SunShadowResolution())>
    unsigned(std::max(0,m_capabilities.limits.textureSize))){
  error="Shadow resolution exceeds the current device texture limit";return false;
 }
 if(quality.SunShadowResolution()!=quality.shadowResolution&&!SupportsIndependentShadowResolution()){
  error="Independent shadow resolutions require 18 fragment texture units";return false;
 }
 if(quality.materialAnisotropy>m_maxMaterialAnisotropy){
  error="Material anisotropy exceeds the current device sampler limit";return false;
 }
 RenderTargetHandle stagedMain;
 if(quality.renderScale!=1.f&&outputSize.x>0&&outputSize.y>0){
  const auto size=ScaledMainViewSize(outputSize,quality.renderScale);
  if(RenderTargetSize(m_scaledMainView)!=size){
   stagedMain=CreateRenderTarget(size.x,size.y,error,true);
   if(!stagedMain.IsValid())return false;
  }
 }
 if(m_shadowLayers&&(quality.shadowResolution!=m_graphicsQuality.shadowResolution||
                     quality.SunShadowResolution()!=m_graphicsQuality.SunShadowResolution())){
  const unsigned limit=quality.localShadowBudget?unsigned(kLocalShadowBase)+6*quality.localShadowBudget:
                       quality.directionalCascades;
  const unsigned layers=std::min(m_shadowLayers,limit);
  if(!AllocateShadowLayers(layers,quality,false)){
   if(stagedMain.IsValid()){DestroyRenderTarget(stagedMain);m_targets.pop_back();}
   error="Could not allocate the requested shadow depth array";return false;
  }
  m_shadowContentValid.fill(false);
  m_render->shadowCache.Clear();
 }
 if(stagedMain.IsValid()||quality.renderScale==1.f)ReplaceMainViewTarget(stagedMain);
 m_mainViewError.clear();
 m_failedMainViewSize={};
 if(quality.softShadows!=m_graphicsQuality.softShadows){
  glUseProgram(m_shaderProgram);
  glUniform1i(glGetUniformLocation(m_shaderProgram,"uFastShadows"),!quality.softShadows);
 }
 if(quality.materialAnisotropy!=m_graphicsQuality.materialAnisotropy)
  for(const auto& entry:m_anisotropicSamplers)
   if(entry.second)glSamplerParameterf(entry.second,kMaterialAnisotropyParameter,float(quality.materialAnisotropy));
 if(quality.decorativeDensity!=m_graphicsQuality.decorativeDensity)
  m_render->shadowCache.Clear();
 m_graphicsQuality=quality;
 m_rejectedShadowLayers=0;
 return true;
}

bool Renderer::ReserveShadowLayers(unsigned layers){
 layers=std::min(layers,unsigned(kShadowMapCount));
 if(!m_shadowsEnabled||layers<=m_shadowLayers)return true;
 if(m_rejectedShadowLayers&&layers>=m_rejectedShadowLayers)return false;
 if(AllocateShadowLayers(layers,m_graphicsQuality,true))return true;
 m_rejectedShadowLayers=layers;
 std::fprintf(stderr,"Cannot grow shadow depth storage to %u layers at %u pixels\n",
              layers,m_graphicsQuality.shadowResolution);
 return false;
}

std::size_t Renderer::ShadowStorageBytes() const {
 const auto local=m_graphicsQuality.shadowResolution;
 if(!m_directionalShadowArray)return size_t(m_shadowLayers)*local*local*4;
 const auto sun=m_graphicsQuality.SunShadowResolution();
 const auto directionalLayers=std::min(unsigned(kLocalShadowBase),m_shadowLayers);
 return (size_t(directionalLayers)*sun*sun+size_t(m_shadowLayers-directionalLayers)*local*local)*4;
}

bool Renderer::AllocateShadowLayers(unsigned layers,const GraphicsQuality& quality,bool preserve){
 const bool split=quality.SunShadowResolution()!=quality.shadowResolution;
 bool cubeViews=false;
#if !defined(__ANDROID__) && !defined(__EMSCRIPTEN__)
 cubeViews=m_capabilities.modernTier&&m_capabilities.textureViews&&m_capabilities.limits.fragmentTextureUnits>16;
#endif
 if(cubeViews&&layers>kLocalShadowBase)layers=std::min(unsigned(kShadowMapCount),((layers+5)/6)*6);
 const unsigned directionalLayers=split?std::min(layers,unsigned(kLocalShadowBase)):0;
 const unsigned localLayers=layers-directionalLayers;
 if(std::max(localLayers,directionalLayers)>unsigned(std::max(0,m_capabilities.limits.arrayTextureLayers)))return false;
 GLint draw=0,read=0;
 glGetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING,&draw);
 glGetIntegerv(GL_READ_FRAMEBUFFER_BINDING,&read);
 GLuint temporary=0;
 glGenFramebuffers(1,&temporary);
 glBindFramebuffer(GL_FRAMEBUFFER,temporary);
 const GLenum none=GL_NONE;
 glDrawBuffers(1,&none);glReadBuffer(GL_NONE);
 const auto allocate=[&](GLuint& texture,unsigned count,unsigned size,bool immutable) {
  if(!count)return true;
  glGenTextures(1,&texture);
  glBindTexture(GL_TEXTURE_2D_ARRAY,texture);
#if !defined(__ANDROID__) && !defined(__EMSCRIPTEN__)
  if(immutable)glTexStorage3D(GL_TEXTURE_2D_ARRAY,1,GL_DEPTH_COMPONENT24,size,size,count);
  else
#endif
   glTexImage3D(GL_TEXTURE_2D_ARRAY,0,GL_DEPTH_COMPONENT24,size,size,count,0,GL_DEPTH_COMPONENT,GL_UNSIGNED_INT,nullptr);
  glTexParameteri(GL_TEXTURE_2D_ARRAY,GL_TEXTURE_MIN_FILTER,GL_NEAREST);
  glTexParameteri(GL_TEXTURE_2D_ARRAY,GL_TEXTURE_MAG_FILTER,GL_NEAREST);
  glTexParameteri(GL_TEXTURE_2D_ARRAY,GL_TEXTURE_WRAP_S,GL_CLAMP_TO_EDGE);
  glTexParameteri(GL_TEXTURE_2D_ARRAY,GL_TEXTURE_WRAP_T,GL_CLAMP_TO_EDGE);
  glFramebufferTextureLayer(GL_DRAW_FRAMEBUFFER,GL_DEPTH_ATTACHMENT,texture,0,GLint(count-1));
  return glCheckFramebufferStatus(GL_DRAW_FRAMEBUFFER)==GL_FRAMEBUFFER_COMPLETE;
 };
 GLuint next=0,nextDirectional=0;
 const bool complete=allocate(next,localLayers,quality.shadowResolution,cubeViews)&&
                     allocate(nextDirectional,directionalLayers,quality.SunShadowResolution(),false);
 const auto attach=[&](GLenum target,unsigned slot){
  const bool sun=slot<directionalLayers;
  glFramebufferTextureLayer(target,GL_DEPTH_ATTACHMENT,sun?nextDirectional:next,0,
                            GLint(sun?slot:slot-directionalLayers));
 };
 if(complete&&preserve){
  for(unsigned i=0;i<m_shadowLayers;++i){
   const auto size=i<directionalLayers?quality.SunShadowResolution():quality.shadowResolution;
   glBindFramebuffer(GL_READ_FRAMEBUFFER,m_shadowFbo[i]);
   glReadBuffer(GL_NONE);
   attach(GL_DRAW_FRAMEBUFFER,i);
   glBlitFramebuffer(0,0,size,size,0,0,size,size,GL_DEPTH_BUFFER_BIT,GL_NEAREST);
  }
 }
 glDeleteFramebuffers(1,&temporary);
 if(!complete){
  glDeleteTextures(1,&next);glDeleteTextures(1,&nextDirectional);
  glBindFramebuffer(GL_DRAW_FRAMEBUFFER,GLuint(draw));glBindFramebuffer(GL_READ_FRAMEBUFFER,GLuint(read));
  return false;
 }
 for(unsigned i=0;i<layers;++i){
  if(!m_shadowFbo[i])glGenFramebuffers(1,&m_shadowFbo[i]);
  glBindFramebuffer(GL_FRAMEBUFFER,m_shadowFbo[i]);
  attach(GL_FRAMEBUFFER,i);
  glDrawBuffers(1,&none);glReadBuffer(GL_NONE);
 }
 if(m_pointShadowView){glDeleteTextures(1,&m_pointShadowView);m_pointShadowView=0;}
#if !defined(__ANDROID__) && !defined(__EMSCRIPTEN__)
 if(cubeViews&&layers>kLocalShadowBase){
  glGenTextures(1,&m_pointShadowView);
  glTextureView(m_pointShadowView,GL_TEXTURE_CUBE_MAP_ARRAY,next,GL_DEPTH_COMPONENT24,0,1,
                kLocalShadowBase-directionalLayers,layers-kLocalShadowBase);
  glBindTexture(GL_TEXTURE_CUBE_MAP_ARRAY,m_pointShadowView);
  glTexParameteri(GL_TEXTURE_CUBE_MAP_ARRAY,GL_TEXTURE_MIN_FILTER,GL_NEAREST);
  glTexParameteri(GL_TEXTURE_CUBE_MAP_ARRAY,GL_TEXTURE_MAG_FILTER,GL_NEAREST);
  glTexParameteri(GL_TEXTURE_CUBE_MAP_ARRAY,GL_TEXTURE_WRAP_S,GL_CLAMP_TO_EDGE);
  glTexParameteri(GL_TEXTURE_CUBE_MAP_ARRAY,GL_TEXTURE_WRAP_T,GL_CLAMP_TO_EDGE);
  glTexParameteri(GL_TEXTURE_CUBE_MAP_ARRAY,GL_TEXTURE_WRAP_R,GL_CLAMP_TO_EDGE);
  glActiveTexture(GL_TEXTURE16);glBindTexture(GL_TEXTURE_CUBE_MAP_ARRAY,m_pointShadowView);glActiveTexture(GL_TEXTURE0);
 }
#endif
 for(unsigned i=layers;i<m_shadowLayers;++i){glDeleteFramebuffers(1,&m_shadowFbo[i]);m_shadowFbo[i]=0;}
 glDeleteTextures(1,&m_shadowArray);glDeleteTextures(1,&m_directionalShadowArray);
 m_shadowArray=next;m_directionalShadowArray=nextDirectional;m_shadowLayers=layers;
 if(SupportsIndependentShadowResolution()){
  GLint previous=0;glGetIntegerv(GL_CURRENT_PROGRAM,&previous);
  glUseProgram(m_shaderProgram);
  glUniform1i(MaterialUniform("uSeparateShadowMaps"),split);
  glUseProgram(GLuint(previous));
 }
 glBindFramebuffer(GL_DRAW_FRAMEBUFFER,GLuint(draw));glBindFramebuffer(GL_READ_FRAMEBUFFER,GLuint(read));
 glBindTexture(GL_TEXTURE_2D_ARRAY,0);
 return true;
}

size_t Renderer::TextureResidentBytes(TextureHandle h) const {
    if (!ValidTexture(h))
        return 0;
    const auto &t = m_textures[h.id];
    size_t bytes = t.uploadedBytes;
    if (ValidTexture(t.colourView))
        bytes += m_textures[t.colourView.id].uploadedBytes;
    return bytes;
}
size_t Renderer::MeshResidentBytes(MeshHandle h) const {
    if (!h.IsValid() || h.id >= m_meshes.size() || !m_meshes[h.id].alive ||
        m_meshes[h.id].generation != h.generation)
        return 0;
    const auto &m = m_meshes[h.id];
    size_t bytes =
        size_t(m.vertexCount) * sizeof(MeshVertex) + size_t(m.indexCount) * 4 +
        (m.skinVbo ? size_t(m.vertexCount) * sizeof(MeshSkinVertex) : 0);
    for (auto handle : m.materials)
        if (ValidMaterial(handle))
            for (auto t : m_materials[handle.id].textures)
                bytes += TextureResidentBytes(t);
    return bytes;
}

TextureHandle Renderer::InstallTexture(GpuTexture texture) {
    TextureHandle handle;
    if (m_freeTextures.empty()) {
        handle.id = unsigned(m_textures.size());
        m_textures.push_back(std::move(texture));
    } else {
        handle.id = m_freeTextures.back();
        m_freeTextures.pop_back();
        texture.generation = m_textures[handle.id].generation;
        handle.generation = texture.generation;
        m_textures[handle.id] = std::move(texture);
    }
    return handle;
}
size_t Renderer::MaterialResidentBytes(MaterialHandle h) const {
    if (!ValidMaterial(h))
        return 0;
    size_t bytes = 0;
    for (auto t : m_materials[h.id].textures)
        bytes += TextureResidentBytes(t);
    return bytes;
}
size_t Renderer::EnvironmentResidentBytes(EnvironmentHandle h) const {
    return h.IsValid() && h.id < m_environments.size() &&
                   m_environments[h.id].alive
               ? m_environments[h.id].bytes
               : 0;
}

bool Renderer::HasViewDependentMeshLods() const {
    // The multiplier is set per renderable. The previous object's choice is
    // not a world-wide policy and cannot qualify reuse for another camera.
    for (const auto& mesh : m_meshes)
        if (mesh.alive && mesh.lods.size() > 1 && mesh.lodPixels > 0)
            return true;
    return false;
}

bool Renderer::ReuseShadowChart(int slot,const glm::mat4& matrix) {
    if(slot<0||slot>=kShadowMapCount||!m_shadowsEnabled||m_shadowPassActive||
       unsigned(slot)>=m_shadowLayers||!m_shadowContentValid[unsigned(slot)])return false;
    if(!m_render->viewUniforms.SetShadow(unsigned(slot),matrix))return false;
    m_shadowLightSpaceMatrix[slot]=matrix;
    ++m_stats.reusedShadowPasses;
    return true;
}

JudasRender::ShadowPassCache& Renderer::ShadowCache(){return m_render->shadowCache;}

void Renderer::SetDirectionalCascadeCount(unsigned n){glUseProgram(m_shaderProgram);glUniform1i(glGetUniformLocation(m_shaderProgram,"uSunCascadeCount"),GLint(std::min(n,3u)));}
