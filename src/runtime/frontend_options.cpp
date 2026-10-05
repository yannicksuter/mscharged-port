#include "runtime/frontend_options.h"
#include "runtime/frontend_handler.h"
#include "resources/frontend_animation.h"
#include "Game/FE/FrontendOptionsSteps.h"
#include "Game/FE/FrontendMainMenuSteps.h"
#include "NL/nlFileGC.h"
#include <algorithm>
#include <map>
#include <thread>

namespace mscharged
{
namespace
{
using namespace resources;
void CheckOptions(bool value,const char* message){if(!value)throw std::logic_error(message);}
struct Position {struct{float x,y,z;} f;};
struct OptionsState
{
    FrontendOptionsStatus status;
    std::array<std::uint32_t,3> instances{};
    std::array<FrontendPointerBinding,3> bindings{};
    std::array<bool,3> disabled{};
};
struct Step
{
    FrontendAnimationPlayback& playback;
    OptionsState value;
    int mState=value.status.state;
    int mNextScene=value.status.next_scene;
    struct Slide
    {
        Step& owner;std::uint32_t id;
        const FrontendSlide& Value()const
        {const auto& s=owner.playback.Scene().slides;auto it=std::find_if(s.begin(),s.end(),[&](const auto& v){return v.offset==id;});CheckOptions(it!=s.end(),"Options slide is absent");return *it;}
        float GetCurrentTime()const{return Value().time;}
        float GetStartTime()const{return Value().start;}
        float GetDuration()const{return Value().duration;}
    };
    struct Instance
    {
        Step& owner;std::uint32_t id;
        FrontendNode Root()const{return {FrontendNodeKind::Instance,id};}
        const FrontendInstance& Value()const
        {const auto& values=owner.playback.Scene().instances;auto it=std::find_if(values.begin(),values.end(),[&](const auto& v){return v.offset==id;});CheckOptions(it!=values.end(),"Options instance is absent");return *it;}
        Position GetAssetPosition()const{const auto& p=Value().attributes.position;return {{p[0],p[1],p[2]}};}
        void SetActiveSlide(const char* name,bool reset,bool preserve)
        {const auto& v=Value();CheckOptions(v.type==4&&v.library,"Options callback requires its actual component");CheckOptions(owner.playback.SelectComponent(*v.library,name,reset,preserve),"Options feedback slide is absent");}
    };
    struct Presentation
    {
        Step& owner;Slide* m_currentSlide=nullptr;
        FrontendNode Root()const{return {};}
        void SetActiveSlide(const char* name,bool reset)
        {CheckOptions(owner.playback.SelectPresentation(name,reset),"Options authored presentation slide is absent");m_currentSlide=owner.ActiveSlide();}
        void Update(float delta){owner.playback.Advance(delta);m_currentSlide=owner.ActiveSlide();}
    } presentation{*this};
    Presentation* mPresentation=&presentation;
    struct Button
    {
        std::array<int,4> states{};FrontendPointerBinding binding;bool disabled=false;
        bool HasOtherPointerState(int v,unsigned index)const{return FrontendPointerHasOtherState(states,v,index);}
        void SetPointerState(int v,unsigned index){CheckOptions(index<4,"Options pointer exceeds four");states[index]=v;}
        void SetInstanceBounds(Instance* instance,bool rotate,float x,float y,float sx,float sy)
        {CheckOptions(instance,"Options hit instance is absent");const auto type=instance->Value().type;CheckOptions(type>=1&&type<=5,"Options hit instance type is unsupported");binding={instance->id,rotate,x,y,sx,sy};}
        void Disable(){disabled=true;}
    };
    std::array<Instance*,3> mOptionInstances{};
    std::array<Button,3> mOptionButtons{};
    std::map<std::uint32_t,Instance> instances;
    std::map<std::uint32_t,Slide> slides;
    Step(FrontendAnimationPlayback& p,OptionsState v):playback(p),value(std::move(v))
    {
        for(unsigned i=0;i<3;++i)
        {
            if(value.instances[i])mOptionInstances[i]=At(value.instances[i]);
            mOptionButtons[i]={value.status.pointer_states[i],value.bindings[i],value.disabled[i]};
        }
        presentation.m_currentSlide=ActiveSlide();
    }
    Instance* At(std::uint32_t id){return &instances.try_emplace(id,Instance{*this,id}).first->second;}
    Slide* ActiveSlide(){const auto id=playback.Scene().active_slide;CheckOptions(bool(id),"Options presentation has no active slide");return &slides.try_emplace(*id,Slide{*this,*id}).first->second;}
    Presentation* GetPresentation(){return &presentation;}
    template<class T,int N>struct Finder
    {
        template<class Root,class... Names>static T* Find(Root* root,Names... names)
        {
            const std::array<std::string_view,sizeof...(Names)> path{names...};
            // Original FindChecked returns by hash without inspecting template N.
            // In retail, Find<TLComponentInstance,3> here returns an IMAGE.
            const auto node=FindFrontendNode(root->owner.playback.Scene(),root->Root(),FrontendNamedPath(path),FrontendNodeType::Any);
            CheckOptions(node&&node->kind==FrontendNodeKind::Instance,"Options authored lookup path is absent");return root->owner.At(node->id);
        }
    };
    void Command(FrontendOptionsCommandKind kind,unsigned argument=0)
    {CheckOptions(value.status.commands.size()<32,"Options dependency command budget exceeded");value.status.commands.push_back({kind,argument});}
    OptionsState Result()
    {
        value.status.state=mState;value.status.next_scene=mNextScene;
        for(unsigned i=0;i<3;++i)
        {
            CheckOptions(mOptionInstances[i],"Options button component is absent");
            CheckOptions(mOptionInstances[i]->Value().type==4,"Options button is not a component");
            value.instances[i]=mOptionInstances[i]->id;value.status.pointer_states[i]=mOptionButtons[i].states;
            value.bindings[i]=mOptionButtons[i].binding;value.disabled[i]=mOptionButtons[i].disabled;
        }
        return std::move(value);
    }
};
struct Pending {FrontendPointerCallback kind;unsigned item,index;FrontendSession::Handle frame;};
struct Busy {bool& value;explicit Busy(bool& v):value(v){value=true;}~Busy(){value=false;}};
}
struct FrontendOptions::Implementation
{
    std::shared_ptr<FrontendSession> session;
    FrontendInput& input;
    std::shared_ptr<FrontendAudio> audio;
    unsigned& seed;unsigned controller;
    std::thread::id thread=std::this_thread::get_id();
    FrontendSession::Handle current;
    OptionsState state;
    FrontendPointerHost host;
    std::unique_ptr<FrontendHandler> handler;
    std::array<std::shared_ptr<FrontendPointerRegion>,3> regions{};
    std::vector<Pending> pending;
    std::vector<FrontendAudioHandle> sounds;
    bool failed=false,busy=false;
    Implementation(std::shared_ptr<FrontendSession> s,FrontendInput& i,std::shared_ptr<FrontendAudio> a,unsigned& rng,unsigned c)
        :session(std::move(s)),input(i),audio(std::move(a)),seed(rng),controller(c),host(i,c)
    {
        CheckOptions(controller<4&&session&&audio&&audio->Loaded(),"Options requires scene, resident audio and a valid controller");
        current=session->Current();CheckOptions(current&&current->request.animate&&current->request.image_profile==FrontendImageProfile::Main,"Options requires animated Main-profile resources");
        const auto slide=std::find_if(current->graph.slides.begin(),current->graph.slides.end(),[&](const auto& v){return v.offset==current->graph.active_slide;});
        CheckOptions(slide!=current->graph.slides.end()&&slide->name=="in","Options owner requires the authored in presentation selected by its caller");
        pending.reserve(16);handler=std::make_unique<FrontendHandler>(session,input);
    }
    void Ready()const{CheckOptions(thread==std::this_thread::get_id()&&session&&!failed,"Options requires its live nonfailed owner thread");}
    void Mutable()const{Ready();CheckOptions(!busy&&!nlGetCurrentAsyncRead(),"Options recursive or NL-callback mutation is unsupported");}
    void Expected(const FrontendSession::Handle& frame)const
    {Mutable();CheckOptions(frame&&frame==current&&frame==session->Current(),"Options requires its exact current visible frame");}
    bool CanRoute()const{return state.status.state==1&&state.status.initialized&&!state.status.transition;}
    void Queue(FrontendPointerCallback kind,unsigned item,unsigned index,const FrontendSession::Handle& frame)
    {
        if(kind!=FrontendPointerCallback::Enter&&kind!=FrontendPointerCallback::Leave&&kind!=FrontendPointerCallback::Press)return;
        CheckOptions(pending.size()<16,"Options pointer callback budget exceeded");pending.push_back({kind,item,index,frame});
    }
    void Play(const std::vector<std::uint32_t>& cues)
    {
        const auto live=audio->Handles();std::erase_if(sounds,[&](auto h){return std::find(live.begin(),live.end(),h)==live.end();});
        sounds.reserve(sounds.size()+cues.size());for(auto cue:cues)if(auto handle=audio->Play(cue,seed))sounds.push_back(*handle);
    }
    void CancelSounds()
    {
        std::exception_ptr error;const auto live=audio->Handles();
        for(auto handle:sounds)if(std::find(live.begin(),live.end(),handle)!=live.end())try{audio->Cancel(handle);}catch(...){if(!error)error=std::current_exception();}
        sounds.clear();if(error)std::rethrow_exception(error);
    }
};
FrontendOptions::FrontendOptions(std::shared_ptr<FrontendSession> session,FrontendInput& input,std::shared_ptr<FrontendAudio> audio,unsigned& seed,unsigned controller)
    :impl_(std::make_unique<Implementation>(std::move(session),input,std::move(audio),seed,controller))
{
    auto& s=*impl_;OptionsState next;
    s.session->HandlerTransaction(s.current,[&](auto& playback){
        Step step(playback,{});FrontendOptionsCreated<Step::Instance,Step::Finder>(step);
        step.Command(FrontendOptionsCommandKind::HideNavigation);step.Command(FrontendOptionsCommandKind::BindNavigationBack,4);
        for(unsigned i=0;i<4;++i)step.Command(FrontendOptionsCommandKind::PointerWaiting,i);
        step.Command(FrontendOptionsCommandKind::SelectMusic,1);next=step.Result();
    });
    s.state=std::move(next);s.current=s.session->Current();
}
FrontendOptions::~FrontendOptions(){try{Release();}catch(...){std::terminate();}}
FrontendSession::Handle FrontendOptions::Current()const{impl_->Ready();return impl_->current;}
FrontendOptionsStatus FrontendOptions::Status()const
{const auto& s=*impl_;CheckOptions(s.thread==std::this_thread::get_id()&&s.session,"Options status requires its live owner thread");auto out=s.state.status;out.failed=s.failed;return out;}
std::array<FrontendPointerBounds,3> FrontendOptions::Bounds()const
{
    auto& s=*impl_;s.Ready();CheckOptions(s.state.status.initialized,"Options bounds wait for its authored intro");std::array<FrontendPointerBounds,3> out;
    for(unsigned i=0;i<3;++i){CheckOptions(bool(s.regions[i]),"Options bounds require presentation acknowledgement");out[i]=s.regions[i]->Bounds();}return out;
}
void FrontendOptions::Acknowledge(const FrontendSession::Handle& frame,FrontendPointerViewport viewport)
{
    auto& s=*impl_;s.Expected(frame);
    try
    {
        if(s.state.status.initialized)
        {
            for(unsigned i=0;i<3;++i)
            {
                if(s.regions[i])s.regions[i]->RebindFrame(frame);
                else s.regions[i]=std::make_shared<FrontendPointerRegion>(s.input,frame,s.state.bindings[i],[&s,i](auto kind,unsigned index,const auto& source){s.Queue(kind,i,index,source);});
                if(s.state.disabled[i]&&s.regions[i]->Enabled())s.regions[i]->Disable();
            }
            s.host.Publish(frame,viewport,s.regions);
        }
        else s.host.Publish(frame,viewport,{});
    }
    catch(...){s.failed=true;throw;}
}
void FrontendOptions::ApplyPending()
{
    auto& s=*impl_;s.Expected(s.current);if(s.pending.empty())return;const auto frame=s.current;Busy guard(s.busy);
    try
    {
        auto events=std::move(s.pending);s.pending={};s.pending.reserve(16);OptionsState next;std::vector<std::uint32_t> cues;
        s.session->HandlerTransaction(frame,[&](auto& playback){
            Step step(playback,s.state);step.value.status.commands.clear();
            const auto play=[&](unsigned long cue,const void* name,void* context,bool restartable){CheckOptions(cue<=UINT32_MAX&&!name&&!context&&restartable,"Options audio request exceeds supported profile");cues.push_back(std::uint32_t(cue));};
            for(const auto& event:events)
            {
                CheckOptions(event.frame==frame,"Options callback received a stale presentation");if(step.mState!=1)break;
                if(event.kind==FrontendPointerCallback::Enter)FrontendOptionsEnter(step,event.index,event.item,play);
                else if(event.kind==FrontendPointerCallback::Leave)FrontendOptionsLeave(step,event.index,event.item);
                else FrontendOptionsPress(step,int(event.item),[&](int i){step.Command(FrontendOptionsCommandKind::PointerWaiting,unsigned(i));},[&]{step.Command(FrontendOptionsCommandKind::HideNavigation);},play);
            }
            next=step.Result();
        },[&]{s.Play(cues);});
        s.state=std::move(next);s.current=s.session->Current();
        for(unsigned i=0;i<3;++i)if(s.state.disabled[i]&&s.regions[i]->Enabled())s.regions[i]->Disable();
    }
    catch(...){s.failed=true;throw;}
}
void FrontendOptions::DeliverPointer(const FrontendSession::Handle& frame,const FrontendPointerEvent& event)
{
    auto& s=*impl_;s.Expected(frame);CheckOptions(s.host.Current()&&s.host.Current()->Frame()==frame,"Options input requires acknowledged presentation");
    CheckOptions(event.index<4,"Options pointer exceeds four");if(!s.CanRoute())return;
    try{for(const auto& region:s.regions)region->Deliver(frame,event);ApplyPending();}catch(...){s.failed=true;throw;}
}
FrontendPointerDispatch FrontendOptions::Route(const FrontendPointerDesktopSample& sample)
{
    auto& s=*impl_;s.Expected(s.current);CheckOptions(s.host.Current()&&s.host.Current()->Frame()==s.current,"Options input requires acknowledged presentation");if(!s.CanRoute())return {};
    try{auto out=s.host.Route(s.host.Current(),sample);ApplyPending();return out;}catch(...){s.failed=true;throw;}
}
FrontendPointerDispatch FrontendOptions::Poll(SDL_Window* window,bool capture)
{
    auto& s=*impl_;s.Expected(s.current);CheckOptions(s.host.Current()&&s.host.Current()->Frame()==s.current,"Options input requires acknowledged presentation");if(!s.CanRoute())return {};
    try{auto out=s.host.Poll(s.host.Current(),window,capture);ApplyPending();return out;}catch(...){s.failed=true;throw;}
}
void FrontendOptions::AdvanceVisual(const FrontendSession::Handle& frame,float delta)
{
    auto& s=*impl_;s.Expected(frame);CheckOptions(s.pending.empty(),"Options callbacks must finish before its visual update");if(s.state.status.transition)return;
    try
    {
        s.handler->Update(frame,delta);s.current=s.session->Current();OptionsState next;std::vector<std::uint32_t> cues;
        s.session->HandlerTransaction(s.current,[&](auto& playback){
            Step step(playback,s.state);step.value.status.commands.clear();
            const auto play=[&](unsigned long cue,const void* name,void* context,bool restartable){CheckOptions(cue<=UINT32_MAX&&!name&&!context&&restartable,"Options audio request exceeds supported profile");cues.push_back(std::uint32_t(cue));};
            const bool input=FrontendOptionsGate(step,[&](int i){step.Command(FrontendOptionsCommandKind::PointerWaiting,unsigned(i));},
                [&]{step.Command(FrontendOptionsCommandKind::ShowNavigationBack,4);},
                [&](int scene){CheckOptions(scene==14||scene==15||scene==23,"Options scene request is unsupported");step.Command(FrontendOptionsCommandKind::PushScene,unsigned(scene));step.value.status.transition=FrontendOptionsTransition{FrontendOptionsCommandKind::PushScene,scene,frame};},play,
                [&]{step.Command(FrontendOptionsCommandKind::TransitionOptionsToMainMenu);step.value.status.transition=FrontendOptionsTransition{FrontendOptionsCommandKind::TransitionOptionsToMainMenu,-1,frame};},
                [&]{step.Command(FrontendOptionsCommandKind::PopScene);});
            if(input)
            {
                if(!step.value.status.initialized){for(int i=0;i<3;++i)FrontendOptionsBind<Step::Instance,Position,Step::Finder>(step,i);step.value.status.initialized=true;}
                for(unsigned i=0;i<4;++i)step.Command(i==s.controller?FrontendOptionsCommandKind::PointerCursor:FrontendOptionsCommandKind::PointerWaiting,i);
            }
            next=step.Result();
        },[&]{s.Play(cues);});
        s.state=std::move(next);s.current=s.session->Current();
    }
    catch(...){s.failed=true;throw;}
}
void FrontendOptions::NotifyBackButton(const FrontendSession::Handle& frame)
{
    auto& s=*impl_;s.Expected(frame);CheckOptions(s.CanRoute()&&s.host.Current()&&s.host.Current()->Frame()==frame,"Options back completion requires current interactive presentation");Busy guard(s.busy);
    try
    {
        OptionsState next;s.session->HandlerTransaction(frame,[&](auto& playback){Step step(playback,s.state);step.value.status.commands.clear();FrontendOptionsBack(step,[&]{step.Command(FrontendOptionsCommandKind::HideNavigation);});next=step.Result();});
        s.state=std::move(next);s.current=s.session->Current();
    }
    catch(...){s.failed=true;throw;}
}
void FrontendOptions::Release()
{
    auto& s=*impl_;CheckOptions(s.thread==std::this_thread::get_id()&&!s.busy&&!nlGetCurrentAsyncRead(),"Options release requires idle owner thread");if(!s.session)return;
    s.host.Release();s.regions={};s.pending.clear();if(s.handler)s.handler->Release();s.handler.reset();
    s.CancelSounds();s.current.reset();s.state={};s.session.reset();s.audio.reset();
}
}
