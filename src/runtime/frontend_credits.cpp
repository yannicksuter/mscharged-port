#include "runtime/frontend_credits.h"
#include "runtime/frontend_handler.h"
#include "resources/frontend_animation.h"
#include "Game/FE/FrontendCreditsSteps.h"
#include "Game/FE/FrontendMainMenuSteps.h"
#include "NL/nlFileGC.h"
#include <algorithm>
#include <cmath>
#include <map>
#include <thread>
namespace mscharged
{
namespace
{
using namespace resources;
void Check(bool v,const char* message){if(!v)throw std::logic_error(message);}
struct Guard{bool& value;explicit Guard(bool& v):value(v){value=true;}~Guard(){value=false;}};
struct Effect{bool audio=false;std::uint32_t cue=0;FrontendCreditsCommand command{};};
struct Step
{
    FrontendAnimationPlayback& playback;FrontendCreditsStatus value;bool widescreen;unsigned video;
    std::vector<Effect> effects;
    bool& mFadeStarted=value.fade_started;float& mTimeElapsed=value.elapsed;unsigned& mPhase=value.phase;
    unsigned mNextScene=13;
    struct EmptySlide{float m_time=0;void Update(float){throw std::logic_error("Credits default component must remain empty");}};
    struct EmptyComponent{EmptySlide* pChildren=nullptr;EmptySlide* m_pActiveSlide=nullptr;} default_fade;
    struct Slide
    {
        Step& owner;std::uint32_t id;
        void Update(float delta){owner.playback.AdvanceSlide(id,delta);}
    };
    struct Component
    {
        Step& owner;std::uint32_t id;
        void SetActiveSlide(const char* name,bool reset,bool preserve)
        {
            if(!id)
            {
                FrontendMainComponentSelect<EmptySlide>(owner.default_fade,FrontendLowerHash(name),reset,preserve,
                    [](EmptySlide* children,unsigned long)->EmptySlide*{Check(!children,"Credits default component gained a slide");return nullptr;});
                return;
            }
            const auto& scene=owner.playback.Scene();const auto it=std::find_if(scene.instances.begin(),scene.instances.end(),[&](const auto& n){return n.offset==id;});
            Check(it!=scene.instances.end()&&it->type==4&&it->library,"Credits fade is not an authored component");
            Check(owner.playback.SelectComponent(*it->library,name,reset,preserve),"Credits fade slide is absent");
        }
    };
    std::map<std::uint32_t,Slide> slides;std::map<std::uint32_t,Component> components;
    struct Presentation
    {
        Step& owner;Slide* m_currentSlide=nullptr;
        void SetActiveSlide(const char* name,bool reset)
        {Check(owner.playback.SelectPresentation(name,reset),"Credits presentation is absent");m_currentSlide=owner.Active();}
    } presentation{*this};Presentation* mPresentation=&presentation;
    Step(FrontendAnimationPlayback& p,FrontendCreditsStatus s,bool wide,unsigned v):playback(p),value(std::move(s)),widescreen(wide),video(v)
    {presentation.m_currentSlide=Active();}
    Slide* Active()
    {const auto id=playback.Scene().active_slide;Check(bool(id),"Credits active slide is absent");return &slides.try_emplace(*id,Slide{*this,*id}).first->second;}
    void Command(FrontendCreditsCommandKind kind,unsigned argument=0)
    {Check(effects.size()<16,"Credits external request budget exceeded");effects.push_back({false,0,{kind,argument}});}
    void SetMovieDetails(const char* path,bool sound,bool loop)
    {
        Check(sound&&!loop,"Credits movie details differ from original profile");
        FrontendCreditsMovieRequest request;request.generation=value.movie?value.movie->generation+1:1;request.path=path;
        value.movie=std::move(request);value.boundary=FrontendCreditsBoundary::MoviePlayback;
    }
    void SetupForCredits(){throw std::logic_error("Credits scrolling awaits genuine movie presentation/completion");}
    void SetupForPhase()
    {
        FrontendCreditsSetupPhase(*this,[&]{return widescreen;},[&]{return video;},
            [&](std::uint32_t cue,int a,int b,int c){Check(!a&&!b&&c==1,"Credits cue arguments differ");effects.push_back({true,cue,{}});},
            [&](bool enabled){Command(FrontendCreditsCommandKind::StadiumRendering,enabled);},
            [&](unsigned scene,int screen,bool pop){Check(!screen&&pop,"Credits replacement topology differs");Command(FrontendCreditsCommandKind::ReplaceScene,scene);},
            [&](unsigned id){Command(FrontendCreditsCommandKind::SelectMusic,id);});
    }
    Component* GetWhiteFadeComponent()
    {
        const std::array<std::string_view,2> path{"Layer","WHITE FADE"};const auto active=playback.Scene().active_slide;
        Check(bool(active),"Credits white fade requires its current presentation");
        const auto found=FindFrontendNode(playback.Scene(),{FrontendNodeKind::Slide,*active},FrontendNamedPath(path));
        if(!found)
        {
            value.default_fade=true;
            return &components.try_emplace(0,Component{*this,0}).first->second;
        }
        Check(found->kind==FrontendNodeKind::Instance,"Credits white fade lookup is not an instance");
        // Original unchecked Find<TLComponentInstance,2> has no stored-type test.
        // Validate the decoded stored component before its native use.
        const auto& all=playback.Scene().instances;const auto it=std::find_if(all.begin(),all.end(),[&](const auto& n){return n.offset==found->id;});
        Check(it!=all.end()&&it->type==4,"Credits white fade stored type is not a component");
        return &components.try_emplace(found->id,Component{*this,found->id}).first->second;
    }
};
}
struct FrontendCredits::Implementation
{
    std::shared_ptr<FrontendSession> session;FrontendInput& input;std::shared_ptr<FrontendAudio> audio;
    unsigned& seed;Services services;bool wide;unsigned video;
    std::unique_ptr<FrontendHandler> handler;Frame current;FrontendCreditsStatus state;
    std::vector<FrontendAudioHandle> sounds;const std::thread::id thread=std::this_thread::get_id();
    bool busy=false,restoration_needed=false;
    Implementation(std::shared_ptr<FrontendSession> s,FrontendInput& i,std::shared_ptr<FrontendAudio> a,unsigned& rng,Services f,bool w,unsigned v)
        :session(std::move(s)),input(i),audio(std::move(a)),seed(rng),services(std::move(f)),wide(w),video(v)
    {
        Check(session&&audio&&audio->Loaded()&&services&&video<=2,"Credits requires retained scene/audio and explicit host services");
        current=session->Current();Check(current&&current->request.animate&&current->request.image_profile==FrontendImageProfile::Main,"Credits requires animated Main resources");
        handler=std::make_unique<FrontendHandler>(session,input);
    }
    void Ready()const{Check(thread==std::this_thread::get_id()&&session&&!state.failed&&!busy&&!nlGetCurrentAsyncRead(),"Credits requires its idle live owner thread");}
    void Expected(const Frame& frame)const{Ready();Check(frame&&frame==current&&frame==session->Current(),"Credits requires its exact retained current frame");}
    void Service(FrontendCreditsCommand command)
    {
        services(command);Check(session->Current()==current,"Credits service changed the session during publication");
    }
    void Effects(const std::vector<Effect>& effects)
    {
        for(const auto& effect:effects)
        {
            if(!effect.audio){Service(effect.command);continue;}
            const auto live=audio->Handles();std::erase_if(sounds,[&](auto h){return std::find(live.begin(),live.end(),h)==live.end();});
            sounds.reserve(sounds.size()+1);const auto handle=audio->Play(effect.cue,seed);
            Check(bool(handle),"Credits original FE_GEN cue was not admitted");sounds.push_back(*handle);
        }
    }
    void Cleanup()
    {
        std::exception_ptr error;
        const auto attempt=[&](auto f){try{f();}catch(...){if(!error)error=std::current_exception();}};
        if(restoration_needed)
        {
            // Original CreditScene destructor enables both, not saved values.
            attempt([&]{services({FrontendCreditsCommandKind::PointerEnabled,1});});
            attempt([&]{services({FrontendCreditsCommandKind::StadiumRendering,1});});restoration_needed=false;
        }
        if(audio&&audio->Loaded()&&!sounds.empty())
        {
            const auto live=audio->Handles();for(auto h:sounds)if(std::find(live.begin(),live.end(),h)!=live.end())attempt([&]{audio->Cancel(h);});
        }
        sounds.clear();if(handler)attempt([&]{handler->Release();});handler.reset();current.reset();session.reset();audio.reset();services={};
        if(error)std::rethrow_exception(error);
    }
};
FrontendCredits::FrontendCredits(std::shared_ptr<FrontendSession> s,FrontendInput& input,std::shared_ptr<FrontendAudio> audio,
    unsigned& seed,Services services,bool wide,unsigned video)
    :impl_(std::make_unique<Implementation>(std::move(s),input,std::move(audio),seed,std::move(services),wide,video))
{
    auto& owner=*impl_;Guard guard(owner.busy);
    try
    {
        owner.restoration_needed=true;owner.Service({FrontendCreditsCommandKind::PointerEnabled,0});
        FrontendCreditsStatus next;std::vector<Effect> effects;
        owner.session->HandlerTransaction(owner.current,[&](auto& playback){Step step(playback,owner.state,wide,video);step.SetupForPhase();step.Command(FrontendCreditsCommandKind::StopMusic);next=std::move(step.value);effects=std::move(step.effects);},[&]{owner.Effects(effects);});
        owner.state=std::move(next);owner.current=owner.session->Current();
    }
    catch(...){owner.state.failed=true;owner.Cleanup();throw;}
}
FrontendCredits::~FrontendCredits(){try{Release();}catch(...){std::terminate();}}
FrontendCredits::Frame FrontendCredits::Current()const{impl_->Ready();return impl_->current;}
FrontendCreditsStatus FrontendCredits::Status()const
{auto& s=*impl_;Check(s.thread==std::this_thread::get_id()&&s.session,"Credits status requires live owner thread");return s.state;}
void FrontendCredits::Update(const Frame& expected,float delta)
{
    auto& s=*impl_;s.Expected(expected);Check(std::isfinite(delta)&&delta>=0&&delta<=60,"Credits delta exceeds original bounded profile");
    Check(s.state.boundary==FrontendCreditsBoundary::None,"Credits requires actual THP presentation/audio completion; decoded EOF is not movie completion");
    Guard guard(s.busy);
    try
    {
        // Original CreditScene::Update phase0 executes exactly one BaseUpdate.
        s.handler->Update(s.current,delta);s.current=s.session->Current();FrontendCreditsStatus next;std::vector<Effect> effects;
        s.session->HandlerTransaction(s.current,[&](auto& playback){Step step(playback,s.state,s.wide,s.video);FrontendCreditsUpdateNintendoLogo(step,delta);next=std::move(step.value);effects=std::move(step.effects);},[&]{s.Effects(effects);});
        s.state=std::move(next);s.current=s.session->Current();
    }
    catch(...){s.state.failed=true;throw;}
}
void FrontendCredits::Release()
{
    auto& s=*impl_;Check(s.thread==std::this_thread::get_id()&&!s.busy&&!nlGetCurrentAsyncRead(),"Credits teardown requires its idle owner thread");
    if(!s.session)return;Guard guard(s.busy);s.Cleanup();
}
}
