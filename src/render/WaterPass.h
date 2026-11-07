#pragma once
#include "../GraphicsApi.h"
#include <glm/glm.hpp>
#include <map>
#include <cstdint>
#include <string>
#include "../fluid/GameWaterSurface.h"
namespace JudasRender {
// Renderer-owned resources. Physical quantities and gameplay queries never enter
// this pass; its interval field exists only for colour, refraction and optics.
class WaterPass {
public:
    struct View {
        glm::mat4 view{1},projection{1};
        glm::vec3 sun{.3f,.8f,.2f},sunColour{1},colour{.025f,.26f,.32f};
        glm::vec3 background{.48f,.66f,.76f},ambient{.35f,.4f,.43f};
        glm::mat3 environmentInverse{1};
        GLuint environment=0;
        float environmentIntensity=1;
        float seconds=0,opacity=.84f;
        bool linear=false;
    };
    bool Draw(std::uint64_t key,std::uint64_t revision,GLuint vao,GLsizei vertices,
              const JudasWater::SurfaceField&,const View&,std::string& error);
    bool Composite(std::string& error);
    bool HasPending() const {return !pending_.empty();}
    void Forget(std::uint64_t key);
    void Shutdown();
    std::size_t Bytes() const;
    std::uint64_t Draws() const {return draws_;}
    std::uint64_t UnderwaterDraws() const {return underwaterDraws_;}
private:
    struct Field {GLuint texture=0;std::uint64_t revision=0;glm::ivec3 cells{0};};
    std::map<std::uint64_t,Field> fields_;
    struct Pending {
        std::uint64_t key=0;
        View camera;
        glm::vec3 origin{0};glm::ivec3 cells{0};glm::mat3 basis{1};float spacing=1;
    };
    std::vector<Pending> pending_;
    GLuint surface_=0,underwater_=0,vao_=0,fbo_=0,colour_=0,depth_=0;
    int width_=0,height_=0;bool linear_=false;
    std::uint64_t draws_=0,underwaterDraws_=0;
    bool Prepare(std::string&);
    bool Capture(bool linear,std::string&);
};
} // namespace JudasRender
