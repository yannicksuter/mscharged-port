#include "platform/dsp_instruction_core.h"

namespace mscharged::platform {
DSPUnsupportedInstruction::DSPUnsupportedInstruction(std::uint16_t address,std::uint16_t word,std::uint16_t detail)
    :std::runtime_error("DSP instruction core reached an unimplemented instruction/register/interface"),
     pc(address),opcode(word),operand(detail) {}
DSPInstructionCore::DSPInstructionCore(NativeDSPMailboxEndpoint mailboxes):mailboxes_(mailboxes) {}
DSPInstructionCore::DSPInstructionCore(NativeDSPMailboxEndpoint mailboxes,NativeDSPControlEndpoint control)
    :mailboxes_(mailboxes),control_(control) {}
void DSPInstructionCore::Load(NativeDSPMemoryEndpoint endpoint,std::uint32_t physical,std::uint32_t bytes,
                             std::uint16_t word_address,bool instruction) {
    if (!bytes || (bytes&1) || word_address>=WORDS || bytes/2>WORDS-word_address)
        throw std::out_of_range("DSP memory transfer is outside its exact 16-bit word bank");
    std::array<std::uint8_t,WORDS*2> raw;
    DSPBackendReadMemory(endpoint,physical,raw.data(),bytes);
    auto& words=instruction?instructions_:data_;
    auto& valid=instruction?instruction_valid_:data_valid_;
    for(std::size_t i=0;i<bytes/2;++i) {
        // Original source byte storage is transported in bus order. Host scalar
        // endian/alignment never changes the firmware/data word it encodes.
        words[word_address+i]=static_cast<std::uint16_t>((static_cast<std::uint16_t>(raw[i*2])<<8)|raw[i*2+1]);
        valid[word_address+i]=true;
    }
}
void DSPInstructionCore::LoadInstructionMemory(NativeDSPMemoryEndpoint endpoint,std::uint32_t physical,
                                               std::uint32_t bytes,std::uint16_t address) {
    Load(endpoint,physical,bytes,address,true);
}
void DSPInstructionCore::LoadDataMemory(NativeDSPMemoryEndpoint endpoint,std::uint32_t physical,
                                        std::uint32_t bytes,std::uint16_t address) {
    Load(endpoint,physical,bytes,address,false);
}
void DSPInstructionCore::LoadROM(NativeDSPMemoryEndpoint endpoint,std::uint32_t physical,bool instruction) {
    auto& loaded=instruction?instruction_rom_loaded_:coefficient_rom_loaded_;
    if (loaded || running_)
        throw std::logic_error("DSP ROM transport requires an unloaded non-executing bank");
    std::array<std::uint8_t,IROM_WORDS*2> raw;
    const auto count=instruction?IROM_WORDS:COEFFICIENT_WORDS;
    DSPBackendReadMemory(endpoint,physical,raw.data(),count*2);
    auto* words=instruction?instruction_rom_.data():coefficient_rom_.data();
    for (std::size_t i=0;i<count;++i)
        words[i]=static_cast<std::uint16_t>((static_cast<std::uint16_t>(raw[i*2])<<8)|raw[i*2+1]);
    loaded=true;
}
void DSPInstructionCore::LoadInstructionROM(NativeDSPMemoryEndpoint endpoint,std::uint32_t physical) {
    LoadROM(endpoint,physical,true);
}
void DSPInstructionCore::LoadCoefficientROM(NativeDSPMemoryEndpoint endpoint,std::uint32_t physical) {
    LoadROM(endpoint,physical,false);
}
std::uint16_t DSPInstructionCore::InstructionWord(std::uint16_t address) const {
    if ((address>>12)==8 && instruction_rom_loaded_)
        return instruction_rom_[address&(IROM_WORDS-1)];
    if(address>=WORDS || !instruction_valid_[address])
        throw std::out_of_range("DSP instruction memory/ROM word is not supplied");
    return instructions_[address];
}
std::uint16_t DSPInstructionCore::DataWord(std::uint16_t address) const {
    if ((address>>12)==1 && coefficient_rom_loaded_)
        return coefficient_rom_[address&(COEFFICIENT_WORDS-1)];
    if(address>=WORDS || !data_valid_[address])
        throw std::out_of_range("DSP data/ROM word is not supplied or written");
    return data_[address];
}
void DSPInstructionCore::BeginExecution(const DSPInstructionRegisters& context) {
    InstructionWord(context.pc);
    if(context.accumulator[0]>0xffffffffffULL || context.accumulator[1]>0xffffffffffULL)
        throw std::out_of_range("DSP register context exceeds a raw 40-bit accumulator");
    registers_=context;running_=true;
}
DSPInstructionRegisters DSPInstructionCore::Registers() const {return registers_;}
DSPInstructionRegisters DSPInstructionCore::Step() {
    if(!running_)throw std::logic_error("DSP execution has no supplied register context");
    if(control_)DSPBackendRequireInstructionExecution(*control_,registers_.status);
    const auto pc=registers_.pc;
    const auto opcode=InstructionWord(pc);
    auto next=registers_;
    std::uint16_t length=1;
    bool branch=false;
    if (opcode==0x029f) {
        // Unconditional immediate jump used by the original OS DSP init
        // vector. Invalid destinations remain real next-fetch failures.
        next.pc=InstructionWord(static_cast<std::uint16_t>(pc+1));
        branch=true;
    } else if((opcode&0xfff8)==0x1200 || (opcode&0xfff8)==0x1300) {
        const auto bit=static_cast<std::uint16_t>(std::uint16_t(1)<<((opcode&7)+6));
        if((opcode&0xff00)==0x1200)next.status&=static_cast<std::uint16_t>(~bit);
        else next.status|=bit;
    } else if(opcode>=0x8a00 && opcode<=0x8f00 && !(opcode&0xff)) {
        // Only the observed non-extended encoding is implemented. Extended
        // writes cannot be ignored just because the main operation is known.
        const auto selected=static_cast<unsigned>((opcode-0x8a00)/0x100);
        constexpr std::array<std::uint16_t,3> masks{0x2000,0x8000,0x4000};
        const auto mask=masks[selected/2];
        if(selected&1)next.status|=mask;
        else next.status&=static_cast<std::uint16_t>(~mask);
    } else if((opcode&0xffe0)==0x0080) {
        length=2;
        const auto immediate=InstructionWord(static_cast<std::uint16_t>(pc+1));
        const auto reg=static_cast<std::uint16_t>(opcode&31);
        if(reg<4)next.address[reg]=immediate;
        else if(reg<8)next.index[reg-4]=immediate;
        else if(reg<12)next.wrap[reg-8]=immediate;
        else if(reg==0x12)next.control=static_cast<std::uint8_t>(immediate);
        else if((reg==0x1e || reg==0x1f) && !(next.status&0x4000)) {
            auto& cell=next.accumulator[reg-0x1e];
            cell=(cell&0xff0000ffffULL)|(static_cast<std::uint64_t>(immediate)<<16);
        } else throw DSPUnsupportedInstruction(pc,opcode,reg);
    } else if(opcode==0x00fe || opcode==0x00ff) {
        length=2;
        const auto address=InstructionWord(static_cast<std::uint16_t>(pc+1));
        if(address>=WORDS || (next.status&0x4000))throw DSPUnsupportedInstruction(pc,opcode,address);
        data_[address]=static_cast<std::uint16_t>(next.accumulator[opcode-0x00fe]>>16);
        data_valid_[address]=true;
    } else if(opcode==0x8100 || opcode==0x8900) {
        next.accumulator[(opcode>>11)&1]=0;
        // Clearing a 40-bit accumulator leaves logic-zero/sticky overflow and
        // mode bits intact, sets arithmetic-zero/top-two-equal, clears C/O/S/S32.
        next.status=static_cast<std::uint16_t>((next.status&0xffc0)|0x24);
    } else if((opcode&0xff00)==0x1600) {
        length=2;
        const auto address=static_cast<std::uint16_t>(0xff00|(opcode&0xff));
        const auto immediate=InstructionWord(static_cast<std::uint16_t>(pc+1));
        if(address==0xfffc)DSPBackendMailFromWriteHigh(mailboxes_,immediate);
        else if(address==0xfffd)DSPBackendMailFromWriteLow(mailboxes_,immediate);
        else if(address==0xfffb && control_)DSPBackendWriteInterruptRequest(*control_,immediate);
        else throw DSPUnsupportedInstruction(pc,opcode,address);
    } else throw DSPUnsupportedInstruction(pc,opcode,0);
    if (!branch)next.pc=static_cast<std::uint16_t>(pc+length);
    ++next.instructions;
    registers_=next;return registers_;
}
} // namespace mscharged::platform
