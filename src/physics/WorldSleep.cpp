#include "WorldInternal.h"

using namespace JudasPhysicsDetail;

auto PhysicsWorld::Impl::Wake(unsigned slot) -> void {
    PhysicsWorkTimer timer(work.wake,work.active);
    if (slot >= bodies.size() || !bodies[slot].alive)
        return;
    // If every live body already has exactly the state this publication can
    // produce, graph discovery cannot change any physical or sleeping state.
    // Inspect authoritative state rather than retaining an invalidatable
    // "already awake" cache: Settle, restore and new bodies remain visible.
    if (std::all_of(aliveSlots.begin(), aliveSlots.end(), [&](unsigned i) {
            return !bodies[i].sleeping && bodies[i].quietSeconds == 0;
        })) {
        ++work.wakeNoops;
        return;
    }
    ++work.wakeGraphTraversals;
    // Snapshot the current wake graph once. The previous traversal scanned
    // every joint/contact again for every reached body (and allocated a set
    // node for each visit). Topology can change between calls, so rebuild this
    // bounded adjacency rather than retaining a stale sleeping-island graph.
    std::vector<std::pair<unsigned,unsigned>> edges;
    edges.reserve(2*(joints.size()+previousTouches.size()+contactCache.size()+constraintSupports.size()));
    const auto link=[&](unsigned from,unsigned to){edges.emplace_back(from,to);};
    for(const auto& j:joints)if(j.state.settings.enabled) {
        const auto a=j.state.settings.bodyA,b=j.state.settings.bodyB;
        if(!Get(a)||!Get(b))continue;
        const auto x=a.id&kSlotMask,y=b.id&kSlotMask;
        link(x,y);if(x!=y)link(y,x);
    }
    for(const auto& [_,e]:previousTouches)if(!e.sensor&&Get(e.a)&&Get(e.b)) {
        const auto a=e.a.id&kSlotMask,b=e.b.id&kSlotMask;
        if(!bodies[b].rigidBody.IsStatic())link(a,b);
        if(!bodies[a].rigidBody.IsStatic())link(b,a);
    }
    for(const auto& c:contactCache)if(c.normalImpulse>0) {
        const auto a=c.key.slotA,b=c.key.slotB;
        if(a>=bodies.size()||b>=bodies.size()||!bodies[a].alive||!bodies[b].alive||
           bodies[a].generation!=c.key.generationA||bodies[b].generation!=c.key.generationB)continue;
        if(!bodies[b].rigidBody.IsStatic())link(a,b);
        if(!bodies[a].rigidBody.IsStatic())link(b,a);
    }
    for(const auto& edge:constraintSupports)if(Get(edge.body)&&Get(edge.support)) {
        const auto a=edge.body.id&kSlotMask,b=edge.support.id&kSlotMask;
        if(bodies[b].isDynamic)link(a,b);
        if(bodies[a].isDynamic)link(b,a);
    }
    std::vector<std::size_t> offsets(bodies.size()+1,0);
    for(auto [from,to]:edges)++offsets[from+1];
    for(std::size_t n=1;n<offsets.size();++n)offsets[n]+=offsets[n-1];
    auto cursor=offsets;
    std::vector<unsigned> neighbours(edges.size());
    for(auto [from,to]:edges)neighbours[cursor[from]++]=to;
    std::vector<unsigned char> visited(bodies.size(),0);
    std::vector<unsigned> queue;queue.reserve(bodies.size());queue.push_back(slot);visited[slot]=1;
    work.wakeGraphEdges=std::max(work.wakeGraphEdges,edges.size());
    work.wakeScratchBytes=std::max(work.wakeScratchBytes,
        edges.capacity()*sizeof(edges[0])+(offsets.capacity()+cursor.capacity())*sizeof(std::size_t)+
        (neighbours.capacity()+queue.capacity())*sizeof(unsigned)+visited.capacity()*sizeof(unsigned char));
    for(std::size_t i=0;i<queue.size();++i) {
        const auto n=queue[i];auto& body=bodies[n];
        body.sleeping=false;body.quietSeconds=0;
        if(n!=slot&&body.rigidBody.IsStatic())continue;
        for(std::size_t e=offsets[n];e<offsets[n+1];++e) {
            const auto other=neighbours[e];
            if(!visited[other]){visited[other]=1;queue.push_back(other);}
        }
    }
}

auto PhysicsWorld::Impl::Settle(float dt) -> void {
    ++sleepTraceStep;
    sleepGraphEdges = sleepIslandCount = 0;
    if (!sleepingEnabled) {
        for (auto i : aliveSlots)
            if (bodies[i].isDynamic)
                ++stats.awakeBodies;
        return;
    }
    sleepParents.resize(bodies.size());
    for (unsigned i = 0; i < sleepParents.size(); ++i)
        sleepParents[i] = i;
    sleepNext.assign(bodies.size(), kNoSleepMember);
    sleepIslands.assign(bodies.size(), SleepIsland{});
    sleepRoots.clear();
    auto root = [&](unsigned i) {
        while (sleepParents[i] != i) {
            sleepParents[i] = sleepParents[sleepParents[i]];
            i = sleepParents[i];
        }
        return i;
    };
    auto unite = [&](unsigned a, unsigned b) {
        ++sleepGraphEdges;
        if (bodies[a].isDynamic && bodies[b].isDynamic)
            sleepParents[root(b)] = root(a);
    };
    // Preserve the original union and row order: changing roots or solver
    // order is unnecessary for eliminating the per-island global rescans.
    auto validSupport = [&](const CachedContact &c) {
        return c.normalImpulse > 0 && c.key.slotA < bodies.size() && c.key.slotB < bodies.size() &&
               bodies[c.key.slotA].alive && bodies[c.key.slotB].alive &&
               bodies[c.key.slotA].generation == c.key.generationA &&
               bodies[c.key.slotB].generation == c.key.generationB;
    };
    for (const auto &c : contactCache)
        if (validSupport(c))
            unite(c.key.slotA, c.key.slotB);
    for (const auto &[_, e] : stepTouches)
        if (!e.sensor && Get(e.a) && Get(e.b))
            unite(e.a.id & kSlotMask, e.b.id & kSlotMask);
    for (const auto &j : joints)
        if (JointActive(j) && Get(j.state.settings.bodyB))
            unite(j.state.settings.bodyA.id & kSlotMask, j.state.settings.bodyB.id & kSlotMask);
    for(const auto& edge:constraintSupports)if(Get(edge.body)&&Get(edge.support))
        unite(edge.body.id&kSlotMask,edge.support.id&kSlotMask);
    for (auto i : aliveSlots)
        if (bodies[i].isDynamic && bodies[i].enabled && !bodies[i].sensor) {
            const auto r = root(i);
            auto &island = sleepIslands[r];
            if (island.first == kNoSleepMember) {
                island.first = i;
                sleepRoots.push_back(r);
            } else
                sleepNext[island.last] = i;
            island.last = i;
            ++island.count;
        }
    // std::map previously visited roots in ascending order and members in
    // alive-slot order. Keep both orders, without allocating a map/vector
    // for every independent machine on every fixed step.
    std::sort(sleepRoots.begin(), sleepRoots.end());
    sleepIslandCount = sleepRoots.size();
    sleepScratchBytes = (sleepParents.capacity() + sleepNext.capacity() + sleepRoots.capacity()) *
                            sizeof(unsigned) +
                        sleepIslands.capacity() * sizeof(SleepIsland);
    auto anchorPair = [&](unsigned a, unsigned b) {
        if (bodies[b].rigidBody.IsStatic())
            sleepIslands[root(a)].anchored = true;
        if (bodies[a].rigidBody.IsStatic())
            sleepIslands[root(b)].anchored = true;
    };
    for (const auto &c : contactCache)
        if (validSupport(c))
            anchorPair(c.key.slotA, c.key.slotB);
    for (const auto &[_, e] : stepTouches)
        if (!e.sensor && Get(e.a) && Get(e.b))
            anchorPair(e.a.id & kSlotMask, e.b.id & kSlotMask);
    for(const auto& edge:constraintSupports)if(Get(edge.body)&&Get(edge.support)) {
        const auto a=edge.body.id&kSlotMask,b=edge.support.id&kSlotMask;
        anchorPair(a,b);
        sleepIslands[root(a)].quiet &= edge.quiet;
    }
    for (const auto &j : joints)
        if (JointActive(j)) {
            const auto &s = j.state.settings;
            auto *a = Get(s.bodyA);
            auto *b = Get(s.bodyB);
            auto &island = sleepIslands[root(s.bodyA.id & kSlotMask)];
            // This deliberately retains the established body-A anchor policy;
            // a performance pass must not silently change sleep semantics.
            island.anchored |= !b || b->rigidBody.IsStatic();
            if (island.first == kNoSleepMember)
                continue;
            // Finite material connections are not rigid welds. Their current
            // equilibrium includes elastic deflection and permanent rest strain;
            // requiring the original anchor/angle keeps a damaged assembly awake
            // forever. Actual body motion and support still gate the entire
            // island below, and forces/configuration changes retain normal wake.
            if(j.state.constitutive.material.enabled)continue;
            auto pa = a->rigidBody.position +
                      a->rigidBody.orientation * (s.anchorA - a->shape.pivotOffset);
            auto pb = b ? b->rigidBody.position +
                              b->rigidBody.orientation * (s.anchorB - b->shape.pivotOffset)
                        : s.anchorB;
            auto error = pb - pa;
            if (s.type == JointType::Slider) {
                auto axis = a->rigidBody.orientation * s.frameA * glm::vec3(1, 0, 0);
                error -= axis * glm::dot(error, axis);
            }
            island.quiet &= glm::length(error) < .02f;
            island.quiet &= !s.motor || s.speed == 0;
            auto qa = glm::normalize(a->rigidBody.orientation * s.frameA),
                 qb = b ? glm::normalize(b->rigidBody.orientation * s.frameB)
                        : glm::normalize(s.frameB);
            if (s.type == JointType::Fixed || s.type == JointType::Slider)
                island.quiet &=
                    2 * std::acos(std::clamp(glm::abs(glm::dot(qa, qb)), 0.f, 1.f)) < .01f;
            if (s.type == JointType::Hinge)
                island.quiet &=
                    glm::length(qa * glm::vec3(1, 0, 0) - qb * glm::vec3(1, 0, 0)) < .01f;
            if (s.spring)
                island.quiet &= std::abs(j.state.coordinate - j.previousCoordinate) < .002f;
            if (s.limits)
                island.quiet &=
                    j.state.coordinate >= s.lower - .01f && j.state.coordinate <= s.upper + .01f;
        }
    for (auto i : aliveSlots)
        if (bodies[i].isDynamic && bodies[i].enabled && !bodies[i].sensor) {
            auto &b = bodies[i];
            auto &island = sleepIslands[root(i)];
            island.quiet &=
                glm::length(b.rigidBody.linearVelocity) < .03f &&
                glm::length(b.rigidBody.angularVelocity) < .03f &&
                glm::length(b.rigidBody.position - b.previousPosition) < .002f &&
                glm::abs(glm::dot(b.rigidBody.orientation, b.previousOrientation)) > .999999f;
            island.seconds = std::min(island.seconds, b.quietSeconds);
        }
    // Retain the existing geometric neighbourhood test exactly. Debug
    // contacts do not carry body identity; proximity must not be replaced
    // with guessed ownership. Only genuinely deep rows take this path.
    for (const auto &c : lastStepContacts)
        if (c.penetration > .02f) {
            for (auto i : aliveSlots)
                if (bodies[i].isDynamic && bodies[i].enabled && !bodies[i].sensor &&
                    glm::length(c.point - bodies[i].rigidBody.position) <
                        bodies[i].boundingRadius + .1f)
                    sleepIslands[root(i)].quiet = false;
        }
    const bool trace = std::getenv("JUDAS_SLEEP_TRACE") && sleepTraceStep % 60 == 0;
    for (auto r : sleepRoots) {
        auto &island = sleepIslands[r];
        if (trace) {
            float linear = 0, angular = 0, travel = 0, anchorError = 0, angularError = 0,
                  penetration = 0;
            bool drive = false, limit = true;
            for (auto i = island.first; i != kNoSleepMember; i = sleepNext[i]) {
                const auto &b = bodies[i];
                linear = std::max(linear, glm::length(b.rigidBody.linearVelocity));
                angular = std::max(angular, glm::length(b.rigidBody.angularVelocity));
                travel = std::max(travel, glm::length(b.rigidBody.position - b.previousPosition));
            }
            for (const auto &j : joints)
                if (JointActive(j) && root(j.state.settings.bodyA.id & kSlotMask) == r) {
                    const auto &x = j.state.settings;
                    auto *a = Get(x.bodyA);
                    auto *b = Get(x.bodyB);
                    auto pa = a->rigidBody.position +
                              a->rigidBody.orientation * (x.anchorA - a->shape.pivotOffset),
                         pb = b ? b->rigidBody.position +
                                      b->rigidBody.orientation * (x.anchorB - b->shape.pivotOffset)
                                : x.anchorB;
                    anchorError = std::max(anchorError, glm::length(pa - pb));
                    drive |= x.motor && x.speed != 0;
                    limit &= !x.limits || (j.state.coordinate >= x.lower - .01f &&
                                           j.state.coordinate <= x.upper + .01f);
                    auto qa = glm::normalize(a->rigidBody.orientation * x.frameA),
                         qb = b ? glm::normalize(b->rigidBody.orientation * x.frameB)
                                : glm::normalize(x.frameB);
                    if (x.type == JointType::Hinge)
                        angularError = std::max(angularError, glm::length(qa * glm::vec3(1, 0, 0) -
                                                                          qb * glm::vec3(1, 0, 0)));
                }
            for (const auto &c : lastStepContacts)
                for (auto i = island.first; i != kNoSleepMember; i = sleepNext[i])
                    if (glm::length(c.point - bodies[i].rigidBody.position) <
                        bodies[i].boundingRadius + .1f)
                        penetration = std::max(penetration, c.penetration);
            std::fprintf(
                stderr,
                "SLEEP step=%llu root=%u members=%zu v=%g w=%g travel=%g anchor=%g axis=%g penetration=%g drive=%d limits=%d quiet=%d time=%g supported=%d\n",
                (unsigned long long)sleepTraceStep, island.first, std::size_t(island.count), linear,
                angular, travel, anchorError, angularError, penetration, drive, limit, island.quiet,
                island.seconds, island.anchored);
        }
        island.seconds = island.quiet ? island.seconds + dt : 0;
        bool freeAtRest = true;
        if (island.quiet && island.seconds >= .75f && !island.anchored) {
            for (auto i = island.first; i != kNoSleepMember; i = sleepNext[i])
                if (!(glm::length(bodies[i].lastAcceleration) < 1e-6f &&
                      glm::length(bodies[i].rigidBody.linearVelocity) < 1e-6f &&
                      glm::length(bodies[i].rigidBody.angularVelocity) < 1e-6f)) {
                    freeAtRest = false;
                    break;
                }
        }
        const bool asleep =
            island.quiet && island.seconds >= .75f && (island.anchored || freeAtRest);
        for (auto i = island.first; i != kNoSleepMember; i = sleepNext[i]) {
            auto &b = bodies[i];
            b.quietSeconds = island.seconds;
            b.sleeping = asleep;
            if (asleep) {
                b.rigidBody.linearVelocity = {0, 0, 0};
                b.rigidBody.angularVelocity = {0, 0, 0};
            }
        }
        stats.sleepingIslands += asleep;
    }
    for (auto &j : joints)
        j.previousCoordinate = j.state.coordinate;
    for (auto i : aliveSlots)
        if (bodies[i].isDynamic) {
            stats.sleepingBodies += bodies[i].sleeping;
            stats.awakeBodies += !bodies[i].sleeping;
        }
}

bool PhysicsWorld::IsSleeping(BodyHandle h) const {
    auto *b = m_impl ? m_impl->Get(h) : nullptr;
    return b && b->sleeping;
}

void PhysicsWorld::Wake(BodyHandle h) {
    if (m_impl && m_impl->Get(h))
        m_impl->Wake(h.id & Impl::kSlotMask);
}

void PhysicsWorld::SetSleepingEnabled(bool enabled) {
    m_impl->sleepingEnabled = enabled;
    if (!enabled)
        for (auto i : m_impl->aliveSlots)
            m_impl->Wake(i);
}

void PhysicsWorld::PersistSleep(SaveArchive &a, BodyHandle h) {
    auto *b = m_impl->Get(h);
    a.Require(b, "missing sleep body");
    a(b->sleeping, b->quietSeconds, b->lastAcceleration, b->deferredAcceleration);
    a.Require(std::isfinite(b->quietSeconds) && b->quietSeconds >= 0 &&
                  std::isfinite(b->lastAcceleration.x) && std::isfinite(b->lastAcceleration.y) &&
                  std::isfinite(b->lastAcceleration.z),
              "invalid settling state");
}

void PhysicsWorld::SetConstraintSupports(std::vector<ConstraintSupport> supports) {
    auto key=[](const auto& edge){return std::make_pair(edge.body.id,edge.support.id);};
    supports.erase(std::remove_if(supports.begin(),supports.end(),[&](const auto& edge){
        return !m_impl->Get(edge.body)||!m_impl->Get(edge.support)||
               !IsBodyEnabled(edge.body)||!IsBodyEnabled(edge.support);
    }),supports.end());
    std::sort(supports.begin(),supports.end(),[&](const auto& a,const auto& b){return key(a)<key(b);});
    for(const auto& previous:m_impl->constraintSupports) {
        const auto found=std::lower_bound(supports.begin(),supports.end(),previous,
            [&](const auto& a,const auto& b){return key(a)<key(b);});
        if(found==supports.end()||key(*found)!=key(previous))Wake(previous.body);
    }
    for(const auto& edge:supports)if(!edge.quiet)Wake(edge.body);
    m_impl->constraintSupports=std::move(supports);
}
