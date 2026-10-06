#include <aurora/aurora.h>
#include <aurora/event.h>
#include <aurora/gfx.h>
#include <aurora/gfx.hpp>
#include <dolphin/gx/GXAurora.h>
#include <atomic>
#include <vector>
#include <memory>
#include <chrono>
#include <thread>
#include <aurora/video.h>
#include <aurora/dvd.h>
#include <dolphin/os.h>
#include "platform/stm_device.h"
#include "platform/system.h"
#include "platform/video_device.h"
#include "platform/interrupt_controller.h"
#include <dlfcn.h>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <stdexcept>
#include <filesystem>
#include <string>
namespace aurora { extern AuroraConfig g_config; }
extern "C" bool __OSInitSTM();
namespace {
void Check(bool value, const char* reason) { if (!value) throw std::runtime_error(reason); }
struct Snapshot : std::enable_shared_from_this<Snapshot> {
    aurora::gfx::ResolvedTargets target;
    wgpu::Buffer buffer;
    unsigned stride;std::atomic_int done;
    std::vector<unsigned char> rgb;
    static void Copy(const aurora::gfx::EncoderTaskContext& ctx,const wgpu::CommandEncoder& cmd,const void*,std::size_t,void* p) {
        auto& self=*static_cast<Snapshot*>(p);
        const wgpu::BufferDescriptor d{.usage=wgpu::BufferUsage::MapRead|wgpu::BufferUsage::CopyDst,.size=std::uint64_t(self.stride)*self.target.height};
        self.buffer=ctx.device.CreateBuffer(&d);
        const wgpu::TexelCopyTextureInfo src{.texture=self.target.colorTexture};
        const wgpu::TexelCopyBufferInfo dst{.layout={.offset=0,.bytesPerRow=self.stride,.rowsPerImage=self.target.height},.buffer=self.buffer};
        const wgpu::Extent3D extent{self.target.width,self.target.height,1};
        cmd.CopyTextureToBuffer(&src,&dst,&extent);
    }
    static void Map(const aurora::gfx::EncoderTaskCompletionContext&,const void*,std::size_t,void* p) {
        auto self=static_cast<Snapshot*>(p)->shared_from_this();
        const auto n=std::uint64_t(self->stride)*self->target.height;
        self->buffer.MapAsync(wgpu::MapMode::Read,0,n,wgpu::CallbackMode::AllowSpontaneous,[self,n](wgpu::MapAsyncStatus status,wgpu::StringView) {
            if(status!=wgpu::MapAsyncStatus::Success){self->done=-1;return;}
            auto* raw=static_cast<const unsigned char*>(self->buffer.GetConstMappedRange(0,n));
            const bool bgra=self->target.colorFormat==wgpu::TextureFormat::BGRA8Unorm;
            for(unsigned y=0;y<self->target.height;++y)for(unsigned x=0;x<self->target.width;++x) {
                auto* out=self->rgb.data()+3*(std::size_t(y)*self->target.width+x);
                const auto* in=raw+std::size_t(y)*self->stride+4*x;
                out[0]=in[bgra?2:0];out[1]=in[1];out[2]=in[bgra?0:2];
            }
            self->buffer.Unmap();self->done=1;
        });
    }
};
void EndWithSnapshot(const std::filesystem::path& output) {
    auto s=std::make_shared<Snapshot>();
    Check(aurora::gfx::resolve_pass({},s->target),"Cannot resolve actual original Credits EFB");
    Check(s->target.colorFormat==wgpu::TextureFormat::BGRA8Unorm || s->target.colorFormat==wgpu::TextureFormat::RGBA8Unorm,"Unsupported snapshot color format");
    s->stride=(s->target.width*4+255)&~255u;
    s->rgb.resize(std::size_t(s->target.width)*s->target.height*3);
    const aurora::gfx::EncoderTaskDescriptor d{"Original Credits diagnostic snapshot",Snapshot::Copy,s.get(),Snapshot::Map};
    const auto task=aurora::gfx::register_encoder_task_type(d);
    const bool queued=aurora::gfx::push_encoder_task(task,nullptr,0);
    aurora_end_frame();aurora::gfx::synchronize();aurora::gfx::unregister_encoder_task_type(task);
    Check(queued,"Actual EFB snapshot task rejected");
    const auto end=std::chrono::steady_clock::now()+std::chrono::seconds(5);
    while(!s->done && std::chrono::steady_clock::now()<end){aurora::gfx::device().Tick();std::this_thread::sleep_for(std::chrono::milliseconds(1));}
    Check(s->done==1,"Original Credits GPU readback did not complete");
    auto* f=std::fopen(output.c_str(),"wb");Check(f,"Cannot create Credits snapshot");
    std::fprintf(f,"P6\n%u %u\n255\n",s->target.width,s->target.height);
    const auto written=std::fwrite(s->rgb.data(),1,s->rgb.size(),f);const auto closed=std::fclose(f);
    Check(written==s->rgb.size() && !closed,"Credits snapshot write failed");
    unsigned lit=0;for(std::size_t i=0;i<s->rgb.size();i+=3)if(s->rgb[i]||s->rgb[i+1]||s->rgb[i+2])++lit;
    Check(lit != 0, "Original Credits actual EFB is entirely black; visibility remains unqualified");
    std::printf("Actual Credits EFB %ux%u, nonblack pixels%u; saved %s.\n",s->target.width,s->target.height,lit,output.c_str());
}
}
int main(int argc, char** argv) {
    try {
        bool interactive=false;
        std::filesystem::path disc;
        for (int i=1;i<argc;++i) {
            const std::string argument=argv[i];
            if (argument=="--help") {
                std::puts("Usage: mscharged-original-main-credits-check --disc FILE [--window]\n"
                          "Enter original main, then load and update the original Credits scene.\n"
                          "Explicit USA/English system configuration. Full tasks, movie/audio,\n"
                          "physical input, source VI swap and game shutdown remain omitted.\n"
                          "--window keeps the source scene running until you close the window.");
                return 0;
            }
            if(argument=="--window")interactive=true;
            else if(argument=="--disc" && i+1<argc)disc=argv[++i];
            else throw std::runtime_error("Unknown or incomplete argument; use --help");
        }
        Check(!disc.empty() && std::filesystem::is_regular_file(disc),"Supply your own game ISO/RVZ with --disc FILE");
        const auto executable=std::filesystem::absolute(std::filesystem::path(argv[0]));
        const auto modulePath=executable.parent_path()/MSCHARGED_ORIGINAL_MAIN_CREDITS_MODULE_FILENAME;
        Check(std::filesystem::is_regular_file(modulePath),"Original-main source module is missing beside the executable");
        // Real host window/device/FIFO owner exists before any original module
        // constructors. Original main still owns its GXInit/glStartup decisions.
        const auto dataDirectory = std::filesystem::absolute(
            executable.parent_path() / "original-main-credits-data");
        std::filesystem::create_directories(dataDirectory);
        const auto dataPath = dataDirectory.string();
        AuroraConfig config{};
        config.appName="Mario Strikers Charged | original-main Credits diagnostic";
        config.userPath=config.cachePath=dataPath.c_str();
        config.desiredBackend=BACKEND_VULKAN;
        config.windowWidth=800;config.windowHeight=600;
        config.windowPosX=config.windowPosY=-1;
        config.mem1Size=MEM1_DEFAULT_SIZE;
        config.mem2Size=64u*1024u*1024u;
        config.logLevel=LOG_INFO;config.enableBackendValidation=true;
        config.vsync=true;
        const auto host=aurora_initialize(argc,argv,&config);
        if(!host.window||host.backend!=BACKEND_VULKAN)
            throw std::runtime_error("Actual Vulkan foundation unavailable; no fallback acceptance");
        // Selected scene diagnostic keeps explicit native begin/end ownership.
        // Continuous aurora_configure_native_gx_hardware is a separate mode;
        // arming it here would reject mixed diagnostic frame ownership.
        OSInit();
        if(!OSGetArenaLo()||!OSGetMEM2ArenaLo())
            throw std::runtime_error("Actual captured SDK arenas unavailable");
        mscharged::platform::InitializeNativeInterruptController();
        // Explicit USA diagnostic backing; independent SC and VI settings.
        mscharged::ConfigureNativeSystemSettings({1,0,0,0});
        mscharged::platform::ConfigureNativeVideoHardware(VI_TVMODE_NTSC_INT,false);
        mscharged::platform::InitializeNativeSTMDevice();
        if(!__OSInitSTM())throw std::runtime_error("Actual original STM initialization failed");
        if(!aurora_dvd_open(disc.c_str())) throw std::runtime_error("Actual owned Wii data partition failed");
        auto* module=dlopen(modulePath.c_str(),RTLD_LAZY|RTLD_LOCAL);
        if(!module)throw std::runtime_error(dlerror());
        auto entry=reinterpret_cast<int(*)()>(dlsym(module,"charged_original_entry"));
        if(!entry)throw std::runtime_error("Original source main export unavailable");
        std::fprintf(stderr,"Entering actual source main with real Aurora Vulkan/FIFO owner and native PI_VI under temporary MAIN_BOOTSTRAP; flow remains incomplete.\n");
        std::fflush(nullptr);
        const int result=entry();
        Check(result==85, "Original main selected scene did not complete checkpoint85");
        auto frame=reinterpret_cast<void(*)(float)>(dlsym(module,"charged_original_scene_frame"));
        Check(frame,"Same original-main module scene-frame export unavailable");
        // Existing actual source GX/state/material/font/view initialization is
        // retained. No host GXInit, source pool restart or fixture font setup.
        AuroraSetViewportPolicy(AURORA_VIEWPORT_FIT);
        unsigned frames=0,draws=0,quiet=0; bool snapshot=false,encoded=false,exit=false;
        const auto start=std::chrono::steady_clock::now(); auto previous=start;
        while(!exit && (interactive || !snapshot)) {
            for(const auto* event=aurora_update();event->type!=AURORA_NONE;++event)
                if(event->type==AURORA_EXIT)exit=true;
            if(exit)break;
            const auto now=std::chrono::steady_clock::now();
            Check(interactive || now-start<std::chrono::seconds(40),"Original main Credits Vulkan pipeline timed out");
            if(!aurora_begin_frame()){std::this_thread::yield();continue;}
            const auto receipt=AuroraGXBeginDrawReceipt();Check(receipt,"Real source frame draw receipt unavailable");
            const float delta=std::chrono::duration<float>(now-previous).count(); previous=now;
            frame(delta); AuroraGXEndDrawReceipt();
            const bool sample=!snapshot && now-start>=std::chrono::seconds(3) && quiet>=2;
            if(sample){EndWithSnapshot(dataDirectory/"original-main-credits.ppm");snapshot=true;}
            else aurora_end_frame();
            aurora::gfx::synchronize();
            encoded=encoded || AuroraGXWasDrawEncoded(receipt);
            draws+=aurora_get_stats()->drawCallCount;++frames;
            quiet=std::atomic_ref<const unsigned>(aurora_get_stats()->queuedPipelines).load()?0:quiet+1;
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
        Check(snapshot && encoded && draws && aurora_get_last_presentation().sequence,
              "Original main Credits real source draw/presentation incomplete");
        std::printf("Original main→selected Credits live: %u actual source frames,%u encoded draws; one initialized game/SDK, native owner elapsed time. Explicit task/VI swap/movie/audio/physical-input/full-CRT omissions remain.\n",frames,draws);
        std::fflush(nullptr);std::_Exit(0);
    }catch(const std::exception& e){std::fprintf(stderr,"Actual source diagnostic stopped: %s\n",e.what());std::fflush(nullptr);std::_Exit(1);}
}
