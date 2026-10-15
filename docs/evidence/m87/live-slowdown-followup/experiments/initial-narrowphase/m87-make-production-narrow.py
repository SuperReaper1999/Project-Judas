from pathlib import Path
s=Path('/tmp/m87-narrow-prototype.cpp').read_text()
a=s.index('// Temporary isolated prototype service injection.')
b=s.index('namespace {',a)
s=s[:a]+s[b:]
s=s.replace('''        ContactManifold manifold;bool touching=false;''','''        ContactManifold manifold;bool touching=false,ready=false;''',1)
s=s.replace('''    std::vector<InitialContactGeometry> initialContactGeometry;''','''    // Optional borrowed process service; no physical state or worker ownership.
    JobSystem* contactJobs=nullptr;unsigned contactJobLanes=4;
    static constexpr std::size_t kMaxInitialContactPairs=16384;
    std::vector<InitialContactGeometry> initialContactGeometry;''',1)
s=s.replace('''        initialContactGeometry.clear();JobSystem* jobs=m87PrototypeJobs;
        if(!jobs||jobs->IsShutDown()||m87PrototypeWorkerLimit==0||candidatePairs.size()<256)return;''','''        initialContactGeometry.clear();JobSystem* jobs=contactJobs;
        JUDAS_PROFILE_COUNTER("Physics initial geometry worker lanes",0,ProfileCounterMode::Latest);
        JUDAS_PROFILE_COUNTER("Physics parallel initial manifolds",0,ProfileCounterMode::Latest);
        JUDAS_PROFILE_COUNTER("Physics initial geometry scratch bytes",initialContactGeometry.capacity()*sizeof(InitialContactGeometry),ProfileCounterMode::Latest);
        if(!jobs||jobs->IsShutDown()||contactJobLanes<=1||candidatePairs.size()<256)return;''',1)
s=s.replace('''        const auto count=std::count_if(candidatePairs.begin(),candidatePairs.end(),eligible);
        if(count<256)return;''','''        std::size_t count=0;
        for(const auto& pair:candidatePairs)if(eligible(pair)&&++count>kMaxInitialContactPairs)return;
        if(count<256)return;''',1)
s=s.replace('''const unsigned lanes=unsigned(std::min({std::size_t(m87PrototypeWorkerLimit),std::size_t(jobs->WorkerCount())+1,initialContactGeometry.size()/64}));''','''const unsigned lanes=unsigned(std::min({std::size_t(contactJobLanes),std::size_t(jobs->WorkerCount())+1,initialContactGeometry.size()/64}));''',1)
a=s.index('''        auto run=[&](unsigned lane){''',s.index('    void PrepareInitialContactGeometry('))
b=s.index('''        std::array<JobHandle,JobSystem::kMaxWorkers> handles''',a)
replacement='''        const auto compute=[&](InitialContactGeometry& output){
            const auto& a=bodies[output.a];const auto& b=bodies[output.b];
            const auto childA=PrimitiveAt(a.shape,a.rigidBody,0,output.orientationA);
            const auto childB=PrimitiveAt(b.shape,b.rigidBody,0,output.orientationB);
            output.manifold=ComputeContacts(childA,childB,output.margin,output.orientationA,output.orientationB);
            output.touching=false;
            for(int p=0;p<output.manifold.count;++p){const auto& c=output.manifold.points[p];output.touching=output.touching||(c.hasLocalAnchors?c.signedSeparation<=0:c.penetration>=0);}
            if(!output.touching&&output.manifold.count)output.touching=ComputeContacts(childA,childB,0,output.orientationA,output.orientationB).count>0;
            output.ready=true;
        };
        auto run=[&](unsigned lane){
            const auto contactsBefore=GetContactGeometryDiagnostics();const auto cookedBefore=GetCollisionGeometryStats();
            JUDAS_PROFILE_SCOPE("Physics initial geometry worker batch");
            try {
                for(;;){const auto first=next.fetch_add(8,std::memory_order_relaxed);if(first>=initialContactGeometry.size())break;
                    for(auto i=first;i<std::min(first+8,initialContactGeometry.size());++i)compute(initialContactGeometry[i]);
                }
            }catch(...){AddContactDiagnostics(diagnostics[lane].contacts,GetContactGeometryDiagnostics(),contactsBefore);AddCookedDiagnostics(diagnostics[lane].cooked,GetCollisionGeometryStats(),cookedBefore);throw;}
            AddContactDiagnostics(diagnostics[lane].contacts,GetContactGeometryDiagnostics(),contactsBefore);AddCookedDiagnostics(diagnostics[lane].cooked,GetCollisionGeometryStats(),cookedBefore);
        };
'''
s=s[:a]+replacement+s[b:]
s=s.replace('''        JobHandle failed;for(unsigned i=0;i<handleCount;++i)if(jobs->Wait(handles[i])!=JobState::Completed&&!failed.IsValid())failed=handles[i];''','''        // The owner has exhausted the shared range cursor. A job still queued
        // behind unrelated resource work can no longer claim useful ranges.
        // Cancel only these jobs; running jobs retain their claimed outputs.
        for(unsigned i=0;i<handleCount;++i)if(jobs->StateOf(handles[i])==JobState::Queued)jobs->Cancel(handles[i]);
        JobHandle failed;
        for(unsigned i=0;i<handleCount;++i)if(jobs->Wait(handles[i])==JobState::Failed&&!failed.IsValid())failed=handles[i];''',1)
s=s.replace('''        if(!workerError.empty())throw std::runtime_error(workerError);
        JUDAS_PROFILE_COUNTER''','''        if(!workerError.empty())throw std::runtime_error(workerError);
        // Shutdown may cancel jobs before they start; incomplete outputs then
        // run through the unchanged owner-thread geometry path after all joins.
        // Never fall back concurrently with a worker writing an output slot.
        for(auto& output:initialContactGeometry)if(!output.ready)compute(output);
        JUDAS_PROFILE_COUNTER''',1)
needle='''void PhysicsWorld::Shutdown() {
    delete m_impl;
    m_impl = nullptr;
}
'''
replacement=needle+'''
void PhysicsWorld::SetJobSystem(JobSystem* jobs,unsigned maxLanes) {
    if(!m_impl)throw std::logic_error("physics job service requires initialized world");
    if(m_impl->stepping||m_impl->coupledStep)throw std::logic_error("physics job service requires completed step boundary");
    m_impl->contactJobs=jobs;
    m_impl->contactJobLanes=std::min(maxLanes,JobSystem::kMaxWorkers+1);
    m_impl->initialContactGeometry.clear();
}
'''
assert s.count(needle)==1;s=s.replace(needle,replacement,1)
s=s.replace('        JUDAS_PROFILE_COUNTER("Physics parallel initial manifolds",initialContactGeometry.size(),ProfileCounterMode::Latest);','        JUDAS_PROFILE_COUNTER("Physics parallel initial manifolds",initialContactGeometry.size(),ProfileCounterMode::Latest);\n        JUDAS_PROFILE_COUNTER("Physics initial geometry scratch bytes",initialContactGeometry.capacity()*sizeof(InitialContactGeometry),ProfileCounterMode::Latest);',1)
Path('/tmp/m87-production-narrow.cpp').write_text(s)
