#include "Deformable.h"
#include <algorithm>
#include <stdexcept>
// The fastener reaction limits and the sheet's local plastic work are distinct
// physical failure modes. Publication does not add an impulse or reset velocity.
double DeformableInstance::AttachmentWork(unsigned index)const{
    if(index>=settings.attachments.size())throw std::out_of_range("attachment index");
    if(!panelBending)return 0;
    const auto found=asset->groups.find(settings.attachments[index].group);
    if(found==asset->groups.end())return 0;
    double work=0;
    for(const auto& hinge:panelBending->hinges){
        unsigned owners=0;for(unsigned k=0;k<4;++k)owners+=std::find(found->second.begin(),found->second.end(),hinge.nodes[k])!=found->second.end();
        work+=hinge.work*(double(owners)/4);
    }
    return work;
}
bool DeformableInstance::CommitPanelFailures(){
    if(!panelBending)return false;
    bool changed=false;
    for(unsigned i=0;i<settings.attachments.size();++i){const auto& a=settings.attachments[i];
        if(a.enabled&&!released[i]&&a.tearWork>0&&AttachmentWork(i)>=a.tearWork){released[i]=1;changed=true;}
    }
    if(changed)Wake();
    return changed;
}
