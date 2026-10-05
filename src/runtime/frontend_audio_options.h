#pragma once
#include "runtime/frontend_audio.h"
#include "runtime/audio_volume.h"
#include "runtime/frontend_pointer_host.h"
namespace mscharged
{
enum class FrontendAudioOptionsCommandKind { HideNavigation,BindBack,BindDone,PointerWaiting,PointerCursor,ShowBackAndDone,DoneText,PushOptions,HoverRumble };
struct FrontendAudioOptionsCommand {FrontendAudioOptionsCommandKind kind;unsigned argument=0;};
struct FrontendAudioOptionsStatus
{
    int state=0;
    bool initialized=false,failed=false;
    std::array<int,3> settings{},backup{};
    std::array<std::array<int,4>,6> pointer_states{};
    std::vector<FrontendAudioOptionsCommand> commands;
};
// Original scene14's mode0 visuals/volume listeners over a retained OPTIONS_IN
// session. Requires explicit actual category authority shared with resident
// output; caller also binds music to it and advances its single frame clock.
// This is not full SceneCreated, GameInfo, SaveLoad, NAV or overlay readiness.
// Real Save is unavailable and fails before changing the visible scene.
class FrontendAudioOptions
{
    struct Implementation;std::unique_ptr<Implementation> impl_;
    void ApplyPending();
public:
    FrontendAudioOptions(std::shared_ptr<FrontendSession>,FrontendInput&,
        std::shared_ptr<FrontendAudio>,AudioCategoryVolumes::Handle,unsigned& caller_seed,unsigned controller=0);
    ~FrontendAudioOptions();
    FrontendAudioOptions(const FrontendAudioOptions&)=delete;
    FrontendAudioOptions& operator=(const FrontendAudioOptions&)=delete;
    FrontendSession::Handle Current()const;
    FrontendAudioOptionsStatus Status()const;
    void AdvanceVisual(const FrontendSession::Handle&,float delta);
    void Acknowledge(const FrontendSession::Handle&,FrontendPointerViewport);
    std::array<FrontendPointerBounds,6> Bounds()const;
    void DeliverPointer(const FrontendSession::Handle&,const FrontendPointerEvent&);
    FrontendPointerDispatch Route(const FrontendPointerDesktopSample&);
    FrontendPointerDispatch Poll(SDL_Window*,bool capture=false);
    void NotifyBackButton(const FrontendSession::Handle&);
    void Save(const FrontendSession::Handle&); // Explicit missing SaveLoad boundary.
    void Release();
};
}
