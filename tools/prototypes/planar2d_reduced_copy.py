"""Private Stage 0 reduced-mobility patch for a disposable MK2 source tree.

This is deliberately not production feature wiring. Generic scene components,
2D collision geometry, JS, SDK and editor support belong to subsequent stages.
"""
from pathlib import Path


def replace(text, old, new, count=1):
    if text.count(old)!=count:
        raise RuntimeError(f"Reduced prototype patch site drifted ({count} expected): {old[:80]}")
    return text.replace(old,new)


def patch(source: Path, change):
    def edit(name, old, new):
        change(source / "src" / name, old, new)

    edit("RigidBody.h", "    float inverseMass = 0.0f;", """    // Disposable reduced-motion experiment; absent from production RigidBody.
    float stage0InverseIz = 0;
    glm::vec3 stage0Normal{0,0,1};
    bool Stage0Planar() const { return stage0InverseIz > 0; }
    glm::vec3 LinearResponse(glm::vec3 impulse) const {
        if (!Stage0Planar()) return impulse * inverseMass;
        return (impulse-stage0Normal*glm::dot(stage0Normal,impulse))*inverseMass;
    }
    glm::mat3 ResponseInertiaWorld() const {
        return Stage0Planar() ? glm::outerProduct(stage0Normal,stage0Normal)*stage0InverseIz : InverseInertiaWorld();
    }
    double Stage0Energy() const {
        const double spin=glm::dot(glm::dvec3(stage0Normal),glm::dvec3(angularVelocity));
        return .5*glm::dot(glm::dvec3(linearVelocity),glm::dvec3(linearVelocity))/inverseMass + .5*spin*spin/stage0InverseIz;
    }
    float inverseMass = 0.0f;""")
    edit("RigidBody.h", "linearVelocity += impulse * inverseMass;", "linearVelocity += LinearResponse(impulse);")
    edit("RigidBody.h", "angularVelocity += InverseInertiaWorld() * angularImpulse;", "angularVelocity += ResponseInertiaWorld() * angularImpulse;")
    edit("RigidBody.cpp", "body.forceAccumulator * body.inverseMass", "body.LinearResponse(body.forceAccumulator)")
    edit("RigidBody.cpp", "body.InverseInertiaWorld();", "body.ResponseInertiaWorld();")
    edit("RigidBody.cpp", "const auto generator=AngularDriftGenerator(body.inverseInertiaLocal,body.orientation,body.angularVelocity,fixedDeltaTime);", "const auto generator=body.Stage0Planar()?body.angularVelocity:AngularDriftGenerator(body.inverseInertiaLocal,body.orientation,body.angularVelocity,fixedDeltaTime);")
    edit("RigidBody.cpp", "body.angularVelocity=TransportAngularVelocity(body.inverseInertiaLocal,previous,body.orientation,body.angularVelocity);", "if(!body.Stage0Planar())body.angularVelocity=TransportAngularVelocity(body.inverseInertiaLocal,previous,body.orientation,body.angularVelocity);")

    edit("PhysicsWorld.h", "private:\n", "    bool Stage0ConfigurePlane(BodyHandle,glm::vec3 normal);\nprivate:\n")
    with (source / "src/physics/WorldBodies.cpp").open("a", encoding="utf-8") as f:
        f.write("""
bool PhysicsWorld::Stage0ConfigurePlane(BodyHandle handle,glm::vec3 normal) {
    auto* body=m_impl->Get(handle);
    if(!body||!body->isDynamic||!FiniteVector(normal)||glm::length(normal)<.99f)return false;
    normal=glm::normalize(normal);
    if(glm::length(glm::cross(normal,body->rigidBody.orientation*glm::vec3(0,0,1)))>1e-5f)return false;
    if(std::abs(glm::dot(normal,body->rigidBody.linearVelocity))>1e-5f||
       glm::length(glm::cross(normal,body->rigidBody.angularVelocity))>1e-5f)return false;
    const auto physicalInertia=glm::inverse(body->rigidBody.InverseInertiaWorld());
    const float iz=glm::dot(normal,physicalInertia*normal);
    if(!(iz>0)||!std::isfinite(iz))return false;
    body->rigidBody.stage0Normal=normal;
    body->rigidBody.stage0InverseIz=1/iz;
    return true;
}
""")
    edit("physics/WorldBodies.cpp", "    if (body) {\n        if (velocity !=", "    if (body) {\n        if(body->rigidBody.Stage0Planar()&&std::abs(glm::dot(body->rigidBody.stage0Normal,velocity))>1e-5f)throw std::invalid_argument(\"Stage0 planar velocity outside authored motion space\");\n        if (velocity !=")
    edit("physics/WorldBodies.cpp", "    if (body) {\n        if (angularVelocity !=", "    if (body) {\n        if(body->rigidBody.Stage0Planar()&&glm::length(glm::cross(body->rigidBody.stage0Normal,angularVelocity))>1e-5f)throw std::invalid_argument(\"Stage0 planar angular velocity outside authored motion space\");\n        if (angularVelocity !=")
    edit("physics/WorldJoints.cpp", "    static std::atomic<std::uint64_t> next{1};", """    if(a->rigidBody.Stage0Planar()||(b&&b->rigidBody.Stage0Planar())) {
        if(settings.type!=JointType::Stage0Pin)return {};
        if(b&&(!a->rigidBody.Stage0Planar()||!b->rigidBody.Stage0Planar()||
           glm::dot(a->rigidBody.stage0Normal,b->rigidBody.stage0Normal)<.99999f))return {};
    }
    static std::atomic<std::uint64_t> next{1};""")
    # A prototype-only, explicitly versioned extension to the native body
    # participant. It does not alter published save formats or promise old-file
    # compatibility. Parse and validate all new fields before assigning state.
    edit("physics/WorldPersistence.cpp", "    archive(inverseMass, inverseInertia);", """    archive(inverseMass, inverseInertia);
    uint32_t motionVersion=1;
    auto planeIz=body->rigidBody.stage0InverseIz;
    auto planeNormal=body->rigidBody.stage0Normal;
    archive(motionVersion,planeIz,planeNormal);
    archive.Require(motionVersion==1,\"Stage0 newer body motion-space version\");
    archive.Require(planeIz>=0&&(!planeIz||(body->isDynamic&&
        std::abs(glm::dot(planeNormal,planeNormal)-1)<1e-5f&&
        glm::length(glm::cross(planeNormal,body->rigidBody.orientation*glm::vec3(0,0,1)))<1e-5f)),
        \"Stage0 invalid saved planar mobility\");""")
    edit("physics/WorldPersistence.cpp", "        body->rigidBody.inverseMass = inverseMass;", "        body->rigidBody.stage0InverseIz=planeIz;body->rigidBody.stage0Normal=planeNormal;\n        body->rigidBody.inverseMass = inverseMass;")
    edit("physics/WorldPersistence.cpp", "    field(body.rigidBody.inverseInertiaLocal);", "    field(body.rigidBody.inverseInertiaLocal);\n    field(body.rigidBody.stage0InverseIz);field(body.rigidBody.stage0Normal);")
    edit("physics/WorldBodies.cpp", "    body->rigidBody.linearVelocity += acceleration * fixedDeltaTime;\n    body->stepVelocityDelta += acceleration * fixedDeltaTime;", "    const auto change=body->rigidBody.Stage0Planar()?body->rigidBody.LinearResponse(acceleration)/body->rigidBody.inverseMass*fixedDeltaTime:acceleration*fixedDeltaTime;\n    body->rigidBody.linearVelocity += change;\n    body->stepVelocityDelta += change;")
    edit("physics/WorldBodies.cpp", "    body->lastAcceleration = acceleration;", "    body->lastAcceleration = body->rigidBody.Stage0Planar()?body->rigidBody.LinearResponse(acceleration)/body->rigidBody.inverseMass:acceleration;")

    # Preserve the explicit motion-space descriptor through all motion leaves;
    # CCD occasionally publishes the full evaluated RigidBody, not just a pose.
    edit("RigidMotion.h", "    float inverseMass = 0;", "    float stage0InverseIz=0; glm::vec3 stage0Normal{0,0,1};\n    float inverseMass = 0;")
    edit("RigidMotion.h", "        result.inverseMass=inverseMass;", "        result.stage0InverseIz=stage0InverseIz;result.stage0Normal=stage0Normal;\n        result.inverseMass=inverseMass;")
    edit("RigidMotion.h", "        if(!prescribed&&!result.IsStatic())", "        if(!prescribed&&!result.IsStatic()&&!result.Stage0Planar())")
    edit("RigidMotion.h", "        s.inverseMass=body.inverseMass;", "        s.stage0InverseIz=body.stage0InverseIz;s.stage0Normal=body.stage0Normal;\n        s.inverseMass=body.inverseMass;")
    edit("RigidMotion.h", "s.driftAngularVelocity=prescribed?body.angularVelocity:", "s.driftAngularVelocity=prescribed||body.Stage0Planar()?body.angularVelocity:")
    edit("RigidRotation.cpp", "bool Principal(const RigidBody& body) {", "bool Principal(const RigidBody& body) {\n    if(body.Stage0Planar())return true;")
    edit("RigidRotation.cpp", "    segment.inverseMass = body.inverseMass;", "    segment.stage0InverseIz=body.stage0InverseIz;segment.stage0Normal=body.stage0Normal;\n    segment.inverseMass = body.inverseMass;")

    edit("physics/WorldMk2Solver.cpp", "    return a.invMass + b.invMass +", "    const float translation=(a.rb->Stage0Planar()||b.rb->Stage0Planar())?glm::dot(d,(a.dynamic?a.rb->LinearResponse(d):glm::vec3(0))+(b.dynamic?b.rb->LinearResponse(d):glm::vec3(0))):a.invMass+b.invMass;\n    return translation +")
    for old, new in (
        ("impulse * a.invMass", "a.rb->LinearResponse(impulse)"),
        ("impulse * b.invMass", "b.rb->LinearResponse(impulse)"),
        ("d * (a.invMass * impulse)", "(a.rb->Stage0Planar()?a.rb->LinearResponse(d * impulse):d*(a.invMass*impulse))"),
        ("d * (b.invMass * impulse)", "(b.rb->Stage0Planar()?b.rb->LinearResponse(d * impulse):d*(b.invMass*impulse))"),
    ):
        edit("physics/WorldMk2Solver.cpp", old, new)
    edit("physics/WorldMk2Solver.cpp", "double(A.invMass + B.invMass)", "double((A.rb->Stage0Planar()||B.rb->Stage0Planar())?glm::dot(rx.n,(A.dynamic?A.rb->LinearResponse(ry.n):glm::vec3(0))+(B.dynamic?B.rb->LinearResponse(ry.n):glm::vec3(0))):A.invMass+B.invMass)")
    # Every cached response tensor follows the same mobility. Physical inertia
    # remains invertible and is retained for the unchanged 3D paths.
    path = source / "src/physics/WorldMk2Solver.cpp"
    text = path.read_text(encoding="utf-8")
    if text.count("->InverseInertiaWorld()") != 3 or text.count(".InverseInertiaWorld()") != 1:
        raise RuntimeError("MK2 response tensor patch sites drifted")
    text = text.replace("->InverseInertiaWorld()", "->ResponseInertiaWorld()").replace(".InverseInertiaWorld()", ".ResponseInertiaWorld()")
    text = replace(text,"const glm::dvec3 v(sb.rb->linearVelocity), w(sb.rb->angularVelocity);", "if(sb.rb->Stage0Planar()){k+=sb.rb->Stage0Energy();continue;}\n                    const glm::dvec3 v(sb.rb->linearVelocity), w(sb.rb->angularVelocity);")
    text = replace(text,"const glm::dvec3 v(part.rb->linearVelocity),w(part.rb->angularVelocity);", "if(part.rb->Stage0Planar()){result+=part.rb->Stage0Energy();continue;}\n        const glm::dvec3 v(part.rb->linearVelocity),w(part.rb->angularVelocity);")
    if text.count("TransportAngularVelocity(body.rigidBody.inverseInertiaLocal,") != 2:
        raise RuntimeError("MK2 angular publication patch sites drifted")
    text = text.replace("TransportAngularVelocity(body.rigidBody.inverseInertiaLocal,", "body.rigidBody.Stage0Planar()?realSpin:TransportAngularVelocity(body.rigidBody.inverseInertiaLocal,")
    text = replace(text,"TransportAngularVelocity(physical.inverseInertiaLocal,", "physical.Stage0Planar()?physical.angularVelocity:TransportAngularVelocity(physical.inverseInertiaLocal,")
    # These dormant/hold leaves must carry the mobility descriptor as well.
    text = replace(text,"rest.inverseMass=body.rigidBody.inverseMass;", "rest.stage0InverseIz=body.rigidBody.stage0InverseIz;rest.stage0Normal=body.rigidBody.stage0Normal;\n                    rest.inverseMass=body.rigidBody.inverseMass;")
    text = replace(text,"rest.inverseMass = body.rigidBody.inverseMass;", "rest.stage0InverseIz=body.rigidBody.stage0InverseIz;rest.stage0Normal=body.rigidBody.stage0Normal;\n            rest.inverseMass = body.rigidBody.inverseMass;")
    path.write_text(text, encoding="utf-8")

    # Existing ordinary joints must see the same condensed response, otherwise
    # contact and joint rows disagree about the body they share.
    path = source / "src/JointSolver.cpp"
    text = path.read_text(encoding="utf-8")
    text = replace(text,"r.linearA*(p*r.a->inverseMass)", "(r.a->Stage0Planar()?r.a->LinearResponse(r.linearA*p):r.linearA*(p*r.a->inverseMass))")
    text = replace(text,"r.linearB*(p*r.b->inverseMass)", "(r.b->Stage0Planar()?r.b->LinearResponse(r.linearB*p):r.linearB*(p*r.b->inverseMass))")
    text = replace(text,"a.InverseInertiaWorld(),ib=b.InverseInertiaWorld()", "a.ResponseInertiaWorld(),ib=b.ResponseInertiaWorld()")
    text = replace(text,"a.inverseMass*glm::dot(la,la)+b.inverseMass*glm::dot(lb,lb)", "(a.Stage0Planar()||b.Stage0Planar()?glm::dot(la,a.LinearResponse(la))+glm::dot(lb,b.LinearResponse(lb)):a.inverseMass*glm::dot(la,la)+b.inverseMass*glm::dot(lb,lb))",2)
    path.write_text(text, encoding="utf-8")
