#pragma once

#include "bootstrap/gamepad_actions.h"

#include <SDL3/SDL_gamepad.h>

#include <array>
#include <string>
#include <string_view>

namespace mscharged::platform {
// A gamepad input: an SDL button (0 .. SDL_GAMEPAD_BUTTON_COUNT - 1), a
// trigger (kGamepadTriggerBase + its SDL axis) or kGamepadNone.
using GamepadInput = int;
inline constexpr GamepadInput kGamepadNone = -1;
inline constexpr GamepadInput kGamepadTriggerBase = 64;
// One or two inputs per gamepad action; kGamepadNone = unused.
using GamepadBinding = std::array<GamepadInput, 2>;
using GamepadBindings = std::array<GamepadBinding, GamepadActionCount>;

// Settings of one gamepad player.
struct GamepadProfile {
    GamepadBindings bindings{};
    // false: the left stick moves the Nunchuk stick and the right stick points;
    // true swaps them.
    bool swap_sticks = false;
};

// SDL names ("a", "leftshoulder", "righttrigger"); unknown names, analog
// stick axes and kGamepadNone give kGamepadNone / an empty string.
GamepadInput ParseGamepadInput(std::string_view name);
std::string GamepadInputName(GamepadInput input);
// "a | start". Unknown names are left out.
GamepadBinding ParseGamepadBinding(std::string_view text);
std::string FormatGamepadBinding(const GamepadBinding& binding);
// An action without a usable input keeps its default.
GamepadBindings ParseGamepadBindings(const std::array<std::string, GamepadActionCount>& texts);
GamepadBindings DefaultGamepadBindings();
// A trigger counts as pressed past half its travel.
inline constexpr Sint16 kGamepadTriggerPressed = 16384;
bool GamepadInputHeld(SDL_Gamepad* pad, GamepadInput input);
bool GamepadBindingHeld(SDL_Gamepad* pad, const GamepadBinding& binding);
} // namespace mscharged::platform
