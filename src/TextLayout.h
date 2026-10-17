#pragma once
#include <cstdint>
#include <memory>
#include <string>
#include <vector>
// Immutable font bytes may cross resource workers. Native faces never do.
struct TextFont {std::vector<uint8_t> bytes;std::string revision,label;};
bool DecodeTextFont(std::vector<uint8_t> bytes,const std::string& label,std::shared_ptr<const TextFont>& out,std::string& error);
bool LoadTextFont(const std::string& path,std::shared_ptr<const TextFont>& out,std::string& error);
bool ValidTextUTF8(const std::string& text,std::string& error);
enum class TextDirection {Auto,LTR,RTL};
enum class TextAlignment {Left,Right,Centre,Start,End};
struct TextOptions {float pixels=24,width=0;bool wrap=false;TextDirection direction=TextDirection::Auto;TextAlignment alignment=TextAlignment::Left;std::string locale="en";};
struct ShapedGlyph {std::shared_ptr<const TextFont> font;uint32_t glyph=0,cluster=0;float x=0,y=0,advance=0;bool rtl=false;};
struct TextLine {uint32_t start=0,end=0;float width=0,baseline=0,ascent=0,descent=0;bool rtl=false;};
struct TextLayout {std::vector<ShapedGlyph> glyphs;std::vector<TextLine> lines;float width=0,height=0,pixels=24;bool replacedInput=false;unsigned missingClusters=0;};
struct TextRaster {int width=0,height=0,left=0,top=0;std::vector<uint8_t> coverage;};
struct TextCacheStats {uint64_t layouts=0,hits=0,shapes=0,rasters=0;size_t layoutBytes=0,fontBytes=0;};
// Owner-thread object. Immutable layouts contain glyph identities, never atlas UVs.
class TextEngine {
public:
 TextEngine();~TextEngine();TextEngine(const TextEngine&)=delete;TextEngine& operator=(const TextEngine&)=delete;
 std::shared_ptr<const TextLayout> Layout(const std::string&,const std::vector<std::shared_ptr<const TextFont>>&,const TextOptions&);
 bool Raster(const ShapedGlyph&,float pixels,TextRaster&,std::string& error);
 void Clear();TextCacheStats Stats()const;
private:struct Impl;std::unique_ptr<Impl> m;
};
