#include "platform/gamepad_bindings.h"

namespace mscharged::platform {
GamepadInput ParseGamepadInput(std::string_view name) {
    const std::string text(name);
    const SDL_GamepadButton button = SDL_GetGamepadButtonFromString(text.c_str());
    if (button != SDL_GAMEPAD_BUTTON_INVALID) return GamepadInput(button);
    const SDL_GamepadAxis axis = SDL_GetGamepadAxisFromString(text.c_str());
    if (axis == SDL_GAMEPAD_AXIS_LEFT_TRIGGER || axis == SDL_GAMEPAD_AXIS_RIGHT_TRIGGER)
        return kGamepadTriggerBase + GamepadInput(axis);
    return kGamepadNone;
}

std::string GamepadInputName(GamepadInput input) {
    const char* name = nullptr;
    if (input >= 0 && input < SDL_GAMEPAD_BUTTON_COUNT)
        name = SDL_GetGamepadStringForButton(SDL_GamepadButton(input));
    else if (input == kGamepadTriggerBase + SDL_GAMEPAD_AXIS_LEFT_TRIGGER ||
             input == kGamepadTriggerBase + SDL_GAMEPAD_AXIS_RIGHT_TRIGGER)
        name = SDL_GetGamepadStringForAxis(SDL_GamepadAxis(input - kGamepadTriggerBase));
    return name ? name : "";
}

GamepadBinding ParseGamepadBinding(std::string_view text) {
    GamepadBinding binding{kGamepadNone, kGamepadNone};
    std::size_t used = 0;
    while (!text.empty() && used < binding.size()) {
        const auto bar = text.find('|');
        auto name = text.substr(0, bar);
        text = bar == std::string_view::npos ? std::string_view{} : text.substr(bar + 1);
        while (!name.empty() && name.front() == ' ') name.remove_prefix(1);
        while (!name.empty() && name.back() == ' ') name.remove_suffix(1);
        if (name.empty()) continue;
        const GamepadInput input = ParseGamepadInput(name);
        if (input != kGamepadNone && (used == 0 || binding[0] != input)) binding[used++] = input;
    }
    return binding;
}

std::string FormatGamepadBinding(const GamepadBinding& binding) {
    std::string text;
    for (const GamepadInput input : binding) {
        const auto name = GamepadInputName(input);
        if (name.empty()) continue;
        if (!text.empty()) text += " | ";
        text += name;
    }
    return text;
}

GamepadBindings ParseGamepadBindings(const std::array<std::string, GamepadActionCount>& texts) {
    GamepadBindings bindings{};
    for (std::size_t n = 0; n < GamepadActionCount; ++n) {
        bindings[n] = ParseGamepadBinding(texts[n]);
        if (bindings[n][0] == kGamepadNone) bindings[n] = ParseGamepadBinding(kGamepadActions[n].defaults);
    }
    return bindings;
}

GamepadBindings DefaultGamepadBindings() {
    GamepadBindings bindings{};
    for (std::size_t n = 0; n < GamepadActionCount; ++n)
        bindings[n] = ParseGamepadBinding(kGamepadActions[n].defaults);
    return bindings;
}

bool GamepadInputHeld(SDL_Gamepad* pad, GamepadInput input) {
    if (input >= 0 && input < SDL_GAMEPAD_BUTTON_COUNT) return SDL_GetGamepadButton(pad, SDL_GamepadButton(input));
    if (input >= kGamepadTriggerBase && input < kGamepadTriggerBase + SDL_GAMEPAD_AXIS_COUNT)
        return SDL_GetGamepadAxis(pad, SDL_GamepadAxis(input - kGamepadTriggerBase)) > kGamepadTriggerPressed;
    return false;
}

bool GamepadBindingHeld(SDL_Gamepad* pad, const GamepadBinding& binding) {
    return GamepadInputHeld(pad, binding[0]) || GamepadInputHeld(pad, binding[1]);
}
} // namespace mscharged::platform
