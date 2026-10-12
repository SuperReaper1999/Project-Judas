from pathlib import Path
import json,hashlib
root=Path('/home/conner/Documents/GitHub/Project-Judas')
out=root/'.cache/m78-continuation/foot-reference-overlay'
out.mkdir(parents=True,exist_ok=True)
original=(root/'src/PhysicalBalance.cpp').read_text()
needle='''    const auto& swing=m_settings.effectors[swingIndex];
    float t='''
addition='''    const auto& swing=m_settings.effectors[swingIndex];
    // Isolated diagnosis: use the same physical foot orientation selected by
    // the existing ankle-drive policy. An upright offset is not the anchor of
    // a tilted planted body when authored plantedOrientationWeight is zero.
    glm::vec3 balancePoint(0),balanceSupportVelocity(0);unsigned balanceCount=0;
    for(const auto& s:m_observation.supports)if(s.contacts){balancePoint+=s.point;balanceSupportVelocity+=s.velocity;++balanceCount;}
    if(balanceCount){balancePoint/=float(balanceCount);balanceSupportVelocity/=float(balanceCount);}
    if(m_history.swing>=0&&m_observation.supports[1-m_history.swing].contacts){const auto& s=m_observation.supports[1-m_history.swing];balancePoint=s.point;balanceSupportVelocity=s.velocity;}
    auto balanceError=balanceCount?Tangent(m_observation.com-balancePoint,m_observation.up):glm::vec3(0);
    auto balanceVelocity=Tangent(m_observation.velocity-balanceSupportVelocity,m_observation.up);
    auto balanceTilt=glm::cross(m_observation.up,-(balanceError*m_settings.positionResponse+balanceVelocity*m_settings.velocityResponse));
    const float balanceLean=glm::length(balanceTilt);
    const auto balanceCorrection=balanceLean>1e-5f?glm::angleAxis(std::min(balanceLean,m_settings.maximumLean),balanceTilt/balanceLean):glm::quat(1,0,0,0);
    std::array<glm::quat,2> footOrientations;
    for(unsigned f=0;f<2;++f){const auto body=m_bodies[m_settings.effectors[f].foot];
        const auto authored=balanceCorrection*upright*body.restRotation;
        const bool preserve=(m_history.swing!=int(f)||m_history.swingTime<0)&&m_observation.supports[f].contacts;
        footOrientations[f]=preserve?glm::slerp(world.GetTransform(body.body).rotation,authored,m_settings.plantedOrientationWeight):upright;
    }
    float t='''
assert original.count(needle)==1
s=original.replace(needle,addition)
needle='auto end=footTargets[f]+upright*ankle.settings.anchorA;'
assert s.count(needle)==1;s=s.replace(needle,'auto end=footTargets[f]+footOrientations[f]*ankle.settings.anchorA;')
needle='auto start=World(rootReference,hip.settings.anchorB),end=footTargets[f]+upright*ankle.settings.anchorA;'
assert s.count(needle)==1;s=s.replace(needle,'auto start=World(rootReference,hip.settings.anchorB),end=footTargets[f]+footOrientations[f]*ankle.settings.anchorA;')
(out/'PhysicalBalance.cpp').write_text(s)
sha=lambda x:hashlib.sha256(x).hexdigest()
(out/'OVERLAY.json').write_text(json.dumps({'kind':'Isolated diagnostic; not production change','source':str(root/'src/PhysicalBalance.cpp'),'source_sha256':sha(original.encode()),'overlay_sha256':sha(s.encode()),'mechanism':'Planted-foot reach and IK endpoints use the same resolved world orientation as the existing ankle drive; airborne/unplanted endpoints, fixture, gains, thresholds and defaults unchanged','scope':'PhysicalBalance.cpp only; duplicated correction expression for isolation, not proposed final factoring','unchanged':['airborne/unplanted endpoint orientation','ankle-drive target selection','common balance correction','target interpolation','touchdown gate','motor budgets','rig inputs']},indent=2)+'\n')
print(out)
