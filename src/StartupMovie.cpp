#include "StartupMovie.h"
#include "TextureLoader.h"
#include <cmath>
#include <cstdint>
#include <cstring>

namespace {
unsigned ReadU32(std::istream& in){
    unsigned char b[4]{};in.read(reinterpret_cast<char*>(b),4);
    return unsigned(b[0])|(unsigned(b[1])<<8)|(unsigned(b[2])<<16)|(unsigned(b[3])<<24);
}
}
void StartupMovie::Close(){m_file.close();m_file.clear();m_frames.clear();m_width=m_height=m_fps=0;}
bool StartupMovie::Open(const std::string& path,std::string& error){
    Close();error.clear();m_file.open(path,std::ios::binary|std::ios::ate);
    const auto bytes=m_file.tellg();
    const auto fail=[&](const char* why){error=std::string("Startup movie: ")+why;Close();return false;};
    if(!m_file||bytes<20||bytes>32*1024*1024)return fail("missing or outside 32 MiB input bound");
    m_file.seekg(0);char magic[4]{};m_file.read(magic,4);
    m_width=ReadU32(m_file);m_height=ReadU32(m_file);m_fps=ReadU32(m_file);const auto count=ReadU32(m_file);
    if(std::memcmp(magic,"JSM1",4)||!m_width||!m_height||m_width>1024||m_height>1024||
       !m_fps||m_fps>60||!count||count>600)return fail("invalid header");
    for(unsigned i=0;i<count;++i){
        const auto length=ReadU32(m_file);const auto offset=m_file.tellg();
        if(!m_file||!length||length>2*1024*1024||offset<0||length>bytes-offset)return fail("truncated or oversized frame");
        m_frames.push_back({offset,length});m_file.seekg(length,std::ios::cur);
    }
    if(m_file.tellg()!=bytes)return fail("unexpected trailing data");
    return true;
}
unsigned StartupMovie::FrameIndex(double seconds)const{
    if(m_frames.empty()||!std::isfinite(seconds)||seconds<0)return 0;
    const double duration=double(m_frames.size())/m_fps;
    return unsigned(std::fmod(std::floor(std::fmod(seconds,duration)*m_fps),double(m_frames.size())));
}
bool StartupMovie::Frame(double seconds,TextureData& image,std::string& error){
    if(m_frames.empty()){error="Startup movie is not open";return false;}
    const auto& record=m_frames[FrameIndex(seconds)];std::vector<std::uint8_t> encoded(record.bytes);
    m_file.clear();m_file.seekg(record.offset);m_file.read(reinterpret_cast<char*>(encoded.data()),record.bytes);
    TextureData next;
    if(!m_file||!DecodeTextureFromMemory(encoded.data(),encoded.size(),"startup movie",next,error))return false;
    if(next.width!=int(m_width)||next.height!=int(m_height)){error="Startup movie frame dimensions differ";return false;}
    image=std::move(next);return true;
}
