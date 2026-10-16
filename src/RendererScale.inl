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
 glTexSubImage2D(GL_TEXTURE_2D,0,0,0,1024,GLsizei(rows),GL_RGBA,GL_FLOAT,m_lightGrid.texels.data());glActiveTexture(GL_TEXTURE0);
 m_stats.lightBytes=size_t(m_lightTextureRows)*1024*sizeof(glm::vec4);
 m_stats.shadowBytes=m_shadowsEnabled?size_t(m_shadowLayers)*kShadowMapResolution*kShadowMapResolution*4:0;
}
void Renderer::BeginMeshBatch(){
 if(m_collectingBatch)return;
 m_meshBatches.clear();m_batchLookup.clear();m_collectingBatch=true;
}
void Renderer::QueueMeshInstance(MeshHandle mesh,TextureHandle texture,const glm::mat4& model,glm::vec4 tint){
 // Exact material state defines compatibility. Tint and transform are instance
 // attributes, not grouping keys. Immutable mesh handles are never retained
 // across the pass, resource replacement or scene lifetime.
 std::string key;auto append=[&](const auto& value){key.append(reinterpret_cast<const char*>(&value),sizeof(value));};
 append(mesh.id);append(texture.id);bool reversed=glm::determinant(glm::mat3(model))<0;append(reversed);
 for(const auto& b:m_materialBindings){append(b.handle.id);append(b.explicitAsset);append(b.failed);for(unsigned i=0;i<5;++i){append(b.textures[i].id);append(b.textureOverrides[i]);}auto text=EncodeMaterialOverrides(b.overrides);key+=std::to_string(text.size())+":"+text;}
 auto [it,inserted]=m_batchLookup.emplace(key,m_meshBatches.size());if(inserted)m_meshBatches.push_back({mesh,texture,m_materialBindings,reversed,{}});
 m_meshBatches[it->second].instances.push_back({model,tint});
 // A bounded pass flush avoids an unbounded transient upload on huge worlds.
 if(m_meshBatches[it->second].instances.size()==16384){EndMeshBatch();BeginMeshBatch();}
}
void Renderer::EndMeshBatch(){
 if(!m_collectingBatch)return;
 m_collectingBatch=false;
 if(m_meshBatches.empty())return;
 JUDAS_PROFILE_SCOPE("Opaque instance submission");RendererProfileScope gpu(*this,"Opaque instances");
 auto saved=m_materialBindings;auto layer=m_renderLayer;
 if(!m_instanceBuffer)glGenBuffers(1,&m_instanceBuffer);
 for(auto& batch:m_meshBatches){auto* mesh=GetMesh(batch.mesh);if(!mesh)continue;
  glBindVertexArray(mesh->vao);glBindBuffer(GL_ARRAY_BUFFER,m_instanceBuffer);auto bytes=batch.instances.size()*sizeof(Instance);glBufferData(GL_ARRAY_BUFFER,GLsizeiptr(bytes),batch.instances.data(),GL_STREAM_DRAW);m_stats.instanceBytes=std::max(m_stats.instanceBytes,bytes);
  for(unsigned c=0;c<4;++c){glEnableVertexAttribArray(9+c);glVertexAttribPointer(9+c,4,GL_FLOAT,GL_FALSE,sizeof(Instance),reinterpret_cast<void*>(sizeof(glm::vec4)*c));glVertexAttribDivisor(9+c,1);}
  glEnableVertexAttribArray(13);glVertexAttribPointer(13,4,GL_FLOAT,GL_FALSE,sizeof(Instance),reinterpret_cast<void*>(offsetof(Instance,tint)));glVertexAttribDivisor(13,1);
  m_materialBindings=batch.bindings;m_submittingBatch=true;m_activeInstances=unsigned(batch.instances.size());m_activeBatchReversed=batch.reversed;
  DrawMesh(batch.mesh,{0,0,0},{1,0,0,0},{1,1,1},batch.texture,{1,1,1},1);
  m_submittingBatch=false;glBindVertexArray(mesh->vao);for(unsigned c=9;c<=13;++c){glDisableVertexAttribArray(c);glVertexAttribDivisor(c,0);}
 }
 glBindVertexArray(0);m_materialBindings=std::move(saved);m_renderLayer=layer;m_meshBatches.clear();m_batchLookup.clear();
}

void Renderer::ReserveShadowLayers(unsigned layers){
 layers=std::min(layers,unsigned(kShadowMapCount));if(!m_shadowsEnabled||layers<=m_shadowLayers)return;
 // Grow only when a view actually needs more shadow sources. Existing depth
 // layers are copied, so low-level callers can add a layer after rendering one.
 GLint draw=0,read=0;glGetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING,&draw);glGetIntegerv(GL_READ_FRAMEBUFFER_BINDING,&read);
 GLuint next=0;glGenTextures(1,&next);glBindTexture(GL_TEXTURE_2D_ARRAY,next);
 glTexImage3D(GL_TEXTURE_2D_ARRAY,0,GL_DEPTH_COMPONENT24,kShadowMapResolution,kShadowMapResolution,GLsizei(layers),0,GL_DEPTH_COMPONENT,GL_UNSIGNED_INT,nullptr);
 glTexParameteri(GL_TEXTURE_2D_ARRAY,GL_TEXTURE_MIN_FILTER,GL_NEAREST);glTexParameteri(GL_TEXTURE_2D_ARRAY,GL_TEXTURE_MAG_FILTER,GL_NEAREST);glTexParameteri(GL_TEXTURE_2D_ARRAY,GL_TEXTURE_WRAP_S,GL_CLAMP_TO_EDGE);glTexParameteri(GL_TEXTURE_2D_ARRAY,GL_TEXTURE_WRAP_T,GL_CLAMP_TO_EDGE);
 GLuint temporary=0;glGenFramebuffers(1,&temporary);glBindFramebuffer(GL_FRAMEBUFFER,temporary);const GLenum none=GL_NONE;glDrawBuffers(1,&none);glReadBuffer(GL_NONE);
 for(unsigned i=0;i<m_shadowLayers;++i){glBindFramebuffer(GL_READ_FRAMEBUFFER,m_shadowFbo[i]);glReadBuffer(GL_NONE);glFramebufferTextureLayer(GL_DRAW_FRAMEBUFFER,GL_DEPTH_ATTACHMENT,next,0,GLint(i));glBlitFramebuffer(0,0,kShadowMapResolution,kShadowMapResolution,0,0,kShadowMapResolution,kShadowMapResolution,GL_DEPTH_BUFFER_BIT,GL_NEAREST);}
 glDeleteFramebuffers(1,&temporary);
 for(unsigned i=0;i<layers;++i){if(!m_shadowFbo[i])glGenFramebuffers(1,&m_shadowFbo[i]);glBindFramebuffer(GL_FRAMEBUFFER,m_shadowFbo[i]);glFramebufferTextureLayer(GL_FRAMEBUFFER,GL_DEPTH_ATTACHMENT,next,0,GLint(i));glDrawBuffers(1,&none);glReadBuffer(GL_NONE);}
 if(m_shadowArray)glDeleteTextures(1,&m_shadowArray);
 m_shadowArray=next;m_shadowLayers=layers;glBindFramebuffer(GL_DRAW_FRAMEBUFFER,GLuint(draw));glBindFramebuffer(GL_READ_FRAMEBUFFER,GLuint(read));glBindTexture(GL_TEXTURE_2D_ARRAY,0);
}
