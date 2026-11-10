#include "RuntimeWorld.h"
#include <algorithm>
BodyHandle RuntimeWorld::DeformableAttachmentBody(const DeformableAttachment& attachment,std::string& error){
    if(attachment.part.empty()){error.clear();return RuntimeBody(attachment.target);}
    auto* material=RuntimeDeformable(attachment.target,error);
    if(!material)return {};
    if(!material->asset->fracture||!material->asset->fracture->rigid){error="panel mount target is not a rigid structural assembly";return {};}
    const auto& parts=material->asset->fracture->parts;
    auto part=std::find_if(parts.begin(),parts.end(),[&](const auto& p){return p.key==attachment.part;});
    if(part==parts.end()){error="unknown panel mount structural section: "+attachment.part;return {};}
    if(!PrepareRigidFracture(attachment.target,error))return {};
    auto id=FracturePartEntity(attachment.target,unsigned(part-parts.begin()));
    error.clear();return RuntimeBody(id);
}
void RuntimeWorld::UpdatePanelCollisionExclusions(DeformableInstance& panel){
    panel.carrierCollisionExclusions.clear();
    for(unsigned a=0;a<panel.settings.attachments.size();++a){const auto& mount=panel.settings.attachments[a];
        if(!mount.enabled||panel.released[a]||mount.part.empty())continue;
        auto carrier=m_deformables.find(mount.target);if(carrier==m_deformables.end())continue;
        const auto& frame=carrier->second.simulation;const auto& rigid=carrier->second.rigid;
        if(!rigid.initialized||!frame.asset->fracture)continue;
        const auto& parts=frame.asset->fracture->parts;
        auto part=std::find_if(parts.begin(),parts.end(),[&](const auto& p){return p.key==mount.part;});
        if(part==parts.end())continue;
        auto component=frame.fracture.component[part-parts.begin()];
        for(unsigned i=0;i<parts.size();++i)if(!frame.fracture.removed[i]&&frame.fracture.component[i]==component){
            auto body=RuntimeBody(rigid.parts[i]);
            if(body.IsValid()&&!std::any_of(panel.carrierCollisionExclusions.begin(),panel.carrierCollisionExclusions.end(),[&](auto other){return other.id==body.id&&other.world==body.world;}))panel.carrierCollisionExclusions.push_back(body);
        }
    }
}
