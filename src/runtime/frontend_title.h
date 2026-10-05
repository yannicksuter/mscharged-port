#pragma once
#include "runtime/frontend_audio.h"
#include "runtime/frontend_music.h"
#include "runtime/frontend_pointer_host.h"
#include "runtime/frontend_stack_visual.h"
#include <optional>
namespace mscharged
{
enum class FrontendTitleDevice { DesktopWithoutWiiServices };
enum class FrontendTitleCommandKind
{
    Dimming, PointerWaiting, PointerCursor, PointerAccept, PointerEnabled,
    ResetNavigation, PopScene, TransitionTitleToMain, IntroMovie
};
struct FrontendTitleCommand { FrontendTitleCommandKind kind; unsigned argument=0; };
struct FrontendTitleOptions
{
    unsigned controller=0, movement=0;
    bool widescreen=false;
    FrontendTitleDevice device=FrontendTitleDevice::DesktopWithoutWiiServices;
};
struct FrontendTitleStatus
{
    float elapsed=0;
    bool initialized=false, started_demo=false, highlighted=false, failed=false;
    std::array<bool,9> sequence{};
    std::array<int,4> pointer_states{};
    // Ordered requests from the latest source step, not host-service readiness.
    std::vector<FrontendTitleCommand> commands;
    std::optional<FrontendTitleCommandKind> departure;
    FrontendSession::Handle source;
    // This selected desktop profile has no Wii motion/battery/banner provider.
    bool full_scene_created=false, wii_motion_available=false, icons_loaded=false;
};
// Selected original Title visual/input owner. Music0 and FE cues are actual
// retained audio operations. NAV/global pointer/dimming/scene/script commands
// require host admission; commands alone never complete those dependencies.
// Source smoke/soak/gameplay and Wii-device branches are outside this explicit
// desktop profile. Caller services music and owns input/RNG until Release.
class FrontendTitle final : public FrontendStackVisual
{
    struct Implementation;
    std::unique_ptr<Implementation> impl_;
    void ApplyPending();
    void AfterBaseUpdate(FrontendHandler::UpdateProof&&);
    std::shared_ptr<FrontendSession> StackSession() const override;
    std::shared_ptr<FrontendHandler> StackHandler() const override;
    unsigned StackScene() const override;
    void AttachStack() override;
    bool CanUpdateStack() const override;
    void UpdateStack(FrontendHandler::UpdateProof&&,const FrontendSession::Handle&,const std::function<void()>&) override;
    void ReleaseStack() override;
public:
    FrontendTitle(std::shared_ptr<FrontendSession>,FrontendInput&,std::shared_ptr<FrontendAudio>,
        std::shared_ptr<FrontendMusic>,unsigned& seed,FrontendTitleOptions={});
    FrontendTitle(std::shared_ptr<FrontendSession>,FrontendInput&,std::shared_ptr<FrontendAudio>,
        std::shared_ptr<FrontendMusic>,unsigned& seed,std::shared_ptr<FrontendHandler>,FrontendTitleOptions);
    ~FrontendTitle();
    FrontendTitle(const FrontendTitle&)=delete;
    FrontendTitle& operator=(const FrontendTitle&)=delete;
    FrontendSession::Handle Current() const override;
    FrontendTitleStatus Status() const;
    FrontendPointerBounds Bounds() const;
    void Acknowledge(const FrontendSession::Handle&,FrontendPointerViewport);
    void DeliverPointer(const FrontendSession::Handle&,const FrontendPointerEvent&);
    FrontendPointerDispatch Route(const FrontendPointerDesktopSample&);
    FrontendPointerDispatch Poll(SDL_Window*,bool capture=false);
    void AdvanceVisual(const FrontendSession::Handle&,float delta);
    void Release();
};
}
