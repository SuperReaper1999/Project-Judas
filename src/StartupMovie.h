#pragma once
#include "TextureData.h"
#include <fstream>
#include <string>
#include <vector>

// Engine-owned splash only, not a second project video/asset system. The cooked
// MP4 uses independently decoded JPEG frames so memory does not scale with time.
class StartupMovie {
public:
    bool Open(const std::string& path,std::string& error);
    bool Frame(double seconds,TextureData& image,std::string& error);
    unsigned FrameIndex(double seconds)const;
    unsigned Count()const{return unsigned(m_frames.size());}
    void Close();
private:
    struct FrameRecord{std::streamoff offset;unsigned bytes;};
    std::ifstream m_file;
    std::vector<FrameRecord> m_frames;
    unsigned m_width=0,m_height=0,m_fps=0;
};
