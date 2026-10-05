#pragma once
#include "runtime/frontend_menu_departure.h"
#include "runtime/frontend_options_navigation.h"
#include "runtime/native_preferences.h"

namespace mscharged
{
struct FrontendMenuScenesFrame
{
    FrontendSceneStack::Token token = 0;
    unsigned scene = 0;
    FrontendSession::Handle menu, navigation;
};
struct FrontendMenuScenesStatus
{
    unsigned scene = 0;
    int state = 0;
    bool loading = true, interactive = false, failed = false;
    // Selected visual scope never completes the original SceneCreated/state6.
    bool full_scene_created = false;
    std::optional<unsigned> pending_scene;
    std::optional<FrontendMainSelection> main_selection;
    FrontendMenuTransitionStatus transition;
};
// Actual Main/Options source actions, queue ownership and presentation barrier.
// Input/NL, cameras, music/audio and preferences outlive this owner. The supplied
// music and effect callbacks must genuinely admit their services; no default
// success provider is installed. Preferences are explicitly113's native scope.
// Scene14/15/23 requests remain pending until their concrete glue is supplied.
class FrontendMenuScenes
{
    struct Implementation;
    std::unique_ptr<Implementation> impl_;
public:
    FrontendMenuScenes(FrontendInput&, std::shared_ptr<FrontendAudio>, unsigned& seed,
        FrontendCameras&, resources::Bytes presentation_script,
        std::shared_ptr<NativePreferences>, std::function<void(unsigned)> select_music,
        std::function<bool(unsigned)> stadium_effect, std::function<void()> drain,
        unsigned initial_scene = 1, FrontendLanguage = FrontendLanguage::English);
    ~FrontendMenuScenes();
    FrontendMenuScenes(const FrontendMenuScenes&) = delete;
    FrontendMenuScenes& operator=(const FrontendMenuScenes&) = delete;
    void Service();
    // Advance cameras/NAV once, then the selected stack base/source gate once.
    // The callback runs only inside that screen's controlled old-frame input
    // window. It may call Route/Poll/DeliverPointer and query this owner.
    void Update(float delta, const std::function<void()>& input = {});
    FrontendPointerDispatch Route(const FrontendPointerDesktopSample&);
    FrontendPointerDispatch Poll(SDL_Window*, bool capture = false);
    void DeliverPointer(const FrontendPointerEvent&); // Explicit same-event host route.
    FrontendMenuScenesFrame Current() const;
    void Acknowledge(const FrontendMenuScenesFrame&, FrontendPointerViewport);
    FrontendMenuScenesStatus Status() const;
    std::vector<FrontendPointerBounds> Bounds() const;
    FrontendPointerBounds BackBounds() const;
    void Release();
};
}
