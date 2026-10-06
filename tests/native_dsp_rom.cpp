#include "platform/dsp_instruction_core.h"
#include "platform/dsp_control_abi.h"
#include "platform/dsp_memory_abi.h"
#include "platform/interrupt_controller.h"
#include "platform/interrupts.h"
#include "dsp_boot_data.h"
#include <aurora/aurora.h>
#include <dolphin/os.h>
#include <SDL3/SDL_loadso.h>
extern "C" {
#include <revolution/dsp.h>
}
#include <array>
#include <cstring>
#include <exception>
#include <filesystem>
#include <iostream>
#include <stdexcept>
#include <string>

namespace {
using namespace mscharged::platform;
unsigned checks;
void Check(bool result,const char* text) {++checks;if(!result)throw std::runtime_error(text);}
template<class F> void Throws(F&& action,const char* text) {
    bool held=false;try{action();}catch(const std::exception&){held=true;}Check(held,text);
}
struct SDKLifetime {bool live{};~SDKLifetime(){if(live)aurora_shutdown();}};
void PutWord(unsigned char* bytes,std::size_t word,std::uint16_t value) {
    bytes[word*2]=static_cast<unsigned char>(value>>8);bytes[word*2+1]=static_cast<unsigned char>(value);
}
void Run(int argc,char** argv) {
    Check(argc==3 && std::strlen(argv[2])==64,"genuine source leaf/hash arguments required");
    const auto directory=std::filesystem::absolute("sdk-data").string();std::filesystem::create_directories(directory);
    SDKLifetime sdk;AuroraConfig config{};config.appName="DSP ROM transport qualifier";
    config.userPath=config.cachePath=directory.c_str();config.resourcesPath=".";
    config.desiredBackend=BACKEND_NULL;config.windowWidth=320;config.windowHeight=240;
    config.windowPosX=config.windowPosY=-1;config.mem1Size=MEM1_DEFAULT_SIZE;
    config.mem2Size=64u*1024u*1024u;config.logLevel=LOG_WARNING;
    Check(aurora_initialize(argc,argv,&config).window!=nullptr,"actual SDK initialization failed");sdk.live=true;OSInit();
    auto* source=SDL_LoadObject(argv[1]);Check(source!=nullptr,"whole-source data leaf failed to load");
    const auto reader=reinterpret_cast<OriginalDSPBootData(*)()>(SDL_LoadFunction(source,"OriginalDSPBootDataForFixture"));
    Check(reader!=nullptr,"actual source static initializer export missing");const auto boot=reader();
    Check(boot.count==128,"actual original DSPInitCode has incorrect extent");
    auto* backing=static_cast<unsigned char*>(OSGetMEM2ArenaLo());
    Check(reinterpret_cast<std::uintptr_t>(backing)%32==0 &&
          static_cast<unsigned char*>(OSGetMEM2ArenaHi())-backing>=16384,
          "genuine MEM2 scratch backing is not aligned/large enough");
    // No Game arena has captured this diagnostic SDK arena. Caller supplies
    // and pins real backing; generated bank cells are never claimed retail ROM.
    for(std::size_t i=0;i<2048;++i) {
        PutWord(backing,i,static_cast<std::uint16_t>(i*0x351du+0x2abcu));
        PutWord(backing+4096,i,static_cast<std::uint16_t>(i*0x187bu+0x6341u));
    }
    std::memcpy(backing+8192,boot.bytes,boot.count);SDL_UnloadObject(source);
    const auto memory=AttachNativeDSPMEM1();
    const auto pin=PinNativeDSPMemory(backing,16384,false);
    const auto physical=ChargedDSPTaskMemoryWord(backing,16384,0);
    Check(physical==0x10000000u,"source MEM2 physical base changed");
    InitializeNativeInterruptController();const auto mailboxes=AttachNativeDSPMailboxes();
    const auto control=AttachNativeDSPControl(mailboxes);
    DSPInstructionCore empty(mailboxes,control);
    Throws([&]{empty.InstructionWord(0x8000);},"missing IROM invented reset-vector bytes");
    Throws([&]{empty.DataWord(0x1000);},"missing coefficient ROM invented data");
    Throws([&]{empty.BeginExecution({0x8000,0,0,{0,0},0});},"absent ROM created a reset context");
    Check(empty.Registers().instructions==0 && !DSPCheckInit(),"missing ROM advanced source/device state");
    DSPInstructionCore rom(mailboxes,control);
    Throws([&]{rom.LoadInstructionROM(memory,physical+16384-32);},"unowned short ROM extent silently loaded");
    Throws([&]{rom.InstructionWord(0x8000);},"failed ROM transfer installed a partial bank");
    rom.LoadInstructionROM(memory,physical);rom.LoadCoefficientROM(memory,physical+4096);
    for (std::uint16_t index:std::array<std::uint16_t,8>{0,1,0x21,0x1ff,0x400,0x511,0x7fe,0x7ff}) {
        const auto instruction=static_cast<std::uint16_t>(index*0x351du+0x2abcu);
        const auto coefficient=static_cast<std::uint16_t>(index*0x187bu+0x6341u);
        Check(rom.InstructionWord(static_cast<std::uint16_t>(0x8000+index))==instruction,"IROM endian/word offset differs");
        Check(rom.InstructionWord(static_cast<std::uint16_t>(0x8800+index))==instruction,"IROM mirror address differs");
        Check(rom.DataWord(static_cast<std::uint16_t>(0x1000+index))==coefficient,"coefficient endian/offset differs");
        Check(rom.DataWord(static_cast<std::uint16_t>(0x1800+index))==coefficient,"coefficient mirror differs");
    }
    std::memset(backing,0,8192);
    Check(rom.InstructionWord(0x8000)==0x2abc && rom.DataWord(0x1000)==0x6341,
          "immutable ROM banks still alias writable source backing");
    Throws([&]{rom.LoadInstructionROM(memory,physical);},"IROM backing was replaced after installation");
    Throws([&]{rom.LoadCoefficientROM(memory,physical);},"coefficient ROM was replaced after installation");
    Throws([&]{rom.InstructionWord(0x9000);},"unmapped instruction bank invented ROM alias");
    Throws([&]{rom.DataWord(0x2000);},"unmapped data bank invented ROM alias");
    Throws([&]{rom.LoadInstructionMemory(memory,physical,32,0x8000);},"IRAM DMA overwrote a ROM bank");
    Throws([&]{rom.LoadDataMemory(memory,physical,32,0x1000);},"DRAM DMA overwrote a ROM bank");
    PutWord(backing,0,0x029f);PutWord(backing,1,0x0010);
    DSPInstructionCore cross(mailboxes,control);
    cross.LoadInstructionROM(memory,physical);
    cross.LoadInstructionMemory(memory,physical+8192,128,0);
    cross.BeginExecution({0x8800,0xffff,0,{0,0},0});ChargedDSPControlWrite(0);
    auto crossed=cross.Step();
    Check(crossed.pc==0x10 && crossed.instructions==1 && crossed.status==0xffff,
          "generated mirrored ROM instruction did not jump to supplied IRAM");
    crossed=cross.Step();
    Check(crossed.pc==0x11 && crossed.instructions==2 && crossed.status==0xefff,
          "ROM-to-IRAM transfer did not execute the actual source next instruction");
    Throws([&]{cross.LoadCoefficientROM(memory,physical+4096);},"ROM transport changed an executing bank context");
    // Original source bytes, explicitly begun at IRAM0 for instruction/data
    // qualification only. No reset, ROM or OSAudioSystem flow is substituted.
    DSPInstructionCore init(mailboxes,control);
    init.LoadInstructionMemory(memory,physical+8192,128,0);
    Check(init.InstructionWord(0)==0x029f && init.InstructionWord(1)==0x0010 &&
          init.InstructionWord(0x14)==0x0080 && init.InstructionWord(0x15)==0x8000,
          "independent actual source initialization-vector oracle differs");
    init.BeginExecution({0,0xffff,0,{0,0},0});ChargedDSPControlWrite(0);
    auto state=init.Step();Check(state.pc==0x10 && state.status==0xffff && state.instructions==1,
                                 "actual original vector JMP changed SR or target/count");
    for(unsigned i=0;i<4;++i)init.Step();
    Check(init.Registers().pc==0x14 && init.Registers().status==0xe1ff && init.Registers().instructions==5,
          "literal source status-clear prefix arithmetic/order differs");
    bool unsupported=false;try{init.Step();}catch(const DSPUnsupportedInstruction& gap) {
        unsupported=gap.pc==0x14 && gap.opcode==0x0080 && gap.operand==0;
    }
    Check(unsupported && init.Registers().pc==0x14 && init.Registers().instructions==5,
          "actual missing AR0 register/ISA was bypassed");
    // Generated JMP oracle reaches absent IROM and stops on its actual next
    // fetch. It never synthesizes bootstrap mail, a source flag or callbacks.
    PutWord(backing,0,0x029f);PutWord(backing,1,0x8000);
    DSPInstructionCore jump(mailboxes,control);jump.LoadInstructionMemory(memory,physical,32,0);
    jump.BeginExecution({0,0x5432,7,{0x123456789aULL,0x00abcdef01ULL},0});state=jump.Step();
    Check(state.pc==0x8000 && state.status==0x5432 && state.control==7 &&
          state.accumulator[0]==0x123456789aULL && state.accumulator[1]==0xabcdef01ULL && state.instructions==1,
          "generated immediate jump altered unrelated real register cells");
    Throws([&]{jump.Step();},"jump into absent ROM fabricated an instruction");
    Check(jump.Registers().pc==0x8000 && jump.Registers().instructions==1 && !DSPCheckInit(),
          "absent ROM fabricated execution/source init");
    Throws([&]{DSPInit();},"actual DSPInit fabricated bootstrap readiness after ROM transport");
    Check(!DSPCheckInit() && !__DSP_curr_task && !NativeInterruptsEnabled(),
          "actual interrupted source init state/task differed");
    // Explicit fixture recovery of the rejected source request, not production
    // original-game cleanup or a repaired source/CRT shutdown claim.
    OSRestoreInterrupts(TRUE);
    DetachNativeDSPControl();DetachNativeDSPMailboxes();ShutdownNativeInterruptController();
    ReleaseNativeDSPMemory(pin);DetachNativeDSPMEM1();
    std::cout<<"original_dsp_init_prefix: actual128-byte source program runs JMP +4 status clears; holds at AR0; ROM/reset/task boot remains unavailable\n";
}
} // namespace
int main(int argc,char** argv) {
    try{Run(argc,argv);std::cout<<"native_dsp_rom: "<<checks<<" checks passed\n";return 0;}
    catch(const std::exception& failure){std::cerr<<"native_dsp_rom: "<<failure.what()<<"\n";return 1;}
}
