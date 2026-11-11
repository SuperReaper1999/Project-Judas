// Included by Renderer.cpp: context-owned resources, no gameplay or scene policy.
void Renderer::SetDynamicLights(const std::vector<DynamicLight>& lights){
 JUDAS_PROFILE_SCOPE("Light tile selection/upload");
 GLint viewport[4];glGetIntegerv(GL_VIEWPORT,viewport);
 m_lightGrid.Build(lights,m_view,m_projection,unsigned(std::max(1,viewport[2])),unsigned(std::max(1,viewport[3])));
 glUseProgram(m_shaderProgram);glUniform4f(glGetUniformLocation(m_shaderProgram,"uLightViewport"),float(viewport[0]),float(viewport[1]),float(viewport[2]),float(viewport[3]));
 glUniform1i(glGetUniformLocation(m_shaderProgram,"uHasDynamicLights"),m_lightGrid.visible!=0);
 m_stats.lightBytes=size_t(m_lightTextureRows)*1024*sizeof(glm::vec4);m_stats.shadowBytes=m_shadowsEnabled?size_t(m_shadowLayers)*kShadowMapResolution*kShadowMapResolution*4:0;
 if(!m_lightGrid.visible){m_stats.dynamicLights=0;m_stats.lightRejected+=m_lightGrid.rejected;return;}
 m_stats.dynamicLights=m_lightGrid.visible;m_stats.lightRejected+=m_lightGrid.rejected;m_stats.lightTileOverflow+=m_lightGrid.overflow;m_stats.maxTileLights=std::max(m_stats.maxTileLights,m_lightGrid.maxPerTile);
 const unsigned rows=std::max(1u,unsigned((m_lightGrid.texels.size()+1023)/1024));m_lightGrid.texels.resize(size_t(rows)*1024);
 if(!m_lightTexture)glGenTextures(1,&m_lightTexture);
 glActiveTexture(GL_TEXTURE14);glBindTexture(GL_TEXTURE_2D,m_lightTexture);
 if(rows>m_lightTextureRows){m_lightTextureRows=rows;glTexImage2D(GL_TEXTURE_2D,0,GL_RGBA32F,1024,GLsizei(rows),0,GL_RGBA,GL_FLOAT,nullptr);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_NEAREST);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_NEAREST);}
 glTexSubImage2D(GL_TEXTURE_2D,0,0,0,1024,GLsizei(rows),GL_RGBA,GL_FLOAT,m_lightGrid.texels.data());m_stats.uploadedBytes+=size_t(rows)*1024*sizeof(glm::vec4);++m_stats.uploadCalls;glActiveTexture(GL_TEXTURE0);
 m_stats.lightBytes=size_t(m_lightTextureRows)*1024*sizeof(glm::vec4);
 m_stats.shadowBytes=m_shadowsEnabled?size_t(m_shadowLayers)*kShadowMapResolution*kShadowMapResolution*4:0;
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
 if(!m_instanceBuffer)glGenBuffers(1,&m_instanceBuffer);
 for(const auto& batch:m_render->submission.Batches()){auto* mesh=GetMesh(batch.mesh);if(!mesh)continue;
  const auto bytes=batch.instances.size()*sizeof(Instance);
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
 glBindVertexArray(0);m_materialBindings=std::move(saved);m_renderLayer=layer;m_render->submission.Clear();
}

void Renderer::ReserveShadowLayers(unsigned layers){
 layers=std::min(layers,unsigned(kShadowMapCount));if(!m_shadowsEnabled||layers<=m_shadowLayers)return;
 // Grow only when a view actually needs more shadow sources. Existing depth
 // layers are copied, so low-level callers can add a layer after rendering one.
 GLint draw=0,read=0;glGetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING,&draw);glGetIntegerv(GL_READ_FRAMEBUFFER_BINDING,&read);
 GLuint next=0;glGenTextures(1,&next);glBindTexture(GL_TEXTURE_2D_ARRAY,next);
 
#if !defined(__ANDROID__) && !defined(__EMSCRIPTEN__)
 const bool cubeViews=m_capabilities.modernTier&&m_capabilities.textureViews&&m_capabilities.limits.fragmentTextureUnits>16;
 if(cubeViews){layers=std::min(unsigned(kShadowMapCount),((layers+5)/6)*6);glTexStorage3D(GL_TEXTURE_2D_ARRAY,1,GL_DEPTH_COMPONENT24,kShadowMapResolution,kShadowMapResolution,GLsizei(layers));}
 else
#endif
 glTexImage3D(GL_TEXTURE_2D_ARRAY,0,GL_DEPTH_COMPONENT24,kShadowMapResolution,kShadowMapResolution,GLsizei(layers),0,GL_DEPTH_COMPONENT,GL_UNSIGNED_INT,nullptr);
 glTexParameteri(GL_TEXTURE_2D_ARRAY,GL_TEXTURE_MIN_FILTER,GL_NEAREST);glTexParameteri(GL_TEXTURE_2D_ARRAY,GL_TEXTURE_MAG_FILTER,GL_NEAREST);glTexParameteri(GL_TEXTURE_2D_ARRAY,GL_TEXTURE_WRAP_S,GL_CLAMP_TO_EDGE);glTexParameteri(GL_TEXTURE_2D_ARRAY,GL_TEXTURE_WRAP_T,GL_CLAMP_TO_EDGE);
 GLuint temporary=0;glGenFramebuffers(1,&temporary);glBindFramebuffer(GL_FRAMEBUFFER,temporary);const GLenum none=GL_NONE;glDrawBuffers(1,&none);glReadBuffer(GL_NONE);
 for(unsigned i=0;i<m_shadowLayers;++i){glBindFramebuffer(GL_READ_FRAMEBUFFER,m_shadowFbo[i]);glReadBuffer(GL_NONE);glFramebufferTextureLayer(GL_DRAW_FRAMEBUFFER,GL_DEPTH_ATTACHMENT,next,0,GLint(i));glBlitFramebuffer(0,0,kShadowMapResolution,kShadowMapResolution,0,0,kShadowMapResolution,kShadowMapResolution,GL_DEPTH_BUFFER_BIT,GL_NEAREST);}
 glDeleteFramebuffers(1,&temporary);
 for(unsigned i=0;i<layers;++i){if(!m_shadowFbo[i])glGenFramebuffers(1,&m_shadowFbo[i]);glBindFramebuffer(GL_FRAMEBUFFER,m_shadowFbo[i]);glFramebufferTextureLayer(GL_FRAMEBUFFER,GL_DEPTH_ATTACHMENT,next,0,GLint(i));glDrawBuffers(1,&none);glReadBuffer(GL_NONE);}
 if(m_pointShadowView){glDeleteTextures(1,&m_pointShadowView);m_pointShadowView=0;}
#if !defined(__ANDROID__) && !defined(__EMSCRIPTEN__)
 if(cubeViews&&layers>kLocalShadowBase){
  glGenTextures(1,&m_pointShadowView);glTextureView(m_pointShadowView,GL_TEXTURE_CUBE_MAP_ARRAY,next,GL_DEPTH_COMPONENT24,0,1,kLocalShadowBase,layers-kLocalShadowBase);
  glBindTexture(GL_TEXTURE_CUBE_MAP_ARRAY,m_pointShadowView);glTexParameteri(GL_TEXTURE_CUBE_MAP_ARRAY,GL_TEXTURE_MIN_FILTER,GL_NEAREST);glTexParameteri(GL_TEXTURE_CUBE_MAP_ARRAY,GL_TEXTURE_MAG_FILTER,GL_NEAREST);glTexParameteri(GL_TEXTURE_CUBE_MAP_ARRAY,GL_TEXTURE_WRAP_S,GL_CLAMP_TO_EDGE);glTexParameteri(GL_TEXTURE_CUBE_MAP_ARRAY,GL_TEXTURE_WRAP_T,GL_CLAMP_TO_EDGE);glTexParameteri(GL_TEXTURE_CUBE_MAP_ARRAY,GL_TEXTURE_WRAP_R,GL_CLAMP_TO_EDGE);
  glActiveTexture(GL_TEXTURE16);glBindTexture(GL_TEXTURE_CUBE_MAP_ARRAY,m_pointShadowView);glActiveTexture(GL_TEXTURE0);
 }
#endif
 if(m_shadowArray)glDeleteTextures(1,&m_shadowArray);
 m_shadowArray=next;m_shadowLayers=layers;glBindFramebuffer(GL_DRAW_FRAMEBUFFER,GLuint(draw));glBindFramebuffer(GL_READ_FRAMEBUFFER,GLuint(read));glBindTexture(GL_TEXTURE_2D_ARRAY,0);
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
