// Same renderer, mesh shader, material/texture owners and GL backend. Sprites
// allocate no GPU resources until first use, and never enter physics/shadows.
void Renderer::SetCameraFrame(const CameraFrame& frame) {
    SetCamera(frame.view,frame.projection);
    glViewport(frame.content.x,frame.viewport.y-frame.content.y-frame.content.w,frame.content.z,frame.content.w);
    m_spritePixelGrid=frame.pixelGrid;
}

void Renderer::QueueSprite(const SpriteSettings& settings,glm::vec3 position,glm::quat rotation,
                           glm::vec3 scale,TextureHandle texture,glm::vec4 tint,std::uint64_t identity) {
    if(IsShadowPass()||!AcceptsCurrentLayer()||!ValidSprite(settings)||tint.a<=0)return;
    m_spriteDraws.push_back({settings,position,scale,rotation,texture,tint,identity,m_renderLayer});
}

void Renderer::FlushSprites() {
    if(m_spriteDraws.empty())return;
    JUDAS_PROFILE_SCOPE("Sprite submission");
    if(!m_spriteQuad.IsValid()) {
        MeshData quad;
        for(const auto uv:{glm::vec2(0,0),glm::vec2(1,0),glm::vec2(1,1),glm::vec2(0,1)})
            quad.vertices.push_back({{uv.x-.5f,uv.y-.5f,0},{0,0,1},uv});
        quad.indices={0,1,2,0,2,3};
        m_spriteQuad=CreateMesh(quad);
        for(unsigned i=0;i<2;++i) {
            MaterialDefinition material;
            material.model=MaterialModel::Unlit;material.alpha=MaterialAlpha::Blend;material.doubleSided=true;
            // The material owns its sampling state even when the image is an
            // external AssetId override. A tiny fallback creates that sampler
            // through the existing material resource owner.
            material.maps[0].embedded.width=material.maps[0].embedded.height=1;
            material.maps[0].embedded.pixels={255,255,255,255};
            material.maps[0].sampler.wrapS=material.maps[0].sampler.wrapT=GL_CLAMP_TO_EDGE;
            material.maps[0].sampler.minFilter=material.maps[0].sampler.magFilter=i?GL_NEAREST:GL_LINEAR;
            m_spriteMaterials[i]=CreateMaterial(material);
        }
    }
    std::stable_sort(m_spriteDraws.begin(),m_spriteDraws.end(),[](const auto& a,const auto& b){
        return SpriteSortKey(a.settings,a.position.y,a.identity)<SpriteSortKey(b.settings,b.position.y,b.identity);
    });
    const auto savedBindings=m_materialBindings;const auto savedLayer=m_renderLayer;
    const bool savedFlush=m_flushingBlends;
    m_flushingBlends=true; // already ordered; do not enqueue the mesh depth sort
    BeginTransparentPass();
    for(const auto& draw:m_spriteDraws) {
        auto position=draw.position;
        if(m_spritePixelGrid>0&&draw.settings.pixelSnap)
            position=SnapCameraPlane(position,glm::quat_cast(glm::mat3(glm::inverse(m_view))),m_spritePixelGrid);
        const auto centre=SpriteCentre(draw.settings);
        position+=draw.rotation*(draw.scale*glm::vec3(centre,0));
        auto scale=draw.scale*glm::vec3(draw.settings.size*glm::vec2(draw.settings.flipX?-1:1,draw.settings.flipY?-1:1),1);
        MaterialBinding binding;binding.handle=m_spriteMaterials[draw.settings.nearest?1:0];
        binding.overrides.uvScale=glm::vec2(draw.settings.rect.z,draw.settings.rect.w);
        binding.overrides.uvOffset=glm::vec2(draw.settings.rect.x,1-draw.settings.rect.y-draw.settings.rect.w);
        binding.textureOverrides[0]=true;binding.textures[0]=draw.texture;
        m_materialBindings={binding};m_renderLayer=draw.layer;
        DrawMesh(m_spriteQuad,position,draw.rotation,scale,draw.texture,glm::vec3(draw.tint),draw.tint.a);
    }
    EndTransparentPass();m_flushingBlends=savedFlush;m_materialBindings=savedBindings;m_renderLayer=savedLayer;
    m_spriteDraws.clear();
}
