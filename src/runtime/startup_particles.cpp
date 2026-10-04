#include "runtime/startup.h"
#include "runtime/particle_files.h"
#include "runtime/effects_registry.h"
#include <chrono>
#include <thread>

namespace mscharged
{
std::string VerifyStartupParticleResources()
{
    ParticleFileLoad load;
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(10);
    while (load.State() == ParticleFileState::Loading)
    {
        load.Service();
        if (std::chrono::steady_clock::now() > deadline)
            throw std::runtime_error("Particle resource diagnostic timed out");
        if (load.State() == ParticleFileState::Loading)
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    const auto files = load.Result();
    const auto registry = EffectsRegistry::FromFiles(files);
    return "Independent particle resource diagnostic: 4 files, " + std::to_string(registry->Templates())
        + " templates, " + std::to_string(registry->Groups()) + " authored groups; "
        + std::to_string(registry->RegisteredGroups()) + " groups resolved through original code, "
        + std::to_string(registry->UnavailableGroups().size()) + " groups require user-effect factories; "
        + std::to_string(registry->Textures()) + " unique textures retained; "
        + std::to_string((*files)[ParticleFileKind::Geometry].size())
        + " geometry bytes retained. Geometry/GL registration, particle simulation and rendering remain unavailable.";
}
}
