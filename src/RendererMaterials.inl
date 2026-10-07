// Included by Renderer.cpp. All GL operations stay in Renderer ownership.
TextureHandle Renderer::ColourTexture(TextureHandle source){
 if(!source.IsValid()||source.id>=m_textures.size()||!m_textures[source.id].alive)return source;
 const auto& t=m_textures[source.id];
 // Modern camera targets already store linear radiance. Legacy display targets
 // remain live raw images; immutable asset colour views must not freeze them.
 if(t.srgb||t.sceneLinear||t.renderTarget)return source;
 if(t.colourView.IsValid()&&m_textures[t.colourView.id].alive)return t.colourView;
 // GL 3.3 has no texture views: reuse decoded texel bytes once, on the owner
 // thread, then cache a role-correct sRGB upload. No asset decoding here.
 TextureData data;if(!ReadTextureForDiagnostics(source,data))return source;
 auto view=CreateTexture(data,true);m_textures[source.id].colourView=view;return view;
}
GLint Renderer::MaterialUniform(const char* name){auto it=m_materialUniforms.find(name);if(it!=m_materialUniforms.end())return it->second;auto location=glGetUniformLocation(m_shaderProgram,name);m_materialUniforms.emplace(name,location);return location;}
MaterialHandle Renderer::CreateMaterial(const MaterialDefinition& definition){
 std::string error;if(!ValidateMaterial(definition,error))return {};
 GpuMaterial m;m.alive=true;m.definition=MaterialSettings(definition);
 for(size_t i=0;i<5;++i){const auto& image=definition.maps[i];if(!image.embedded.pixels.empty()){m.textures[i]=CreateTexture(image.embedded,(i==0||i==4)&&definition.model!=MaterialModel::Legacy);glGenSamplers(1,&m.samplers[i]);auto& s=image.sampler;glSamplerParameteri(m.samplers[i],GL_TEXTURE_WRAP_S,s.wrapS);glSamplerParameteri(m.samplers[i],GL_TEXTURE_WRAP_T,s.wrapT);glSamplerParameteri(m.samplers[i],GL_TEXTURE_MIN_FILTER,s.minFilter);glSamplerParameteri(m.samplers[i],GL_TEXTURE_MAG_FILTER,s.magFilter);}m.definition.maps[i].embedded=TextureData{};}
 // Encoded source images belong to the immutable CPU resource/cooked archive.
 // Retaining them here would copy megabytes in ApplyMaterialOverride on every
 // draw. The GPU material only needs uploaded handles and lightweight settings.
 for(auto& map:m.definition.maps)std::vector<uint8_t>().swap(map.encodedImage);
 MaterialHandle handle{unsigned(m_materials.size())};m_materials.push_back(std::move(m));return handle;
}
bool Renderer::UploadMeshMap(MeshHandle mesh,const MeshData& data,size_t material,size_t map){
 JUDAS_PROFILE_SCOPE("Mesh texture upload unit");
 auto* gpu=GetMesh(mesh);if(!gpu||material>=gpu->materials.size()||map>=5)return false;
 auto handle=gpu->materials[material];if(!handle.IsValid()||handle.id>=m_materials.size())return false;
 const auto& definition=data.materials.at(material);const auto& image=definition.maps[map];
 auto& m=m_materials[handle.id];if(image.embedded.pixels.empty())return true;
 m.textures[map]=CreateTexture(image.embedded,(map==0||map==4)&&definition.model!=MaterialModel::Legacy);
 if(!m.textures[map].IsValid())return false;
 glGenSamplers(1,&m.samplers[map]);auto& s=image.sampler;
 glSamplerParameteri(m.samplers[map],GL_TEXTURE_WRAP_S,s.wrapS);glSamplerParameteri(m.samplers[map],GL_TEXTURE_WRAP_T,s.wrapT);
 glSamplerParameteri(m.samplers[map],GL_TEXTURE_MIN_FILTER,s.minFilter);glSamplerParameteri(m.samplers[map],GL_TEXTURE_MAG_FILTER,s.magFilter);return true;
}
void Renderer::DestroyMaterial(MaterialHandle handle){if(!handle.IsValid()||handle.id>=m_materials.size())return;auto& m=m_materials[handle.id];if(!m.alive)return;for(auto t:m.textures)DestroyTexture(t);for(auto s:m.samplers)if(s)glDeleteSamplers(1,&s);m={};}
EnvironmentHandle Renderer::CreateEnvironment(const EnvironmentData& data){
 if(data.specular.empty()||data.diffuse.pixels.empty()||data.brdf.empty())return {};
 GpuEnvironment e;e.alive=true;e.levels=unsigned(data.specular.size());GLint previous;glGetIntegerv(GL_TEXTURE_BINDING_2D,&previous);
 auto setup=[](GLuint texture,bool mip){glBindTexture(GL_TEXTURE_2D,texture);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,mip?GL_LINEAR_MIPMAP_LINEAR:GL_LINEAR);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_LINEAR);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_S,GL_REPEAT);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_T,GL_CLAMP_TO_EDGE);};
 glGenTextures(1,&e.specular);setup(e.specular,true);for(size_t i=0;i<data.specular.size();++i){auto& l=data.specular[i];glTexImage2D(GL_TEXTURE_2D,GLint(i),GL_RGB16F,l.width,l.height,0,GL_RGB,GL_FLOAT,l.pixels.data());e.bytes+=size_t(l.width)*l.height*6;}glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAX_LEVEL,GLint(data.specular.size()-1));
 glGenTextures(1,&e.diffuse);setup(e.diffuse,false);auto& d=data.diffuse;glTexImage2D(GL_TEXTURE_2D,0,GL_RGB16F,d.width,d.height,0,GL_RGB,GL_FLOAT,d.pixels.data());e.bytes+=size_t(d.width)*d.height*6;
 glGenTextures(1,&e.brdf);setup(e.brdf,false);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_S,GL_CLAMP_TO_EDGE);glTexImage2D(GL_TEXTURE_2D,0,GL_RG16F,data.brdfSize,data.brdfSize,0,GL_RG,GL_FLOAT,data.brdf.data());e.bytes+=size_t(data.brdfSize)*data.brdfSize*4;
 glBindTexture(GL_TEXTURE_2D,GLuint(previous));EnvironmentHandle handle{unsigned(m_environments.size())};m_environments.push_back(e);return handle;
}
void Renderer::DestroyEnvironment(EnvironmentHandle handle){if(!handle.IsValid()||handle.id>=m_environments.size())return;auto& e=m_environments[handle.id];if(!e.alive)return;glDeleteTextures(1,&e.specular);glDeleteTextures(1,&e.diffuse);glDeleteTextures(1,&e.brdf);e={};}
void Renderer::SetSceneAppearance(bool linear,float exposure,EnvironmentHandle environment,float intensity,const glm::quat& rotation,bool background,const glm::vec3& colour){glClearColor(colour.x,colour.y,colour.z,1);m_linearRendering=linear;m_exposure=exposure;m_environment=environment;m_environmentIntensity=intensity;m_environmentRotation=glm::normalize(rotation);m_environmentBackground=background;}
void Renderer::BindMaterial(const GpuMaterial* gpu,const MaterialOverride& overrides,TextureHandle generated,const glm::vec3&,float alpha,bool shadow){
 MaterialDefinition legacy;legacy.model=MaterialModel::Legacy;
 auto m=ApplyMaterialOverride(gpu?gpu->definition:legacy,overrides);
 auto program=shadow?m_shadowShaderProgram:m_shaderProgram;
 auto location=[&](const char* n){return shadow?glGetUniformLocation(program,n):MaterialUniform(n);};
 glUniform1i(location("uAlphaMode"),int(m.alpha));glUniform1f(location("uAlphaCutoff"),m.alphaCutoff);glUniform1i(location("uFlipV"),m.flipV);glUniform4f(location("uUVTransform"),m.uvScale.x,m.uvScale.y,m.uvOffset.x,m.uvOffset.y);
 for(int i=0;i<5;++i){auto& map=m.maps[i];std::string index="["+std::to_string(i)+"]";glUniform1i(location(("uMapUVSet"+index).c_str()),map.uvSet);glUniform4f(location(("uMapUVTransform"+index).c_str()),map.scale.x,map.scale.y,map.offset.x,map.offset.y);glUniform1f(location(("uMapUVRotation"+index).c_str()),map.rotation);}
 TextureHandle base=generated;if(!generated.IsValid()&&gpu)base=gpu->textures[0];
 if(!shadow&&m.model!=MaterialModel::Legacy)base=ColourTexture(base);
 if(m_activeTarget.IsValid()&&base.IsValid()&&base.id==RenderTargetTexture(m_activeTarget).id){base={};++m_stats.feedbackFallbacks;}
 glActiveTexture(GL_TEXTURE0);glBindTexture(GL_TEXTURE_2D,ResolveTexture(base));glBindSampler(0,gpu&&!generated.IsValid()?gpu->samplers[0]:0);
 if(shadow){glUniform1i(location("uTexture"),0);glUniform1f(location("uAlphaFactor"),m.baseColor.a*alpha);}
 else{
  glUniform1i(location("uMaterialModel"),int(m.model));glUniform1i(location("uModern"),m_linearRendering);glUniform1i(location("uBaseLinear"),base.IsValid()&&base.id<m_textures.size()&&(m_textures[base.id].sceneLinear||m_textures[base.id].srgb));
  glUniform4fv(location("uBaseFactor"),1,glm::value_ptr(m.baseColor));glUniform1f(location("uMetallic"),m.metallic);glUniform1f(location("uRoughness"),m.roughness);glUniform1f(location("uNormalStrength"),m.normalStrength);glUniform1f(location("uOcclusionStrength"),m.occlusionStrength);glUniform3fv(location("uEmission"),1,glm::value_ptr(m.emissive));glUniform1f(location("uEmissionIntensity"),m.emissiveIntensity);
  auto eye=glm::vec3(glm::inverse(m_view)[3]);glUniform3fv(location("uCameraPosition"),1,glm::value_ptr(eye));
  glUniform1i(location("uEmissiveLinear"),gpu&&gpu->textures[4].IsValid()&&m_textures[gpu->textures[4].id].srgb);
  const char* names[]={"uMR","uNormal","uOcclusion","uEmissive"};const char* present[]={"uHasMR","uHasNormal","uHasOcclusion","uHasEmissive"};
  for(unsigned i=1;i<5;++i){glActiveTexture(GL_TEXTURE0+4+i);auto texture=gpu?gpu->textures[i]:TextureHandle{};glBindTexture(GL_TEXTURE_2D,ResolveTexture(texture));glBindSampler(4+i,gpu?gpu->samplers[i]:0);glUniform1i(location(names[i-1]),int(4+i));glUniform1i(location(present[i-1]),texture.IsValid());}
  const GpuEnvironment* env=m_environment.IsValid()&&m_environment.id<m_environments.size()&&m_environments[m_environment.id].alive?&m_environments[m_environment.id]:nullptr;
  glUniform1i(location("uEnvironmentEnabled"),env!=nullptr);glUniform1f(location("uEnvironmentIntensity"),m_environmentIntensity);glUniform1f(location("uEnvLevels"),env?float(env->levels):1);auto inverse=glm::mat3_cast(glm::conjugate(m_environmentRotation));glUniformMatrix3fv(location("uEnvironmentInverse"),1,GL_FALSE,glm::value_ptr(inverse));
  const char* envNames[]={"uEnvDiffuse","uEnvSpecular","uBRDF"};GLuint tex[]={env?env->diffuse:0,env?env->specular:0,env?env->brdf:0};for(int i=0;i<3;++i){glActiveTexture(GL_TEXTURE0+9+i);glBindTexture(GL_TEXTURE_2D,tex[i]);glBindSampler(9+i,0);glUniform1i(location(envNames[i]),9+i);}
 }
 if(m.model==MaterialModel::Legacy||m.doubleSided)glDisable(GL_CULL_FACE);else {glEnable(GL_CULL_FACE);glCullFace(GL_BACK);}
 glActiveTexture(GL_TEXTURE0);
}
void Renderer::FlushMaterialBlends(){
 if(m_blendDraws.empty())return;
 JUDAS_PROFILE_SCOPE("Material transparency");RendererProfileScope gpu(*this,"Material transparency");
 std::stable_sort(m_blendDraws.begin(),m_blendDraws.end(),[](const auto& a,const auto& b){return a.depth>b.depth;});auto bindings=m_materialBindings;auto layer=m_renderLayer;
 m_flushingBlends=true;BeginTransparentPass();for(auto& d:m_blendDraws){m_renderLayer=d.layer;m_materialBindings=d.materials;DrawMesh(d.mesh,d.position,d.rotation,d.scale,d.texture,d.tint,d.alpha,d.skin.empty()?nullptr:&d.skin,&d.hiddenParts);}EndTransparentPass();m_flushingBlends=false;m_materialBindings=std::move(bindings);m_renderLayer=layer;m_blendDraws.clear();
}
void Renderer::BeginLinearPass(int width,int height){
 if(m_linearPass)return;
 glGetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING,&m_outputFramebuffer);
 // Cache by camera output size: main and M33 targets do not reallocate each frame.
 auto existing=std::find_if(m_linearTargets.begin(),m_linearTargets.end(),[&](const auto& t){return t.width==width&&t.height==height;});
 if(existing==m_linearTargets.end()){
  LinearTarget target;target.width=width;target.height=height;glGenFramebuffers(1,&target.fbo);glBindFramebuffer(GL_FRAMEBUFFER,target.fbo);
  auto texture=[&](GLuint& t,GLint format,GLenum channels,GLenum type){glGenTextures(1,&t);glBindTexture(GL_TEXTURE_2D,t);glTexImage2D(GL_TEXTURE_2D,0,format,width,height,0,channels,type,nullptr);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_NEAREST);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_NEAREST);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_S,GL_CLAMP_TO_EDGE);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_T,GL_CLAMP_TO_EDGE);};
  texture(target.color,GL_RGBA16F,GL_RGBA,GL_FLOAT);texture(target.depth,GL_DEPTH_COMPONENT24,GL_DEPTH_COMPONENT,GL_UNSIGNED_INT);glFramebufferTexture2D(GL_FRAMEBUFFER,GL_COLOR_ATTACHMENT0,GL_TEXTURE_2D,target.color,0);glFramebufferTexture2D(GL_FRAMEBUFFER,GL_DEPTH_ATTACHMENT,GL_TEXTURE_2D,target.depth,0);
  if(glCheckFramebufferStatus(GL_FRAMEBUFFER)!=GL_FRAMEBUFFER_COMPLETE){std::fprintf(stderr,"HDR framebuffer incomplete\n");glDeleteFramebuffers(1,&target.fbo);glDeleteTextures(1,&target.color);glDeleteTextures(1,&target.depth);glBindFramebuffer(GL_FRAMEBUFFER,GLuint(m_outputFramebuffer));m_linearRendering=false;return;}
  // Bounded resize cache. Old dimensions are never live during this outer boundary.
  if(m_linearTargets.size()==8){auto old=m_linearTargets.front();glDeleteFramebuffers(1,&old.fbo);glDeleteTextures(1,&old.color);glDeleteTextures(1,&old.depth);m_linearTargets.erase(m_linearTargets.begin());}
  m_linearTargets.push_back(target);existing=m_linearTargets.end()-1;
 }
 m_hdrFbo=existing->fbo;m_hdrColor=existing->color;m_hdrDepth=existing->depth;m_hdrWidth=width;m_hdrHeight=height;
 glBindFramebuffer(GL_FRAMEBUFFER,m_hdrFbo);glDisable(GL_FRAMEBUFFER_SRGB);glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT);m_linearPass=true;
}
void Renderer::ResolveLinearPass(bool sceneLinear){
 if(!m_linearPass)return;
 JUDAS_PROFILE_SCOPE("Display resolve");RendererProfileScope gpu(*this,"Display resolve",m_activeTarget.IsValid()?m_activeTarget.id:0);
 if(!m_outputProgram){const char* vs=R"(#version 330 core
out vec2 uv;void main(){vec2 p=vec2((gl_VertexID<<1)&2,gl_VertexID&2);uv=p;gl_Position=vec4(p*2.0-1.0,0,1);})";
 const char* fs=R"(#version 330 core
in vec2 uv;out vec4 result;uniform sampler2D image,depthImage,environment;uniform bool background,linearOutput;uniform float exposure,intensity;uniform mat4 inverseViewProjection;uniform mat3 environmentInverse;uniform vec3 eye;
vec3 encode(vec3 c){return mix(c*12.92,1.055*pow(c,vec3(1.0/2.4))-0.055,step(vec3(0.0031308),c));}
void main(){vec3 colour=texture(image,uv).rgb;if(background&&texture(depthImage,uv).r>=0.999999){vec4 w=inverseViewProjection*vec4(uv*2.0-1.0,1,1);vec3 d=environmentInverse*normalize(w.xyz/w.w-eye);vec2 e=vec2(atan(d.z,d.x)/6.28318530718,acos(clamp(d.y,-1.0,1.0))/3.14159265359);colour=textureLod(environment,e,0.0).rgb*intensity;}if(linearOutput){result=vec4(max(colour,vec3(0.0)),1);return;}colour=max(colour,vec3(0.0))*exposure;float luminance=dot(colour,vec3(.2126,.7152,.0722));colour/=1.0+luminance;result=vec4(encode(clamp(colour,0.0,1.0)),1.0);})";
 GLuint v=0,f=0;if(!CompileShader(GL_VERTEX_SHADER,vs,v)||!CompileShader(GL_FRAGMENT_SHADER,fs,f)||!LinkProgram(v,f,m_outputProgram)){glDeleteShader(v);glDeleteShader(f);m_linearPass=false;return;}glDeleteShader(v);glDeleteShader(f);glGenVertexArrays(1,&m_outputVao);
 }
 glBindFramebuffer(GL_FRAMEBUFFER,GLuint(m_outputFramebuffer));glViewport(0,0,m_hdrWidth,m_hdrHeight);glDisable(GL_DEPTH_TEST);glDisable(GL_BLEND);glDisable(GL_CULL_FACE);glDisable(GL_FRAMEBUFFER_SRGB);glUseProgram(m_outputProgram);
 auto loc=[&](const char* n){return glGetUniformLocation(m_outputProgram,n);};
 glActiveTexture(GL_TEXTURE0);glBindSampler(0,0);glBindTexture(GL_TEXTURE_2D,m_hdrColor);glUniform1i(loc("image"),0);glActiveTexture(GL_TEXTURE1);glBindTexture(GL_TEXTURE_2D,m_hdrDepth);glBindSampler(1,0);glUniform1i(loc("depthImage"),1);
 const GpuEnvironment* e=m_environment.IsValid()&&m_environment.id<m_environments.size()&&m_environments[m_environment.id].alive?&m_environments[m_environment.id]:nullptr;glActiveTexture(GL_TEXTURE2);glBindSampler(2,0);glBindTexture(GL_TEXTURE_2D,e?e->specular:0);glUniform1i(loc("environment"),2);glUniform1i(loc("background"),m_environmentBackground&&e);glUniform1i(loc("linearOutput"),sceneLinear);glUniform1f(loc("exposure"),m_exposure);glUniform1f(loc("intensity"),m_environmentIntensity);auto inv=glm::inverse(m_projection*m_view);auto rotation=glm::mat3_cast(glm::conjugate(m_environmentRotation));auto eye=glm::vec3(glm::inverse(m_view)[3]);glUniformMatrix4fv(loc("inverseViewProjection"),1,GL_FALSE,glm::value_ptr(inv));glUniformMatrix3fv(loc("environmentInverse"),1,GL_FALSE,glm::value_ptr(rotation));glUniform3fv(loc("eye"),1,glm::value_ptr(eye));glBindVertexArray(m_outputVao);glDrawArrays(GL_TRIANGLES,0,3);glBindVertexArray(0);
 // Preserve scene depth for the editor gizmo/debug pass after display resolve.
 glBindFramebuffer(GL_READ_FRAMEBUFFER,m_hdrFbo);glBindFramebuffer(GL_DRAW_FRAMEBUFFER,GLuint(m_outputFramebuffer));glBlitFramebuffer(0,0,m_hdrWidth,m_hdrHeight,0,0,m_hdrWidth,m_hdrHeight,GL_DEPTH_BUFFER_BIT,GL_NEAREST);glBindFramebuffer(GL_FRAMEBUFFER,GLuint(m_outputFramebuffer));glActiveTexture(GL_TEXTURE0);glEnable(GL_DEPTH_TEST);m_linearPass=false;
}
std::size_t Renderer::AppearanceBytes()const{size_t bytes=0;for(auto& t:m_linearTargets)bytes+=size_t(t.width)*t.height*11;for(auto& e:m_environments)if(e.alive)bytes+=e.bytes;for(const auto& texture:m_textures)if(texture.alive&&texture.colourView.IsValid()&&m_textures[texture.colourView.id].alive)bytes+=m_textures[texture.colourView.id].uploadedBytes;return bytes;}
void Renderer::ShutdownAppearance(){m_blendDraws.clear();for(unsigned i=0;i<m_materials.size();++i)DestroyMaterial({i});for(unsigned i=0;i<m_environments.size();++i)DestroyEnvironment({i});m_materials.clear();m_environments.clear();for(auto& t:m_linearTargets){glDeleteFramebuffers(1,&t.fbo);glDeleteTextures(1,&t.color);glDeleteTextures(1,&t.depth);}m_linearTargets.clear();if(m_outputProgram)glDeleteProgram(m_outputProgram);if(m_outputVao)glDeleteVertexArrays(1,&m_outputVao);m_hdrFbo=m_hdrColor=m_hdrDepth=m_outputProgram=m_outputVao=0;m_hdrWidth=m_hdrHeight=0;m_linearPass=false;m_materialUniforms.clear();}
