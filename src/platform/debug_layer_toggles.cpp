// MSCHARGED_DEBUG_LAYERS_OFF=DepthOfField,Warble,... switches the game's own
// rendering-layer tweaks (eCLV_<name>Enabled, /Rendering/RLView Toggles) off
// before the original main starts, to find which layer renders differently on
// a platform. Unset, it changes nothing. Diagnostic only.
#if !defined(MSCHARGED_NATIVE) || !defined(MSCHARGED_GAME_MODULE)
#error Layer debug toggles belong to the isolated original game module
#endif
#include "Game/Render/RLViewLayers.h"
#include "Game/TweakValue.h"

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>

namespace {
struct LayerToggle {
    const char* name;
    TweakValueBool* tweak;
};
#define LAYER(name) {#name, &eCLV_##name##Enabled}
const LayerToggle kLayers[] = {
    LAYER(ImpostorTexture), LAYER(ShadowTexture), LAYER(GrabTexture), LAYER(PictureInPicture),
    LAYER(PictureInPictureAlpha), LAYER(NoFog), LAYER(Shadowed), LAYER(WorldShadowed), LAYER(Unshadowed),
    LAYER(MegastrikeBackground), LAYER(ImpostorOut), LAYER(Characters), LAYER(PeachPhoto3D), LAYER(MoreCharacters),
    LAYER(WorldAlphaBlended), LAYER(HighRange3D), LAYER(HighRange3DNoFog), LAYER(HighRangeChain), LAYER(HighRange2D),
    LAYER(BigBlackPolygon), LAYER(ShadowVolume), LAYER(ShadowVolumeBlend), LAYER(UnsortedPerspective),
    LAYER(DepthOfField), LAYER(LingeringParticles), LAYER(Particles), LAYER(BallChargeAlphaBlended), LAYER(CoPlanar),
    LAYER(InvisiblePlane), LAYER(ElectricFence), LAYER(PreWarble), LAYER(Warble), LAYER(WarbleBlend),
    LAYER(CameraSpace), LAYER(ScreenBlur), LAYER(ScreenBlur2), LAYER(ScreenGrab), LAYER(FrontEnd),
    LAYER(UnsortedOrtho640), LAYER(UnsortedSquareOrtho),
};
#undef LAYER
} // namespace

extern "C" __attribute__((visibility("default"))) void charged_apply_debug_layer_toggles() {
    const char* list = std::getenv("MSCHARGED_DEBUG_LAYERS_OFF");
    if (!list || !*list) return;
    std::string names(list);
    std::size_t start = 0;
    while (start <= names.size()) {
        const auto end = std::min(names.find(',', start), names.size());
        const auto name = names.substr(start, end - start);
        start = end + 1;
        if (name.empty()) continue;
        bool found = false;
        for (const auto& layer : kLayers)
            if (name == layer.name) {
                layer.tweak->ParseValue("false");
                found = true;
            }
        std::fprintf(stderr, found ? "Debug: rendering layer %s off\n" : "Debug: unknown rendering layer %s\n",
                     name.c_str());
    }
}
