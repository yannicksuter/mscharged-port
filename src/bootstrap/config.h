#pragma once

#include <array>
#include <filesystem>
#include <string>
#include <set>

namespace mscharged
{
struct Settings
{
    std::string disc;
    std::string language = "auto";
    int width = 1280;
    int height = 720;
    bool fullscreen = false;
    bool vsync = true;
    std::string backend = "auto";
    std::string aspect = "16:9";
    // display.picture: soft (the TV signal, as on a Wii), clean (one smooth
    // pass to the window, no TV flicker filter) or sharp (square pixels).
    std::string picture = "clean";
    // display.antialiasing: off or 4x (Aurora multisampling of the game's
    // 640x448 frame; smooths polygon edges, not a higher resolution).
    std::string antialiasing = "off";
    int master_volume = 100;
    int music_volume = 100;
    int effects_volume = 100;
    bool mute = false;
    // controls.player1-4: keyboard, remote1-remote4 (Wii Remote in that
    // DolphinBar slot) or off. Players fill in order; each device plays once.
    std::array<std::string, 4> players{"keyboard", "off", "off", "off"};
    // controls.remote1-4_calibration: pointer calibration of the Wii Remote
    // in that DolphinBar slot (six numbers from the launcher) or none.
    std::array<std::string, 4> remote_calibration{"none", "none", "none", "none"};
    int deadzone = 15;
    bool rumble = true;
    std::string sensor_bar = "bottom"; // controls.sensor_bar: bottom | top (sensor bar / DolphinBar position)
    // Host presentation and diagnostics (never game behaviour).
    bool show_fps = true;             // display.show_fps: frame rate in the game window title
    int monitor = 0;                  // display.monitor: 0 = default display, N = Nth connected display
    bool graphics_validation = true;  // advanced.graphics_validation: backend graphics debug layers
    std::string log_level = "info";   // advanced.log_level: error | warning | info | debug
    std::string ui_scale = "auto";    // launcher.ui_scale: auto or a percentage (75 to 200)
};

struct ConfigFile
{
    std::filesystem::path path;
    std::string contents;
    bool exists = false;
    Settings settings;
    std::set<std::string> configured_keys; // Parsed INI provenance; run overrides never enter this set.
};

ConfigFile LoadConfig(const std::filesystem::path& path, bool allow_missing = false);
void SaveConfig(ConfigFile& file, const Settings& settings);
std::filesystem::path ResolveDiscPath(const Settings& settings, const std::filesystem::path& config);
std::filesystem::path LoadDiscPath(const std::filesystem::path& config);
}
