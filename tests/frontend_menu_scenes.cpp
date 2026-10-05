#include "runtime/frontend_menu_scenes.h"
#include "runtime/frontend_camera_assets.h"
#include "runtime/frontend_options.h"
#include "runtime/frontend_stack.h"
#include "runtime/startup.h"
#include "runtime/startup_files.h"
#include "audio_bank_fixture.h"
#include "NL/nlFileGC.h"
#include "NL/MemAlloc.h"
#include <aurora/aurora.h>
#include <aurora/dvd.h>
#include <SDL3/SDL.h>
#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <new>
#include <source_location>
#include <thread>
thread_local long allocation_budget=-1;
void* operator new(std::size_t n){if(allocation_budget==0)throw std::bad_alloc();if(allocation_budget>0)--allocation_budget;if(auto p=std::malloc(n?n:1))return p;throw std::bad_alloc();}
void* operator new[](std::size_t n){return ::operator new(n);}
void operator delete(void*p)noexcept{std::free(p);}void operator delete[](void*p)noexcept{std::free(p);}
void operator delete(void*p,std::size_t)noexcept{std::free(p);}void operator delete[](void*p,std::size_t)noexcept{std::free(p);}
using namespace mscharged;using namespace mscharged::resources;
namespace
{
unsigned checks=0;
void Check(bool value,const char* message){++checks;if(!value)throw std::runtime_error(message);}
template<class F>void Reject(F f,std::source_location at=std::source_location::current()){++checks;try{f();}catch(const std::exception&){return;}throw std::runtime_error("Invalid Options/navigation operation accepted at "+std::to_string(at.line()));}
void Near(float a,float b,std::source_location at=std::source_location::current()){++checks;if(!std::isfinite(a)||std::abs(a-b)>=.0001f)throw std::runtime_error("Independent clock differs at "+std::to_string(at.line())+": "+std::to_string(a)+" vs "+std::to_string(b));}
std::vector<std::uint8_t> Load(const char* path)
{std::unique_ptr<nlFile> file(nlOpen(path));Check(bool(file),"Missing NAV input");const auto n=nlFileSize(file.get(),nullptr);std::vector<std::uint8_t> out(n);nlRead(file.get(),out.data(),n,n);return out;}
void Pump(FrontendSession& session)
{const auto end=std::chrono::steady_clock::now()+std::chrono::seconds(15);while(session.State()==FrontendSessionState::Loading){session.Service();Check(std::chrono::steady_clock::now()<end,"NAV assets timed out");SDL_Delay(1);}session.Result();}
auto Session(const char* path="/Art/fe/main_menu_v3.fen")
{auto owner=std::make_shared<FrontendSession>();owner->Begin({path,FrontendLanguage::English,FrontendImageProfile::Main,"MAIN",true});Pump(*owner);return owner;}
using namespace audio_bank_fixture;
std::shared_ptr<FrontendAudio> Audio(bool owned,unsigned device=0)
{
 if(owned)
 {
  auto global=Load("/audio/nlxgs.bun");auto catalog=ReadAudioBankCatalog(global);AudioBankLoad owner(catalog,23,21);
  const auto end=std::chrono::steady_clock::now()+std::chrono::seconds(10);while(owner.State()==AudioBankLoadState::Loading){owner.Service();Check(std::chrono::steady_clock::now()<end,"NAV sound bank timed out");SDL_Delay(1);}
  return std::make_shared<FrontendAudio>(owner.Result(),ReadAudioCalculationInitial(global),AudioVoicesOptions{32,64*1024*1024,device});
 }
 auto f=Make();const auto word=[](const Data& d,std::size_t at){return (std::uint32_t(d.at(at))<<24)|(std::uint32_t(d.at(at+1))<<16)|(std::uint32_t(d.at(at+2))<<8)|d.at(at+3);};
 const auto chunks=[&](const Data& data){std::vector<std::pair<unsigned,Data>> out;for(std::size_t at=0;at<data.size();){const auto id=word(data,at),n=word(data,at+4);out.emplace_back(id,Data(data.begin()+at+8,data.begin()+at+8+n));at+=(n+11)&~3u;}return out;};
 const std::array keys{0x6b0689d4u,0x0a93e9a0u,0xf0afd586u,0x304fdd1eu,0xf6eb899eu,0x4430b152u,0xaccdca48u,0x6f6a3a07u,0xb19dbc20u};Data root;
 for(auto [id,data]:chunks(Data(f.bytes.begin()+8,f.bytes.end())))
 {
  if(id==0x80023000)
  {
   Data map,records;Append(map,0x23001,Words({unsigned(keys.size()),0,0}));for(unsigned i=0;i<keys.size();++i){auto b=Words({keys[i],0,0,0,i});records.insert(records.end(),b.begin(),b.end());}Append(map,0x23003,records);data=std::move(map);
  }
  if(id==0x80023300)
  {
   Data graph;bool refs=false;
   for(auto [kind,part]:chunks(data))
   {
    if(kind==0x23301)Put(part,8,unsigned(keys.size()));
    if(kind==0x23302){Data rows;for(auto key:keys){auto row=Data(part.begin()+40,part.end());Put(row,0,key);rows.insert(rows.end(),row.begin(),row.end());}part=std::move(rows);}
    if(kind==0x23308){if(!refs){for(unsigned i=0;i<keys.size();++i)Append(graph,kind,Words({0xf0001000,0,Float(255),0,0}));refs=true;}continue;}
    Append(graph,kind,part);
   }
   data=std::move(graph);
  }
  Append(root,id,data);
 }
 auto bank=std::make_shared<const LoadedAudioBank>(LoadedAudioBank{23,21,{23,"FE_GEN_Sfx"},{21,0,1,false},ReadAudioResidentBank(Wrap(0x80000001,root),f.wave)});
 Data calc;Append(calc,0x23401,Words({2,0xf1000100,0}));Append(calc,0x23402,Words({0,0,0,0,0,0,1,0,0,0xf1000100,0,0}));
 return std::make_shared<FrontendAudio>(bank,ReadAudioCalculationInitial(Wrap(0x80000001,Wrap(0x80023400,calc))),AudioVoicesOptions{32,64*1024*1024,device});
}
void Input(FrontendInput& input)
{std::array<FrontendPadSample,4> pads{};for(auto& p:pads)p.connected=true;input.Update(pads,1.f/60);}
FrontendPointerViewport Viewport(){return {1,960,720,1920,1440,0,0,1920,1440};}
std::array<float,2> Center(FrontendPointerBounds b){return {(b.min_x+b.max_x)/2,(b.min_y+b.max_y)/2};}
FrontendStackRequest Request(unsigned scene)
{FrontendStackRequest r;r.scene=scene;r.initial_slide=scene==1?"MAIN":"in";return r;}
void Pump(FrontendSceneStack& stack,FrontendSceneStack::Token token)
{
 const auto end=std::chrono::steady_clock::now()+std::chrono::seconds(15);
 while(stack.Entry(token).state==FrontendStackState::Queued||stack.Entry(token).state==FrontendStackState::Loading)
 {stack.Service();Check(std::chrono::steady_clock::now()<end,"Visual stack loading timed out");SDL_Delay(1);}
 stack.RethrowFailure(token);
}

void Catalog(CameraAssetLibrary& library)
{
 auto batch=LoadFrontendCameraAssets(library);const auto end=std::chrono::steady_clock::now()+std::chrono::seconds(15);
 while(batch.State()==CameraBatchState::Loading){batch.Service();Check(std::chrono::steady_clock::now()<end,"Camera assets timed out");SDL_Delay(1);}
 Check(batch.State()==CameraBatchState::Ready&&batch.Progress().succeeded==37,"Source camera catalog incomplete");batch.Publish();
}

void Flow(bool owned,const std::vector<std::uint8_t>& script,CameraAssetLibrary& library,const std::filesystem::path& folder)
{
 FrontendInput input;unsigned seed=0x194fe921;auto audio=Audio(owned);unsigned drains=0,effects=0,music=0;
 OriginalCameras core;FrontendCameras cameras(core,library);cameras.Push("startidle");
 auto preferences=std::make_shared<NativePreferences>(std::filesystem::absolute(folder/"native-preferences"));
 preferences->StartLoad();while(preferences->Status().host_pending){preferences->Poll();SDL_Delay(1);}preferences->RethrowFailure();
 FrontendMenuScenes menus(input,audio,seed,cameras,script,preferences,
     [&](unsigned index){Check(index==1,"Source requested another music profile");++music;},
     [&](unsigned kind){Check(kind==95,"Source requested another stadium effect");++effects;return true;},[&]{++drains;});
 Reject([&]{menus.DeliverPointer({});});Reject([&]{menus.Update(-1);});
 const auto deadline=std::chrono::steady_clock::now()+std::chrono::seconds(15);
 while(!menus.Current().menu||!menus.Current().navigation){menus.Service();Check(std::chrono::steady_clock::now()<deadline,"Initial menus timed out");SDL_Delay(1);}
 auto shown=menus.Current();menus.Acknowledge(shown,Viewport());const auto images=shown.menu->images;const auto visuals=shown.menu->visuals;
 const auto step=[&](const std::function<void()>& action={}){
  Input(input);menus.Update(1.f/60,action);shown=menus.Current();
  if(shown.navigation){Check(!shown.menu||(shown.menu->images==images&&shown.menu->visuals==visuals),"Transition replaced permanent resources");menus.Acknowledge(shown,Viewport());}
 };
 const auto until=[&](unsigned scene){
  for(unsigned i=0;i<2400&&!(menus.Status().scene==scene&&menus.Status().interactive);++i)step();
  Check(menus.Status().scene==scene&&menus.Status().interactive,"Source menu did not become interactive");
 };
 until(1);Check(menus.Bounds().size()==7&&!menus.Status().full_scene_created,"Main selected scope differs");
 const auto main_token=shown.token;
 step([&]{menus.DeliverPointer({0,Center(menus.Bounds()[6]),true});});
 Check(!shown.menu&&!shown.token&&!menus.Status().full_scene_created,"Departing Main remained published");
 // Actual source camera end, NAV completion and bytecode select Options.
 until(13);Check(shown.token!=main_token&&shown.menu->image_completed_files==0,"Options reread shared image bundles");
 Check(effects==1&&music>=1&&cameras.ActiveAlias()=="fechoosecaptains","Original departure hosts/camera differ");
 const auto options_token=shown.token;
 step([&]{menus.DeliverPointer({0,Center(menus.BackBounds()),true});});
 Check(menus.Status().state==3&&!menus.Status().interactive,"Real NAV Back did not start source Options outro");
 until(1);Check(shown.token!=main_token&&shown.token!=options_token&&shown.menu->image_completed_files==0,"Returning Main reused old handler or reread resources");
 Check(cameras.ActiveAlias()=="startidle"&&effects==1&&music>=2,"Original return camera/music hosts differ");
 // A second complete loop verifies retained callback identities and fresh handlers.
 step([&]{menus.DeliverPointer({0,Center(menus.Bounds()[6]),true});});until(13);
 step([&]{menus.DeliverPointer({0,Center(menus.BackBounds()),true});});until(1);
 Check(effects==2&&!preferences->Status().original_normal_save_loaded&&!preferences->Status().full_save_complete,"Selected menu manufactured original save readiness");
 const auto retained=shown;menus.Release();Reject([&]{menus.Update(0);});
 Check(retained.menu->images==images&&retained.navigation->visuals==visuals&&drains>8,"Menu teardown lost retained data or missed drains");
}
struct Host {bool live=false,disc=false;~Host(){if(live)ResetStartupFiles();if(disc)aurora_dvd_close();if(live){ResetStartupMemory();aurora_shutdown();}}};
}
int main(int argc,char** argv)
{
 try{
  Check(argc==4,"Supply disc/output/generated or owned");const bool owned=std::string_view(argv[3])=="owned";const auto folder=(std::filesystem::path(argv[2])/"menu-scenes-host").string();std::filesystem::create_directories(folder);AuroraConfig config{};
  config.appName="Charged original menu flow";config.userPath=config.cachePath=folder.c_str();config.resourcesPath=SDL_GetBasePath();config.desiredBackend=BACKEND_NULL;config.windowWidth=320;config.windowHeight=240;config.windowPosX=config.windowPosY=-1;config.mem1Size=MEM1_DEFAULT_SIZE;config.mem2Size=64*1024*1024;config.logLevel=LOG_WARNING;
  Host host;const auto h=aurora_initialize(argc,argv,&config);host.live=true;Check(h.window,"Aurora failed");InitializeStartupOS();nlInitMemory();Check(aurora_dvd_open(argv[1]),"Disc missing");host.disc=true;nlInitFileSystem();
  const auto before1=StandardAllocator.TotalFreeMemory(),before2=VirtualAllocator.TotalFreeMemory();CameraAssetLibrary library;Catalog(library);const auto script=Load("/Art/scripts/fe_presentation.byte_code");
  Flow(owned,script,library,folder);library.Clear();Check(!nlAsyncReadsPending(nullptr)&&before1==StandardAllocator.TotalFreeMemory()&&before2==VirtualAllocator.TotalFreeMemory(),"Menu flow did not restore arenas/files");
  std::cout<<checks<<" original Main/Options menu lifecycle checks passed; full SceneCreated remains unavailable\n";
 }catch(const std::exception&e){allocation_budget=-1;std::cerr<<"FAILED: "<<e.what()<<" (check "<<checks<<")\n";return 1;}
}
