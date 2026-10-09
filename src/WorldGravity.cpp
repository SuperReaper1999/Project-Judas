#include "RuntimeWorld.h"
#include "UniformGravity.h"

const GravityField* RuntimeWorld::SelectedGravitySource(EntityId owner)const {
    auto* definition=RuntimeDefinition(owner);
    if(!definition||!definition->gravitySelection||definition->gravitySelection->mode!=GravitySelection::Mode::Field)return nullptr;
    auto source=definition->gravitySelection->source;
    if(!RuntimeDefinition(source))return nullptr;
    for(size_t i=0;i<m_gravityRegions.size();++i)
        if(m_gravityRegions[i].id==source)return m_gravityFields[i].get();
    return nullptr;
}
bool RuntimeWorld::GravitySelectionAvailable(EntityId owner)const {
    auto* d=RuntimeDefinition(owner);
    return d&&(!d->gravitySelection||d->gravitySelection->mode==GravitySelection::Mode::Uniform||SelectedGravitySource(owner));
}
glm::vec3 RuntimeWorld::SampleEntityGravity(EntityId owner,glm::vec3 position)const {
    auto* d=RuntimeDefinition(owner);
    if(d&&d->gravitySelection){
        const auto& s=*d->gravitySelection;
        if(s.mode==GravitySelection::Mode::Uniform)return UniformGravity(s.acceleration).Sample(position);
        if(auto* field=SelectedGravitySource(owner))return field->Sample(position);
    }
    // A removed source can never alias a newly allocated body/field. Spatial
    // fallback is explicit in readback, without resetting support or velocity.
    return Gravity().Sample(position);
}
bool RuntimeWorld::SetGravitySelection(EntityId owner,std::optional<GravitySelection> selection,std::string& error){
    auto* d=RuntimeDefinition(owner);
    if(!d){error="stale gravity selection owner";return false;}
    if(selection&&!ValidGravitySelection(*selection,error))return false;
    if(selection&&selection->mode==GravitySelection::Mode::Field){
        auto* source=RuntimeDefinition(selection->source);
        if(!source||!source->gravity){error="selection source needs a live authored gravity field";return false;}
        bool registered=false;for(auto& region:m_gravityRegions)registered|=region.id==selection->source;
        if(!registered){error="gravity source is not published";return false;}
    }
    const auto& old=d->gravitySelection;
    if(old.has_value()==selection.has_value()&&(!old||
       (old->mode==selection->mode&&old->source==selection->source&&old->acceleration==selection->acceleration))){
        error.clear();return true;
    }
    m_scriptDefinitions.at(owner).gravitySelection=selection;
    if(auto* e=FindEntity(owner)){
        e->definition.gravitySelection=selection;
        e->coarseMotion=CoarseMotion::Inertial;
    }
    m_physics.Wake(RuntimeBody(owner));
    // No structural change: streaming checks current intent on its cached IDs.
    error.clear();return true;
}
