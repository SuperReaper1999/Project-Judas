#include "VehicleDebug.h"
#include "RuntimeWorld.h"
#include "DebugDraw.h"
#include <algorithm>

namespace JudasVehicles {
void Debug(const RuntimeWorld& world,float alpha,bool shapes,bool contacts,DebugLineList& lines) {
    for(const auto& object:world.ScriptObjects(false))if(object.wheel) {
        const auto* wheel=world.RuntimeWheel(object.id);
        if(!wheel||!wheel->state.available)continue;
        const auto pose=world.PresentedTransform(object.id,object.transform,alpha);
        const auto axle=pose.rotation*glm::vec3(1,0,0);
        const auto color=wheel->state.sleeping?glm::vec3(.25f,.6f,1.f):
            wheel->state.contact?glm::vec3(.2f,1,.5f):glm::vec3(1,.6f,.2f);
        if(shapes) {
            for(float side:{-1.f,1.f})lines.Circle(pose.position+axle*(side*wheel->settings.width*.5f),
                                                  axle,wheel->settings.radius,color,24);
            const auto chassis=world.RuntimeBody(world.WheelChassisEntity(wheel->settings));
            const auto frame=world.Physics().GetTransform(chassis);
            lines.Line(frame.position+frame.rotation*wheel->settings.mount,pose.position,color);
        }
        if(contacts&&wheel->state.contact) {
            lines.Cross(wheel->state.point,.08f,color);
            lines.Arrow(wheel->state.point,wheel->state.point+wheel->state.normal*
                std::min(2.f,wheel->state.normalForce/1000),color);
        }
    }
}
}
