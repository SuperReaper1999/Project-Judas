// Renderer owns atlas placement and GL. Layouts retain only CPU glyph identities.
void Renderer::ResetProjectText(){for(auto& p:m_textPages)DestroyTexture(p.texture);m_textPages.clear();m_atlasGlyphs.clear();++m_textAtlasGeneration;m_textEngine.Clear();m_uiFonts.clear();if(m_defaultTextFont)m_uiFonts[m_defaultUIFont]=m_defaultTextFont;m_textFonts=m_defaultTextFont?std::vector<std::shared_ptr<const TextFont>>{m_defaultTextFont}:std::vector<std::shared_ptr<const TextFont>>{};}
void Renderer::SelectTextFonts(std::vector<std::shared_ptr<const TextFont>> fonts){if(fonts.empty()&&m_defaultTextFont)fonts.push_back(m_defaultTextFont);m_textFonts=std::move(fonts);}
std::shared_ptr<const TextLayout> Renderer::LayoutText(const std::string& s,const TextOptions& o)const{try{auto layout=m_textEngine.Layout(s,m_textFonts,o);static unsigned missing=0;if(layout->missingClusters&&missing++<16)std::fprintf(stderr,"Text coverage missing for %u clusters; primary-font .notdef shown\n",layout->missingClusters);return layout;}catch(const std::exception& e){static unsigned warnings=0;if(warnings++<8)std::fprintf(stderr,"Text layout: %s\n",e.what());return std::make_shared<TextLayout>();}}
void Renderer::DrawTextLayout(const TextLayout& layout,glm::vec2 position,glm::vec4 tint){
 glUniform1i(m_uiUSceneLinear,0);
 JUDAS_PROFILE_SCOPE("Shaped text submission");glUniform4f(m_uiUColor,tint.r,tint.g,tint.b,tint.a);
 glUniform1i(m_uiUTextVertices,1);glUniform2f(m_uiUUVOffset,0,0);glUniform2f(m_uiUUVScale,1,1);glBindVertexArray(m_textVao);
 std::vector<glm::vec4> vertices;vertices.reserve(layout.glyphs.size()*6);unsigned batchPage=~0u;
 auto flush=[&](){if(vertices.empty())return;glBindTexture(GL_TEXTURE_2D,ResolveTexture(m_textPages[batchPage].texture));glBindBuffer(GL_ARRAY_BUFFER,m_textVbo);glBufferData(GL_ARRAY_BUFFER,GLsizeiptr(vertices.size()*sizeof(glm::vec4)),vertices.data(),GL_STREAM_DRAW);glDrawArrays(GL_TRIANGLES,0,GLsizei(vertices.size()));++m_uiDrawCalls;vertices.clear();};
 for(auto& glyph:layout.glyphs){if(m_atlasGlyphs.size()>=32768){flush();batchPage=~0u;m_atlasGlyphs.clear();}std::string key=glyph.font->revision+":"+std::to_string(glyph.glyph)+":"+std::to_string(unsigned(std::ceil(layout.pixels)));auto it=m_atlasGlyphs.find(key);
  if(it==m_atlasGlyphs.end()){TextRaster raster;std::string error;if(!m_textEngine.Raster(glyph,layout.pixels,raster,error)){static unsigned warnings=0;if(warnings++<16)std::fprintf(stderr,"Text raster: %s; .notdef fallback\n",error.c_str());auto replacement=glyph;replacement.glyph=0;if(!m_textEngine.Raster(replacement,layout.pixels,raster,error))continue;}
   AtlasGlyph placement{};placement.raster=std::move(raster);auto& b=placement.raster;
   if(b.width>1020||b.height>1020){static unsigned warnings=0;if(warnings++<8)std::fprintf(stderr,"Text glyph too large for bounded atlas\n");continue;}
   if(b.width&&b.height){bool fit=false;for(unsigned page=0;page<m_textPages.size();++page){auto& p=m_textPages[page];if(p.x+b.width+2>1024){p.x=1;p.y+=p.row+2;p.row=0;}if(p.y+b.height+2<=1024){placement.page=page;fit=true;break;}}
    if(!fit){if(m_textPages.size()>=8){flush();batchPage=~0u;for(auto& p:m_textPages)DestroyTexture(p.texture);m_textPages.clear();m_atlasGlyphs.clear();++m_textAtlasGeneration;}TextureData tex;tex.width=tex.height=1024;tex.pixels.assign(1024*1024*4,255);for(size_t i=3;i<tex.pixels.size();i+=4)tex.pixels[i]=0;auto handle=CreateTexture(tex);glBindTexture(GL_TEXTURE_2D,ResolveTexture(handle));glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_LINEAR);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_LINEAR);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAX_LEVEL,0);m_textPages.push_back({handle,1,1,0});placement.page=unsigned(m_textPages.size()-1);}
    auto& page=m_textPages[placement.page];placement.x=page.x;placement.y=page.y;page.x+=b.width+2;page.row=std::max(page.row,b.height);
    std::vector<uint8_t> rgba(size_t(b.width)*b.height*4);for(size_t i=0;i<b.coverage.size();++i){rgba[i*4]=rgba[i*4+1]=rgba[i*4+2]=255;rgba[i*4+3]=b.coverage[i];}glBindTexture(GL_TEXTURE_2D,ResolveTexture(page.texture));glTexSubImage2D(GL_TEXTURE_2D,0,placement.x,placement.y,b.width,b.height,GL_RGBA,GL_UNSIGNED_BYTE,rgba.data());b.coverage.clear();b.coverage.shrink_to_fit();
   }
   it=m_atlasGlyphs.emplace(key,std::move(placement)).first;
  }
  auto& a=it->second;auto& b=a.raster;if(!b.width||!b.height)continue;if(batchPage!=a.page){flush();batchPage=a.page;}float x=position.x+glyph.x+b.left,y=position.y+glyph.y-b.top,u=a.x/1024.f,v=a.y/1024.f,du=b.width/1024.f,dv=b.height/1024.f;
  vertices.insert(vertices.end(),{{x,y,u,v},{x+b.width,y,u+du,v},{x+b.width,y+b.height,u+du,v+dv},{x,y,u,v},{x+b.width,y+b.height,u+du,v+dv},{x,y+b.height,u,v+dv}});
 }
 flush();glUniform1i(m_uiUTextVertices,0);glBindVertexArray(m_uiQuadVao);glBindBuffer(GL_ARRAY_BUFFER,0);
 auto stats=m_textEngine.Stats();JUDAS_PROFILE_COUNTER("Text atlas bytes",double(TextAtlasBytes()),ProfileCounterMode::Latest);JUDAS_PROFILE_COUNTER("Text layout bytes",double(stats.layoutBytes),ProfileCounterMode::Latest);JUDAS_PROFILE_COUNTER("Text glyph raster total",double(stats.rasters),ProfileCounterMode::Latest);
}
