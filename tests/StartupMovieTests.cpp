#include "StartupMovie.h"
#include "EnginePaths.h"
#include <filesystem>
#include <fstream>
#include <cstdio>
#include <limits>
#include <iterator>
int main(){
    unsigned checks=0,failures=0;
    const auto check=[&](bool ok,const char* name){++checks;if(!ok){++failures;std::fprintf(stderr,"FAIL %s\n",name);}};
    StartupMovie movie;std::string error;TextureData frame;
    const auto source=ResolveEngineDataPath("assets/branding/judas-startup.jsplash");
    check(movie.Open(source,error),"supplied cooked splash opens");
    check(movie.Count()==240,"eight second clip retains 240 frames");
    check(movie.FrameIndex(8)==0&&movie.FrameIndex(7.999)==239&&movie.FrameIndex(-1)==0&&movie.FrameIndex(std::numeric_limits<double>::max())<240&&movie.FrameIndex(std::numeric_limits<double>::infinity())==0,"bounded looping clock");
    for(unsigned i=0;i<240;++i)check(movie.Frame((i+.1)/30,frame,error)&&frame.width==512&&frame.height==512&&frame.pixels.size()==512*512*4,"every supplied frame decodes");
    movie.Close();check(!movie.Frame(0,frame,error),"closed movie rejects decode");
    const auto root=std::filesystem::temp_directory_path()/"judas-startup-movie-tests";std::filesystem::create_directories(root);
    std::ifstream file(source,std::ios::binary);std::string bytes((std::istreambuf_iterator<char>(file)),{});
    auto reject=[&](std::string data,const char* name){auto path=root/"invalid.jsplash";{std::ofstream out(path,std::ios::binary);out.write(data.data(),data.size());}check(!movie.Open(path.string(),error)&&movie.Count()==0&&!error.empty(),name);};
    reject("JSM1","truncated header");reject(bytes.substr(0,bytes.size()-1),"truncated payload");reject(bytes+"x","trailing data");
    auto altered=bytes;altered[0]='X';reject(altered,"wrong magic");altered=bytes;altered[12]=0;reject(altered,"zero fps");
    altered=bytes;for(unsigned i=20;i<24;++i)altered[i]=char(255);reject(altered,"oversized encoded frame");
    altered=bytes;altered[24]=0;{std::ofstream out(root/"invalid.jsplash",std::ios::binary);out.write(altered.data(),altered.size());}
    check(movie.Open((root/"invalid.jsplash").string(),error)&&!movie.Frame(0,frame,error),"corrupt JPEG fails cleanly");
    movie.Close();std::filesystem::remove_all(root);
    std::printf("Startup movie: %u checks, %u failures\n",checks,failures);return failures?1:0;
}
