#include "LightLists.h"
#include "../RenderLightGrid.h"
#include "../render/ComputeDevice.h"
#include <algorithm>
namespace JudasLighting {
namespace {
constexpr const char* TileKernel=R"GLSL(#version 450 core
layout(local_size_x=64) in;
layout(std430,binding=6) writeonly buffer Output { vec4 result[]; };
layout(std430,binding=7) readonly buffer Input { vec4 prepared[]; };
void main(){
    uint index=gl_GlobalInvocationID.x;
    uint tiles=uint(prepared[0].x)*uint(prepared[0].y),lights=uint(prepared[0].w);
    uint prefix=1u+tiles+4u*lights;
    for(uint i=index;i<prefix;i+=gl_NumWorkGroups.x*64u)result[i]=prepared[i];
    if(index>=tiles)return;
    uint x=index%uint(prepared[0].x),y=index/uint(prepared[0].x);
    uint offset=uint(prepared[1u+index].x),count=uint(prepared[1u+index].y),written=0u;
    // Relevance order is stable and identical to the CPU builder. No atomics
    // or worker completion order determine the capped lighting accumulation.
    for(uint light=0u;light<lights&&written<count;++light){
        uvec4 bounds=uvec4(prepared[prefix+light]);
        if(x>=bounds.x&&x<=bounds.z&&y>=bounds.y&&y<=bounds.w)result[offset+written++]=vec4(float(light),0,0,0);
    }
}
)GLSL";
}
struct LightLists::Storage {
    JudasRender::ComputeDevice device;
    JudasRender::ComputeBuffer output[3],input[3];
    std::size_t outputCapacity[3]{},inputCapacity[3]{},used=0;
    JudasRender::ComputeKernel kernel;
    unsigned next=0,current=0;
    bool initialized=false,ready=false;
};
LightLists::LightLists():state(std::make_unique<Storage>()){}
LightLists::~LightLists()=default;
bool LightLists::Upload(const RenderLightGrid& grid,bool gpuBins,const GraphicsCapabilities& caps,std::string& error){
    using namespace JudasRender;auto& s=*state;s.ready=false;
    if(!caps.modernTier||!caps.storageBuffers||!caps.compute)return false;
    if(!s.initialized){if(!s.device.Initialize(caps,{96*1024*1024,8,1},error))return false;s.initialized=true;}
    const unsigned slot=s.next++%3;s.current=slot;
    auto reserve=[&](ComputeBuffer& buffer,std::size_t& capacity,std::size_t bytes){
        if(bytes<=capacity&&buffer.IsValid())return true;
        if(buffer.IsValid()&&!s.device.DestroyBuffer(buffer,error))return false;
        capacity=(bytes+65535)&~size_t(65535);
        buffer=s.device.CreateBuffer(capacity,ComputeBufferUse::Storage,nullptr,error);return buffer.IsValid();
    };
    const auto tiles=grid.columns*grid.rows;
    const size_t prefix=1+tiles+4*grid.visible;
    s.used=gpuBins?prefix+size_t(tiles)*RenderLightGrid::MaxPerTile:grid.texels.size();
    if(!reserve(s.output[slot],s.outputCapacity[slot],s.used*sizeof(glm::vec4)))return false;
    if(gpuBins){
        if(!s.kernel.IsValid()){s.kernel=s.device.CreateKernel(TileKernel,"lighting-tiles",error);if(!s.kernel.IsValid())return false;}
        if(!reserve(s.input[slot],s.inputCapacity[slot],grid.texels.size()*sizeof(glm::vec4))||
           !s.device.UpdateBuffer(s.input[slot],0,grid.texels.data(),grid.texels.size()*sizeof(glm::vec4),error)||
           !s.device.BindStorage(s.input[slot],7,ComputeAccess::ReadOnly,error)||
           !s.device.BindStorage(s.output[slot],6,ComputeAccess::WriteOnly,error)||
           !s.device.Dispatch(s.kernel,{unsigned((prefix+63)/64),1,1},error)||
           !s.device.Barrier(ComputeBarrier::Storage|ComputeBarrier::BufferUpdate,error))return false;
    }else if(!s.device.UpdateBuffer(s.output[slot],0,grid.texels.data(),s.used*sizeof(glm::vec4),error))return false;
    s.ready=true;return Bind(error);
}
unsigned LightLists::GraphicsBuffer(){auto& s=*state;return s.ready?s.device.NativeName(s.output[s.current]):0;}
bool LightLists::Bind(std::string& error){auto& s=*state;return s.ready&&s.device.BindGraphicsStorage(s.output[s.current],6,error);}
bool LightLists::ReadForDiagnostics(std::vector<glm::vec4>& out,std::string& error){auto& s=*state;if(!s.ready)return false;out.resize(s.used);return s.device.ReadForDiagnostics(s.output[s.current],0,out.data(),out.size()*sizeof(glm::vec4),error);}
bool LightLists::Shutdown(std::string& error){state->ready=false;const bool ok=!state->initialized||state->device.Shutdown(error);if(ok)state=std::make_unique<Storage>();return ok;}
std::size_t LightLists::AllocatedBytes()const{return state->device.AllocatedBytes();}
}
