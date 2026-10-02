// Independent synthetic pixel expectations for the original material TEV recipes.
#include "runtime/materials.h"
#include "runtime/gpu_readback.h"
#include "runtime/material_environment.h"
#include "runtime/static_inventory.h"
#include "runtime/graphics_memory.h"
#include "runtime/graphics_state.h"
#include "runtime/startup.h"
#include "runtime/startup_files.h"
#include "Game/GraphicsMemoryStartup.h"
#include "NL/gl/glMemoryInit.h"
#include "NL/gl/glMatrix.h"
#include "NL/gl/glState.h"
#include "NL/glx/GXMaskedSpecularFresnelMaterialProgram.h"
#include "NL/nlMemory.h"
#include "NL/MemAlloc.h"
#include <aurora/aurora.h>
#include <aurora/event.h>
#include <aurora/gfx.h>
#include <dolphin/gx.h>
#include <dolphin/gx/GXAurora.h>
#include <dolphin/mtx.h>
#include <dolphin/vi.h>
#include <SDL3/SDL.h>
#include <imgui.h>
#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <filesystem>
#include <iostream>
#include <stdexcept>

using namespace mscharged;
namespace
{
std::atomic_uint errors = 0;
void Log(AuroraLogLevel level, const char *, const char *message, unsigned length)
{
    if (level >= LOG_ERROR)
        ++errors;
    std::cerr.write(message, length);
    std::cerr << '\n';
}
void Check(bool value, const char *message)
{
    if (!value)
        throw std::runtime_error(message);
}
void Drain()
{
    AuroraGXSync();
}
void Invalidate()
{
    GXInvalidateVtxCache();
    GXInvalidateTexAll();
}
struct Session
{
    bool live = false, gx = false;
    ~Session()
    {
        if (!live)
            return;
        if (gx)
            Drain();
        glShutdownMemory();
        ResetStartupFiles();
        ResetStartupMemory();
        aurora_shutdown();
    }
};
resources::Texture Texture(unsigned id, std::array<unsigned char, 4> c, unsigned alpha_bits = 0, bool stripe = false)
{
    resources::Texture t;
    t.id = id;
    t.width = t.height = 4;
    t.levels = 1;
    t.game_format = 3;
    t.gx_format = 6;
    t.bits = {8, 8, 8, static_cast<unsigned char>(alpha_bits)};
    t.pixels.resize(64);
    for (unsigned i = 0; i < 16; ++i)
    {
        const auto pixel = stripe && i % 4 >= 2 ? std::array<unsigned char, 4>{20, 40, 200, 255} : c;
        t.pixels[i * 2] = pixel[3];
        t.pixels[i * 2 + 1] = pixel[0];
        t.pixels[i * 2 + 32] = pixel[1];
        t.pixels[i * 2 + 33] = pixel[2];
    }
    return t;
}
resources::StaticModel Model(unsigned id, unsigned program, unsigned texture)
{
    resources::Packet p;
    p.primitive = 0;
    p.material.program = program;
    p.material.textures[0] = {texture, 3};
    p.raster = 0xC0007;
    p.vertices = {
        {{-.8f, -.8f, -.3f}, {.125f, .125f}}, {{.8f, -.8f, -.3f}, {.125f, .125f}}, {{0, .8f, -.3f}, {.125f, .125f}}};
    for (auto &v : p.vertices)
    {
        v.normal = {0, 0, 1};
        v.uv1 = v.uv2 = v.uv;
    }
    p.indices = {0, 1, 2};
    return {id, {p}};
}
void PixelCase(const char *name, glModel &model, float time, std::array<unsigned char, 3> expected)
{
    unsigned draws = 0;
    std::array<unsigned char, 3> pixel{};
    nlMatrix4 identity;
    identity.SetIdentity();
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(15);
    for (unsigned frame = 0; frame < 40;)
    {
        Check(std::chrono::steady_clock::now() < deadline, "Material pixel readback deadline exceeded");
        for (const auto *event = aurora_update(); event->type != AURORA_NONE; ++event)
            Check(event->type != AURORA_EXIT, "Material test window closed");
        if (!aurora_begin_frame())
        {
            SDL_Delay(1);
            continue;
        }
        glplatFrameAllocNextFrame();
        glModelSetMatrix(&model, identity);
        Mtx44 projection;
        C_MTXOrtho(projection, 1, -1, -1, 1, 0, 1);
        Mtx matrix = {{1, 0, 0, 0}, {0, 1, 0, 0}, {0, 0, 1, 0}};
        GXSetCopyClear({20, 24, 30, 255}, GX_MAX_Z24);
        GXSetViewport(0, 0, GXNtsc480IntDf.fbWidth, GXNtsc480IntDf.efbHeight, 0, 1);
        GXSetScissor(0, 0, GXNtsc480IntDf.fbWidth, GXNtsc480IntDf.efbHeight);
        GXSetProjection(projection, GX_ORTHOGRAPHIC);
        GXLoadPosMtxImm(matrix, GX_PNMTX0);
        GXSetCurrentMtx(GX_PNMTX0);
        {
            MaterialPreviewScope environment(identity, time);
            for (unsigned i = 0; i < model.numPackets; ++i)
                DrawMaterial(model.packets[i]);
        }
        GXDrawDone();
        if (frame == 39)
        {
            const auto samples = EndFrameAndReadColours();
            std::copy_n(samples[4].begin(), 3, pixel.begin());
        }
        else
            aurora_end_frame();
        draws += aurora_get_stats()->drawCallCount;
        ++frame;
    }
    const bool match = std::abs(int(pixel[0]) - expected[0]) <= 3 && std::abs(int(pixel[1]) - expected[1]) <= 3 &&
                       std::abs(int(pixel[2]) - expected[2]) <= 3;
    std::cout << name << ": RGB " << unsigned(pixel[0]) << ',' << unsigned(pixel[1]) << ',' << unsigned(pixel[2])
              << ", draws " << draws << '\n';
    Check(match && draws > 0, "Original material shader pixel mismatch");
}
} // namespace
int main(int argc, char **argv)
{
    try
    {
        const auto directory = std::filesystem::path(SDL_GetBasePath()) / "material-test-data";
        std::filesystem::create_directories(directory);
        const auto path = directory.string();
        AuroraConfig cfg{};
        cfg.appName = "Charged material pixel checks";
        cfg.userPath = cfg.cachePath = path.c_str();
        cfg.resourcesPath = SDL_GetBasePath();
        cfg.desiredBackend = BACKEND_VULKAN;
        cfg.enableBackendValidation = true;
        cfg.windowWidth = 640;
        cfg.windowHeight = 480;
        cfg.windowPosX = cfg.windowPosY = -1;
        cfg.vsync = true;
        cfg.logLevel = LOG_WARNING;
        cfg.logCallback = Log;
        cfg.mem1Size = MEM1_DEFAULT_SIZE;
        cfg.mem2Size = 64 * 1024 * 1024;
        Session session;
        const auto info = aurora_initialize(argc, argv, &cfg);
        session.live = true;
        Check(info.backend == BACKEND_VULKAN && info.window, "Material tests require real Vulkan");
        ImGui::GetIO().IniFilename = nullptr;
        ImGui::GetIO().LogFilename = nullptr;
        InitializeStartupOS();
        nlInitMemory();
        const auto free1 = StandardAllocator.TotalFreeMemory(), free2 = VirtualAllocator.TotalFreeMemory();
        glInitResourcePools();
        InitializeOriginalGraphicsMemory();
        InitializeOriginalGraphicsState();
        VIInit();
        VIConfigure(&GXNtsc480IntDf);
        alignas(32) std::array<unsigned char, 65536> fifo{};
        GXInit(fifo.data(), fifo.size());
        session.gx = true;
        AuroraSetViewportPolicy(AURORA_VIEWPORT_FIT);
        SetGraphicsCacheInvalidator(Invalidate);
        MaterialPrograms programs;
        auto unlit = Model(1, 0x21db4385, 10), vertex = Model(2, 0xd3e572da, 10), scroll = Model(3, 0x2169db5c, 11),
             masked = Model(4, 0x32475c7d, 10);
        for (auto &v : vertex.packets[0].vertices)
            v.colour = {128, 255, 128, 255};
        scroll.packets[0].material.scalars[0] = .5f;
        auto &m = masked.packets[0].material;
        m.textures[1] = {12, 3};
        m.textures[2] = {13, 3};
        m.scalars = {.5f, 1, 1, 1};
        auto normal_left = masked, normal_right = masked;
        normal_left.id = 8;
        normal_right.id = 9;
        normal_left.packets[0].material.textures[1].texture = normal_right.packets[0].material.textures[1].texture = 11;
        for (auto &v : normal_left.packets[0].vertices)
            v.normal = {-.75f, 0, std::sqrt(7.f) / 4};
        for (auto &v : normal_right.packets[0].vertices)
            v.normal = {.75f, 0, std::sqrt(7.f) / 4};
        auto discard = Model(5, 0x21db4385, 14), blend = Model(6, 0x21db4385, 15);
        auto palette = Texture(16, {0, 0, 0, 255});
        palette.game_format = 8;
        palette.gx_format = 9;
        palette.bits = {5, 5, 5, 0};
        palette.palette_entries = 2;
        palette.palette = {0xfc, 0, 0x80, 0x1f};
        palette.pixels.assign(32, 0);
        auto ci8 = Model(7, 0x21db4385, 16);
        std::vector<resources::Texture> textures = {Texture(10, {80, 100, 120, 255}),
                                                    Texture(11, {200, 40, 20, 255}, 0, true),
                                                    Texture(12, {200, 100, 40, 255}),
                                                    Texture(13, {128, 128, 128, 255}),
                                                    Texture(glGetTexture("global/fresnel1"), {128, 128, 128, 255}),
                                                    Texture(14, {200, 100, 40, 0}, 1),
                                                    Texture(15, {80, 100, 120, 128}, 8),
                                                    palette};
        StaticInventory inventory(*glGetCurrentResourcePool(),
                                  {unlit, vertex, scroll, masked, discard, blend, ci8, normal_left, normal_right},
                                  textures, Drain);
        PixelCase("Unlit diffuse", *inventory.Model(1), 0, {80, 100, 120});
        PixelCase("Vertex colour modulation", *inventory.Model(2), 0, {40, 100, 60});
        PixelCase("Scrolling t=0", *inventory.Model(3), 0, {200, 40, 20});
        PixelCase("Scrolling t=1", *inventory.Model(3), 1, {20, 40, 200});
        PixelCase("Specular mask and Fresnel", *inventory.Model(4), 0, {105, 113, 125});
        PixelCase("Normal lookup left", *inventory.Model(8), 0, {105, 105, 123});
        PixelCase("Normal lookup right", *inventory.Model(9), 0, {83, 105, 145});
        static_cast<GXMaskedSpecularFresnelParameters *>(inventory.Model(4)->packets[0].materialParameters)
            ->specularAmount = 0;
        PixelCase("Specular disabled", *inventory.Model(4), 0, {80, 100, 120});
        PixelCase("Alpha discard", *inventory.Model(5), 0, {20, 24, 30});
        PixelCase("Alpha blend", *inventory.Model(6), 0, {50, 62, 75});
        PixelCase("Big-endian CI8 palette", *inventory.Model(7), 0, {255, 0, 0});
        // Switch back to verify TEV/channel/texture state does not leak between programs.
        PixelCase("Unlit after multi-stage materials", *inventory.Model(1), 0, {80, 100, 120});
        inventory.Release();
        programs.Release();
        glShutdownMemory();
        Check(StandardAllocator.TotalFreeMemory() == free1 && VirtualAllocator.TotalFreeMemory() == free2,
              "Material shutdown leaked original arenas");
        Check(errors == 0, "Material GPU backend errors");
        std::cout << "Original material pipeline pixel and lifetime checks passed\n";
        return 0;
    }
    catch (const std::exception &error)
    {
        std::cerr << "FAILED: " << error.what() << '\n';
        return 1;
    }
}
