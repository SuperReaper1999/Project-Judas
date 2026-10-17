#include "TextLayout.h"
#include "SceneFingerprint.h"
#include "PerformanceProfiler.h"
#include <ft2build.h>
#include FT_FREETYPE_H
#include FT_FONT_FORMATS_H
#include <hb.h>
#include <hb-ot.h>
#include <unicode/ubidi.h>
#include <unicode/ubrk.h>
#include <unicode/ustring.h>
#include <unicode/uscript.h>
#include <unicode/uchar.h>
#include <algorithm>
#include <cmath>
#include <fstream>
#include <map>
#include <sstream>
#include <stdexcept>
namespace {
std::vector<UChar> Decode(const std::string& s,bool strict,bool& replaced){
 UErrorCode e=U_ZERO_ERROR;int32_t n=0,sub=0;
 u_strFromUTF8WithSub(nullptr,0,&n,s.data(),int32_t(s.size()),strict?U_SENTINEL:0xfffd,&sub,&e);
 if(e!=U_BUFFER_OVERFLOW_ERROR&&U_FAILURE(e))throw std::runtime_error("malformed UTF-8");
 e=U_ZERO_ERROR;std::vector<UChar> out(n+1);u_strFromUTF8WithSub(out.data(),n+1,&n,s.data(),int32_t(s.size()),strict?U_SENTINEL:0xfffd,&sub,&e);
 if(U_FAILURE(e))throw std::runtime_error("malformed UTF-8");
 out.resize(n);replaced=sub>0;return out;
}
std::vector<int> Boundaries(const std::vector<UChar>& s,UBreakIteratorType type,const char* locale){
 UErrorCode e=U_ZERO_ERROR;auto* b=ubrk_open(type,locale,s.data(),int32_t(s.size()),&e);if(U_FAILURE(e))throw std::runtime_error(u_errorName(e));
 std::vector<int> v;for(int p=ubrk_first(b);p!=UBRK_DONE;p=ubrk_next(b))v.push_back(p);ubrk_close(b);return v;
}
}
bool ValidTextUTF8(const std::string& s,std::string& error){try{bool replaced=false;Decode(s,true,replaced);error.clear();return true;}catch(const std::exception& e){error=e.what();return false;}}
bool DecodeTextFont(std::vector<uint8_t> bytes,const std::string& label,std::shared_ptr<const TextFont>& out,std::string& error){
 if(bytes.empty()||bytes.size()>32*1024*1024){error="font empty or over 32MiB: "+label;return false;}
 FT_Library lib=nullptr;FT_Face face=nullptr;if(FT_Init_FreeType(&lib)){error="FreeType initialization failed";return false;}
 auto status=FT_New_Memory_Face(lib,bytes.data(),FT_Long(bytes.size()),0,&face);
 bool ok=!status&&FT_IS_SCALABLE(face)&&FT_Select_Charmap(face,FT_ENCODING_UNICODE)==0;
 if(face)FT_Done_Face(face);
 FT_Done_FreeType(lib);
 if(!ok){error="requires scalable Unicode outline TTF/OTF font: "+label;return false;}
 auto f=std::make_shared<TextFont>();f->revision=SceneFingerprintSha256(std::string_view(reinterpret_cast<const char*>(bytes.data()),bytes.size()));f->label=label;f->bytes=std::move(bytes);out=f;error.clear();return true;
}
bool LoadTextFont(const std::string& path,std::shared_ptr<const TextFont>& out,std::string& error){std::ifstream f(path,std::ios::binary);if(!f){error="cannot read font "+path;return false;}return DecodeTextFont(std::vector<uint8_t>(std::istreambuf_iterator<char>(f),{}),path,out,error);}
struct TextEngine::Impl {
 FT_Library lib=nullptr;
 struct Face {std::shared_ptr<const TextFont> data;FT_Face ft=nullptr;hb_font_t* hb=nullptr;~Face(){if(hb)hb_font_destroy(hb);if(ft)FT_Done_Face(ft);}};
 std::map<std::string,std::unique_ptr<Face>> faces;std::map<std::string,uint64_t> faceUse;
 struct Cached {std::shared_ptr<const TextLayout> layout;uint64_t use=0;};std::map<std::string,Cached> layouts;uint64_t clock=0;TextCacheStats stats;
 Impl(){if(FT_Init_FreeType(&lib))throw std::runtime_error("FreeType unavailable");}
 ~Impl(){faces.clear();FT_Done_FreeType(lib);}
 Face& Get(const std::shared_ptr<const TextFont>& data,float pixels){
  if(!faces.count(data->revision)&&faces.size()>=64){auto least=std::min_element(faceUse.begin(),faceUse.end(),[](const auto& a,const auto& b){return a.second<b.second;});faces.erase(least->first);faceUse.erase(least);}
  faceUse[data->revision]=++clock;auto& p=faces[data->revision];if(!p){p=std::make_unique<Face>();p->data=data;if(FT_New_Memory_Face(lib,data->bytes.data(),FT_Long(data->bytes.size()),0,&p->ft))throw std::runtime_error("font face invalidated");FT_Select_Charmap(p->ft,FT_ENCODING_UNICODE);auto* blob=hb_blob_create(reinterpret_cast<const char*>(data->bytes.data()),unsigned(data->bytes.size()),HB_MEMORY_MODE_READONLY,nullptr,nullptr);auto* face=hb_face_create(blob,0);p->hb=hb_font_create(face);hb_ot_font_set_funcs(p->hb);hb_face_destroy(face);hb_blob_destroy(blob);}
  hb_font_set_scale(p->hb,int(pixels*64),int(pixels*64));hb_font_set_ppem(p->hb,unsigned(std::ceil(pixels)),unsigned(std::ceil(pixels)));return *p;
 }
 bool Covers(Face& f,const std::vector<UChar>& text,int a,int b){for(int p=a;p<b;){UChar32 c;U16_NEXT(text.data(),p,b,c);if(u_hasBinaryProperty(c,UCHAR_DEFAULT_IGNORABLE_CODE_POINT)||u_iscntrl(c))continue;if(!FT_Get_Char_Index(f.ft,c))return false;}return true;}
 struct Segment {int a,b,script;size_t font;};
 TextLine ShapeLine(const std::vector<UChar>& text,UBiDi* para,int start,int end,const std::vector<std::shared_ptr<const TextFont>>& fonts,const TextOptions& o,std::vector<ShapedGlyph>& glyphs,unsigned& missing){
  UErrorCode e=U_ZERO_ERROR;auto* bidi=ubidi_open();if(start<end)ubidi_setLine(para,start,end,bidi,&e);if(U_FAILURE(e)){ubidi_close(bidi);throw std::runtime_error(u_errorName(e));}
  TextLine line;line.start=start;line.end=end;line.rtl=start<end?(ubidi_getParaLevel(bidi)&1):o.direction==TextDirection::RTL;float pen=0;int runCount=start<end?ubidi_countRuns(bidi,&e):0;
  for(int r=0;r<runCount;++r){int logical=0,length=0;auto dir=ubidi_getVisualRun(bidi,r,&logical,&length);int a=start+logical,b=a+length;
   // Resolve Common/Inherited using neighboring strong script. Font choice is
   // whole script run first, then whole grapheme clusters with surrounding HB context.
   std::vector<Segment> scripts;int current=USCRIPT_COMMON,segmentStart=a;
   for(int p=a;p<b;){int cpStart=p;UChar32 c;U16_NEXT(text.data(),p,b,c);UErrorCode se=U_ZERO_ERROR;int sc=uscript_getScript(c,&se);if(sc==USCRIPT_COMMON||sc==USCRIPT_INHERITED)continue;if(current==USCRIPT_COMMON)current=sc;else if(current!=sc){scripts.push_back({segmentStart,cpStart,current,0});segmentStart=cpStart;current=sc;}}
   scripts.push_back({segmentStart,b,current,0});std::vector<Segment> segments;
   auto boundaries=Boundaries(text,UBRK_CHARACTER,o.locale.c_str());
   for(auto script:scripts){size_t all=fonts.size();for(size_t f=0;f<fonts.size();++f)if(Covers(Get(fonts[f],o.pixels),text,script.a,script.b)){all=f;break;}
    if(all<fonts.size()){script.font=all;segments.push_back(script);continue;}
    for(int c=script.a;c<script.b;){auto next=std::upper_bound(boundaries.begin(),boundaries.end(),c);int last=next==boundaries.end()?script.b:std::min(*next,script.b);size_t chosen=fonts.size();for(size_t f=0;f<fonts.size();++f)if(Covers(Get(fonts[f],o.pixels),text,c,last)){chosen=f;break;}if(chosen==fonts.size()){chosen=0;++missing;}
     if(!segments.empty()&&segments.back().b==c&&segments.back().font==chosen&&segments.back().script==script.script)segments.back().b=last;else segments.push_back({c,last,script.script,chosen});c=last;}
   }
   if(dir==UBIDI_RTL)std::reverse(segments.begin(),segments.end());
   for(auto s:segments){auto& f=Get(fonts[s.font],o.pixels);float units=float(f.ft->units_per_EM);line.ascent=std::max(line.ascent,f.ft->ascender/units*o.pixels);line.descent=std::max(line.descent,-f.ft->descender/units*o.pixels);
    auto* buffer=hb_buffer_create();hb_buffer_set_direction(buffer,dir==UBIDI_RTL?HB_DIRECTION_RTL:HB_DIRECTION_LTR);hb_buffer_set_script(buffer,hb_script_from_iso15924_tag(uscript_getShortName(UScriptCode(s.script))?hb_tag_from_string(uscript_getShortName(UScriptCode(s.script)),-1):HB_TAG_NONE));hb_buffer_set_language(buffer,hb_language_from_string(o.locale.c_str(),-1));hb_buffer_set_cluster_level(buffer,HB_BUFFER_CLUSTER_LEVEL_MONOTONE_GRAPHEMES);hb_buffer_set_flags(buffer,hb_buffer_flags_t((s.a==start?HB_BUFFER_FLAG_BOT:0)|(s.b==end?HB_BUFFER_FLAG_EOT:0)));hb_buffer_add_utf16(buffer,reinterpret_cast<const uint16_t*>(text.data()),int(text.size()),s.a,s.b-s.a);hb_shape(f.hb,buffer,nullptr,0);++stats.shapes;
    unsigned n=0;auto* info=hb_buffer_get_glyph_infos(buffer,&n);auto* positions=hb_buffer_get_glyph_positions(buffer,&n);
    for(unsigned g=0;g<n;++g){float dx=positions[g].x_offset/64.f,dy=positions[g].y_offset/64.f;glyphs.push_back({fonts[s.font],info[g].codepoint,info[g].cluster,pen+dx,-dy,positions[g].x_advance/64.f,dir==UBIDI_RTL});hb_glyph_extents_t ink;if(hb_font_get_glyph_extents(f.hb,info[g].codepoint,&ink)){line.ascent=std::max(line.ascent,(ink.y_bearing/64.f)+dy);line.descent=std::max(line.descent,-(ink.y_bearing+ink.height)/64.f-dy);}pen+=positions[g].x_advance/64.f;}
    hb_buffer_destroy(buffer);
   }
  }
  if(line.ascent+line.descent<=0){auto& f=Get(fonts[0],o.pixels);line.ascent=f.ft->ascender/float(f.ft->units_per_EM)*o.pixels;line.descent=-f.ft->descender/float(f.ft->units_per_EM)*o.pixels;}
  line.width=pen;ubidi_close(bidi);return line;
 }
};
TextEngine::TextEngine():m(std::make_unique<Impl>()){}TextEngine::~TextEngine()=default;
void TextEngine::Clear(){m->layouts.clear();m->faces.clear();m->faceUse.clear();m->stats.layoutBytes=0;}
TextCacheStats TextEngine::Stats()const{auto s=m->stats;s.fontBytes=0;for(auto& p:m->faces)s.fontBytes+=p.second->data->bytes.size();return s;}
std::shared_ptr<const TextLayout> TextEngine::Layout(const std::string& source,const std::vector<std::shared_ptr<const TextFont>>& fonts,const TextOptions& options){
 JUDAS_PROFILE_SCOPE("Text layout");TextOptions o=options;
 if(fonts.empty())return std::make_shared<TextLayout>();
 if(source.size()>65536||!std::isfinite(o.pixels)||o.pixels<=0||o.pixels>512||!std::isfinite(o.width)||o.width<0)throw std::runtime_error("text layout exceeds bounds");
 // Exact option bytes participate; no locale-dependent numeric string formatting.
 std::string key=source+'\0'+o.locale;key.append(reinterpret_cast<const char*>(&o.pixels),sizeof(float));key.append(reinterpret_cast<const char*>(&o.width),sizeof(float));key+=char(o.wrap);key+=char(o.direction);key+=char(o.alignment);for(auto& f:fonts)key+=f->revision;
 auto it=m->layouts.find(key);if(it!=m->layouts.end()){it->second.use=++m->clock;++m->stats.hits;return it->second.layout;}
 auto out=std::make_shared<TextLayout>();out->pixels=o.pixels;auto text=Decode(source,false,out->replacedInput);float y=0;int paraStart=0;
 do{int paraEnd=paraStart;while(paraEnd<int(text.size())&&text[paraEnd]!=10&&text[paraEnd]!=13&&text[paraEnd]!=0x2028&&text[paraEnd]!=0x2029)++paraEnd;
  UErrorCode e=U_ZERO_ERROR;auto* bidi=ubidi_open();ubidi_setPara(bidi,text.data()+paraStart,paraEnd-paraStart,o.direction==TextDirection::LTR?0:o.direction==TextDirection::RTL?1:UBIDI_DEFAULT_LTR,nullptr,&e);if(U_FAILURE(e)){ubidi_close(bidi);throw std::runtime_error(u_errorName(e));}
  std::vector<UChar> para(text.begin()+paraStart,text.begin()+paraEnd);auto breaks=Boundaries(para,UBRK_LINE,o.locale.c_str());auto graphemes=Boundaries(para,UBRK_CHARACTER,o.locale.c_str());int start=0;
  do{int end=int(para.size());if(o.wrap&&o.width>0&&start<end){int accepted=start;for(int b:breaks)if(b>start){std::vector<ShapedGlyph> probe;unsigned ignored=0;auto l=m->ShapeLine(para,bidi,start,b,fonts,o,probe,ignored);if(l.width<=o.width||accepted==start)accepted=b;else break;if(l.width>o.width)break;}end=accepted;
    // Emergency wrap only at grapheme AND full-shape cluster boundaries.
    std::vector<ShapedGlyph> full;unsigned ignored=0;auto l=m->ShapeLine(para,bidi,start,end,fonts,o,full,ignored);if(l.width>o.width){int acceptedCluster=start;for(int b:graphemes)if(b>start&&b<end&&std::any_of(full.begin(),full.end(),[&](const auto& g){return g.cluster==uint32_t(b);})){std::vector<ShapedGlyph> probe;auto candidate=m->ShapeLine(para,bidi,start,b,fonts,o,probe,ignored);if(candidate.width<=o.width)acceptedCluster=b;else break;}if(acceptedCluster>start)end=acceptedCluster;}
   }
   std::vector<ShapedGlyph> glyphs;auto line=m->ShapeLine(para,bidi,start,end,fonts,o,glyphs,out->missingClusters);line.baseline=y+line.ascent;float x=0;auto align=o.alignment;if(align==TextAlignment::Start)align=line.rtl?TextAlignment::Right:TextAlignment::Left;if(align==TextAlignment::End)align=line.rtl?TextAlignment::Left:TextAlignment::Right;if(align==TextAlignment::Right)x=std::max(0.f,o.width-line.width);if(align==TextAlignment::Centre)x=std::max(0.f,o.width-line.width)*.5f;
   for(auto& g:glyphs){g.x+=x;g.y+=line.baseline;g.cluster+=paraStart;out->glyphs.push_back(g);}line.start+=paraStart;line.end+=paraStart;out->lines.push_back(line);out->width=std::max(out->width,line.width);y+=line.ascent+line.descent+o.pixels*.15f;start=end;
  }while(start<int(para.size()));ubidi_close(bidi);
  if(paraEnd==int(text.size()))break;
  paraStart=paraEnd+1;if(text[paraEnd]==13&&paraStart<int(text.size())&&text[paraStart]==10)++paraStart;
 }while(paraStart<=int(text.size()));out->height=y;++m->stats.layouts;
 if(m->layouts.size()>=256){auto least=std::min_element(m->layouts.begin(),m->layouts.end(),[](const auto& a,const auto& b){return a.second.use<b.second.use;});m->stats.layoutBytes-=least->first.size()+least->second.layout->glyphs.size()*sizeof(ShapedGlyph);m->layouts.erase(least);}
 m->stats.layoutBytes+=key.size()+out->glyphs.size()*sizeof(ShapedGlyph);m->layouts.emplace(std::move(key),Impl::Cached{out,++m->clock});return out;
}
bool TextEngine::Raster(const ShapedGlyph& glyph,float pixels,TextRaster& out,std::string& error){
 JUDAS_PROFILE_SCOPE("Text glyph raster");try{auto& f=m->Get(glyph.font,pixels);FT_Set_Pixel_Sizes(f.ft,0,unsigned(std::ceil(pixels)));if(FT_Load_Glyph(f.ft,glyph.glyph,FT_LOAD_DEFAULT|FT_LOAD_NO_BITMAP)||FT_Render_Glyph(f.ft->glyph,FT_RENDER_MODE_NORMAL)){error="outline glyph raster failed";return false;}
 auto& b=f.ft->glyph->bitmap;if(b.pixel_mode!=FT_PIXEL_MODE_GRAY&&b.width&&b.rows){error="only grayscale outline glyphs supported";return false;}out.width=int(b.width);out.height=int(b.rows);out.left=f.ft->glyph->bitmap_left;out.top=f.ft->glyph->bitmap_top;out.coverage.resize(size_t(out.width)*out.height);for(int y=0;y<out.height;++y){int row=b.pitch>=0?y:out.height-1-y;std::copy_n(b.buffer+row*std::abs(b.pitch),out.width,out.coverage.begin()+y*out.width);}++m->stats.rasters;return true;
 }catch(const std::exception& e){error=e.what();return false;}}
