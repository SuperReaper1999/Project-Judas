// Exercise actual project dialogue through ordinary interaction/UI. No script
// state or audio-play calls are injected by this observer.
  job(5,[&](EngineHost&,RuntimeWorld& w){AimEntityFixture(w,6000);});
  job(5,[&](EngineHost&,RuntimeWorld&){press("interact");});
  job(8,[&](EngineHost& h,RuntimeWorld& w){
    Check(w.UI().Paused(),"actual survivor conversation pauses gameplay");
    auto s=VoiceSnapshot(h,w,990000000);
    Check(s&&s->state==AudioPlaybackState::Playing&&s->durationSeconds>4,"Mara subtitle starts its real decoded speech voice while world is paused");
    Screenshot(h,out/"mara-voiced.png");

  });
  job(35,[&](EngineHost& h,RuntimeWorld& w){
    auto s=VoiceSnapshot(h,w,990000000);
    Check(s&&s->positionSeconds>.08,"native audio cursor advances during the paused conversation");
    Check(audioMixedEnergy>0.01,"native mixer produces nonzero dialogue PCM");
    Check(h.Audio().VoiceCount()<40,"dialogue refresh does not flood native voices");
  });
  select(1);
  job(6,[&](EngineHost& h,RuntimeWorld& w){
    auto a=VoiceSnapshot(h,w,990000000),b=VoiceSnapshot(h,w,990000001);
    Check(a&&a->state==AudioPlaybackState::Stopped,"changing topic stops the former native voice");
    Check(b&&b->state==AudioPlaybackState::Playing,"selected second topic plays its distinct native clip");
  });
  click("secondary");
  job(6,[&](EngineHost& h,RuntimeWorld& w){
    auto s=VoiceSnapshot(h,w,990000001);
    Check(s&&s->state==AudioPlaybackState::Stopped,"opening trade stops conversation audio");
    press("pause");
  });
  job(5,[&](EngineHost&,RuntimeWorld& w){AimEntityFixture(w,6100);});
  job(5,[&](EngineHost&,RuntimeWorld&){press("interact");});
  job(8,[&](EngineHost& h,RuntimeWorld& w){
    auto s=VoiceSnapshot(h,w,990000003);
    Check(s&&s->state==AudioPlaybackState::Playing,"Ivo interaction starts the second synthetic voice");
    Screenshot(h,out/"ivo-voiced.png");press("pause");
  });
  job(8,[&](EngineHost& h,RuntimeWorld& w){
    auto s=VoiceSnapshot(h,w,990000003);
    Check(s&&s->state==AudioPlaybackState::Stopped&&!w.UI().Paused(),"leaving dialogue stops native playback and resumes gameplay");

  });
  job(5,[&](EngineHost&,RuntimeWorld& w){AimEntityFixture(w,6000);});
  job(5,[&](EngineHost&,RuntimeWorld&){press("interact");});
  job(8,[&](EngineHost& h,RuntimeWorld& w){
    auto s=VoiceSnapshot(h,w,990000000);
    Check(s&&s->state==AudioPlaybackState::Playing,"reopened dialogue plays before save");
    Write(out/"expected.json",{{"bag",State(w)["bag"]}});press("save");
  });
  job(20,[&](EngineHost& h,RuntimeWorld& w){
    auto s=VoiceSnapshot(h,w,990000000);
    Check(s&&s->state==AudioPlaybackState::Stopped,"saving from dialogue stops transient speech without replay");
    decltype(w.SceneControl()->Saves(h.Resources())->Status(1)) status=nullptr;
    for(uint64_t id=1;id<64;++id){auto* row=w.SceneControl()->Saves(h.Resources())->Status(id);if(row&&row->operation=="save")status=row;}
    if(!status||status->state!="completed"){
      if(status&&status->state=="failed")throw std::runtime_error(status->error);
      --index;return;
    }
    Check(true,"normal project save completes while conversation is open");
    done=true;Quit();
  });
