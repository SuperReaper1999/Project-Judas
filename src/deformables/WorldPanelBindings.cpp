#include "RuntimeWorld.h"
#include <algorithm>
BodyHandle RuntimeWorld::DeformableAttachmentBody(const DeformableAttachment& attachment,std::string& error){
    if(attachment.part.empty()){error.clear();return RuntimeBody(attachment.target);}
    auto* material=RuntimeDeformable(attachment.target,error);
    if(!material)return {};
    if(!material->asset->fracture||!material->asset->fracture->rigid){error="panel mount target is not a rigid structural assembly";return {};}
    const auto part=material->asset->fracture->partByKey.find(attachment.part);
    if(part==material->asset->fracture->partByKey.end()){error="unknown panel mount structural section: "+attachment.part;return {};}
    if(!PrepareRigidFracture(attachment.target,error))return {};
    auto id=FracturePartEntity(attachment.target,part->second);
    error.clear();return RuntimeBody(id);
}
void RuntimeWorld::UpdatePanelCollisionExclusions(DeformableInstance& panel){
    panel.carrierCollisionExclusions.clear();
    std::vector<std::pair<const DeformableInstance*,unsigned>> visited;visited.reserve(panel.settings.attachments.size());
    for(unsigned a=0;a<panel.settings.attachments.size();++a){const auto& mount=panel.settings.attachments[a];
        if(!mount.enabled||panel.released[a]||mount.part.empty())continue;
        auto carrier=m_deformables.find(mount.target);if(carrier==m_deformables.end())continue;
        const auto& frame=carrier->second.simulation;const auto& rigid=carrier->second.rigid;
        if(!rigid.initialized||!frame.asset->fracture)continue;
        const auto& parts=frame.asset->fracture->parts;
        const auto part=frame.asset->fracture->partByKey.find(mount.part);
        if(part==frame.asset->fracture->partByKey.end())continue;
        const auto component=frame.fracture.component[part->second];
        const auto key=std::make_pair(&frame,component);
        if(std::find(visited.begin(),visited.end(),key)!=visited.end())continue;
        visited.push_back(key);
        for(unsigned i=0;i<parts.size();++i)if(!frame.fracture.removed[i]&&frame.fracture.component[i]==component){
            auto body=RuntimeBody(rigid.parts[i]);
            if(body.IsValid())panel.carrierCollisionExclusions.push_back(body);
        }
    }
}
