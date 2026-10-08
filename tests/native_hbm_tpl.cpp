#include <aurora/aurora.h>
#include <dolphin/os.h>
#include "gfx/texture.hpp"

#include "NL/MemAlloc.h"
#include "platform/arc_data_transport.h"
#include "platform/game_allocation_ownership.h"
#include "revolution/hbm/nw4hbm/lyt/common.h"
#include "revolution/tpl.h"

#include <array>
#include <bit>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <stdexcept>
#include <vector>

namespace aurora { extern AuroraConfig g_config; }
void AuroraOSShutdown();

namespace {
using namespace mscharged::platform;
unsigned checks;
void Check(bool ok, const char* why) {
    ++checks;
    if (!ok) throw std::runtime_error(why);
}
template<class F> void Reject(F call) {
    ++checks;
    try { call(); }
    catch (const std::invalid_argument&) { return; }
    catch (const std::out_of_range&) { return; }
    throw std::runtime_error("Unqualified TPL data was accepted");
}
void W(unsigned char* p, std::uint32_t v) { p[0]=v>>24; p[1]=v>>16; p[2]=v>>8; p[3]=v; }
void H(unsigned char* p, std::uint16_t v) { p[0]=v>>8; p[1]=v; }
struct Spec { GXTexFmt format; std::uint16_t width, height; std::size_t pixelBytes; };
// Literal independent footprints include partial edge tiles, rather than
// deriving expected bounds from the binder's tile-size calculation.
constexpr Spec specs[] = {
    {GX_TF_I4,9,9,128}, {GX_TF_IA4,9,5,128},
    {GX_TF_IA8,5,5,128}, {GX_TF_RGB5A3,4,4,32}
};
std::vector<unsigned char> Palette(const Spec& spec, unsigned seed) {
    std::vector<unsigned char> a(64+spec.pixelBytes);
    auto* p=a.data();
    W(p,0x0020af30); W(p+4,2); W(p+8,12);
    // Two original descriptors intentionally share one texture header.
    W(p+12,28); W(p+20,28);
    H(p+28,spec.height); H(p+30,spec.width); W(p+32,spec.format); W(p+36,64);
    W(p+40,GX_REPEAT); W(p+44,GX_MIRROR); W(p+48,GX_LINEAR); W(p+52,GX_NEAR);
    W(p+56,std::bit_cast<std::uint32_t>(-0.5f)); p[60]=1;
    for (std::size_t i=64;i<a.size();++i) p[i]=static_cast<unsigned char>(i*11+seed);
    return a;
}
struct Archive {
    std::vector<unsigned char> raw;
    std::array<std::size_t,4> starts{};
    std::array<std::size_t,4> sizes{};
};
Archive MakeArchive() {
    Archive a; a.raw.resize(128);
    for (unsigned i=0;i<4;++i) {
        const auto palette=Palette(specs[i],i);
        a.starts[i]=a.raw.size(); a.sizes[i]=palette.size();
        a.raw.insert(a.raw.end(),palette.begin(),palette.end());
    }
    auto* p=a.raw.data(); W(p,0x55aa382d); W(p+4,32); W(p+8,92); W(p+12,128);
    W(p+32,0x01000000); W(p+40,5);
    constexpr std::uint32_t names[]={1,8,16,24};
    for (unsigned i=0;i<4;++i) {
        W(p+44+i*12,names[i]); W(p+48+i*12,a.starts[i]); W(p+52+i*12,a.sizes[i]);
    }
    std::memcpy(p+92,"\0i4.tpl\0ia4.tpl\0ia8.tpl\0rgb.tpl\0",32);
    return a;
}
void Publish(unsigned char* raw, const std::vector<unsigned char>& input) {
    GameByteWriteReservation write(raw,input.size());
    Check(write.Tracked(),"Synthetic byte producer has no genuine allocation owner");
    std::memcpy(raw,input.data(),input.size()); write.Complete(GameByteDomain::WiiSerialized);
}
GXTexObj_ Observe(const GXTexObj& object) {
    static_assert(std::is_trivially_copyable_v<GXTexObj_>);
    static_assert(sizeof(GXTexObj_)<=sizeof(GXTexObj));
    GXTexObj_ result; std::memcpy(&result,&object,sizeof(result)); return result;
}
void Descriptor(const GXTexObj& object, unsigned char* file, const Spec& spec) {
    const auto actual=Observe(object);
    Check(actual.data==file+64,"Real GX descriptor lost its original raw pixel pointer");
    Check(actual.width()==spec.width && actual.height()==spec.height && actual.format()==spec.format,
        "Original layout request changed real GX dimensions/format");
    Check(actual.wrap_s()==GX_REPEAT && actual.wrap_t()==GX_MIRROR,
        "Original wrap fields changed at GX ingress");
    Check(actual.min_filter()==GX_LINEAR && actual.mag_filter()==GX_NEAR,
        "Original filter fields changed at GX ingress");
    Check(actual.min_lod()==0 && actual.max_lod()==0 && actual.lod_bias()==-0.5f
        && actual.do_edge_lod() && !actual.bias_clamp() && actual.max_aniso()==GX_ANISO_1,
        "Original LOD request changed in the actual GX descriptor");
    Check(!actual.has_mips() && actual.userData==nullptr && actual.texObjId!=0 && actual.texDataVersion==1,
        "Real non-CLUT/nonmip GX descriptor initialization changed");
    Check(FindGameByteDomain(actual.data,spec.pixelBytes)==GameByteDomain::WiiSerialized,
        "GX pixel source was converted or lost its completion domain");
}
void Run(MemoryAllocator& owner) {
    const auto initial=owner.TotalFreeMemory();
    const auto archive=MakeArchive();
    std::uint64_t priorIncarnation=0;
    for (unsigned cycle=0;cycle<2;++cycle) {
        auto* raw=static_cast<unsigned char*>(owner.Allocate(archive.raw.size(),32,false));
        Check(raw!=nullptr,"Original archive allocation failed");
        Reject([&]{ TPLBind(raw+archive.starts[0]); });
        Publish(raw,archive.raw);
        NativeARCFileSpan span{};
        Check(FindNativeARCFileSpan(raw+archive.starts[0],span) && span.bytes==archive.sizes[0],
            "Embedded palette inherited an ARC tail instead of its exact FST extent");
        Check(span.archive.allocation.incarnation!=priorIncarnation,"Reload kept a retired allocation incarnation");
        priorIncarnation=span.archive.allocation.incarnation;
        Check(!FindNativeARCFileSpan(raw+archive.starts[0]+4,span),"Interior pointer became an exact ARC file");
        NativeTPLAddress32<char> saved{};
        // Bind the last file first. Earlier files must still work after only
        // the unrelated later TPL structural records become NativeHeader.
        for (int i=3;i>=0;--i) {
            auto* file=raw+archive.starts[i]; auto* pal=reinterpret_cast<TPLPalette*>(file);
            Check(pal->descriptorArray<reinterpret_cast<TPLDescriptor*>(std::uintptr_t(0x80000000)),
                "Original unbound comparison decoded a relative address");
            GXTexObj object{};
            nw4hbm::lyt::detail::InitGXTexObjFromTPL(&object,pal,7);
            Descriptor(object,file,specs[i]);
            Check(!(pal->descriptorArray<reinterpret_cast<TPLDescriptor*>(std::uintptr_t(0x80000000))),
                "Original bound comparison requested another relocation");
            Check(TPLGet(pal,7)==reinterpret_cast<TPLDescriptor*>(file+20)
                && TPLGet(pal,UINT32_MAX)==TPLGet(pal,1),"Whole original TPLGet modulo/index changed");
            auto* texture=TPLGet(pal,0)->textureHeader.Get();
            Check(texture==reinterpret_cast<TPLHeader*>(file+28) && texture->unpacked==1,
                "Original descriptor/header address or relocation marker changed");
            Check(FindGameByteDomain(file,12)==GameByteDomain::NativeHeader
                && FindGameByteDomain(file+12,16)==GameByteDomain::NativeHeader
                && FindGameByteDomain(file+28,36)==GameByteDomain::NativeHeader,
                "Structural word transport published the wrong domain");
            Check(std::memcmp(file+64,archive.raw.data()+archive.starts[i]+64,specs[i].pixelBytes)==0,
                "Original tiled pixel bytes changed during binding");
            std::vector<unsigned char> bound(file,file+archive.sizes[i]);
            nw4hbm::lyt::detail::InitGXTexObjFromTPL(&object,pal,0);
            Descriptor(object,file,specs[i]);
            Check(std::memcmp(file,bound.data(),bound.size())==0,"Repeated original layout call rebound native headers");
            if(i==0) saved=texture->data;
            object={}; // Retire the CPU descriptor borrower before its raw owner.
        }
        owner.Free(raw);
        GameCompletedSpan retired{};
        Check(!FindGameCompletedSpan(raw,1,retired) && !FindNativeARCFileSpan(raw+archive.starts[0],span),
            "Original free retained completed archive bytes");
        Reject([&]{ saved.Get(); });
    }
    // Standalone completed TPL: the existing SaveLoad binder/copy usage has no
    // enclosing ARC. This checks that consumer boundary, not SaveLoad callbacks.
    const Spec icon{GX_TF_RGB5A3,32,32,2048}; const auto authored=Palette(icon,13);
    auto* file=static_cast<unsigned char*>(owner.Allocate(authored.size(),32,false));
    Check(file!=nullptr,"Standalone icon allocation failed"); Publish(file,authored);
    auto* pal=reinterpret_cast<TPLPalette*>(file); TPLBind(pal);
    std::array<unsigned char,2048> copied{};
    std::memcpy(copied.data(),pal->descriptorArray.Get()[0].textureHeader->data.Get(),copied.size());
    Check(std::memcmp(copied.data(),authored.data()+64,copied.size())==0,
        "Standalone SaveLoad-style RGB5A3 copy changed raw pixels");
    owner.Free(file);
    // The malicious data offset stays inside the ARC but crosses the first
    // authoritative FST file. Binding must reject before any structural write.
    auto invalid=archive.raw; W(invalid.data()+archive.starts[0]+36,archive.sizes[0]-32);
    auto* raw=static_cast<unsigned char*>(owner.Allocate(invalid.size(),32,false));
    Check(raw!=nullptr,"Negative archive allocation failed"); Publish(raw,invalid);
    GXTexObj object{};
    Reject([&]{ nw4hbm::lyt::detail::InitGXTexObjFromTPL(&object,
        reinterpret_cast<TPLPalette*>(raw+archive.starts[0]),0); });
    Check(std::memcmp(raw,invalid.data(),invalid.size())==0,
        "Rejected cross-file pixel range partially mutated the archive");
    owner.Free(raw);
    Check(owner.TotalFreeMemory()==initial,
        "Original allocator did not recover all test owners");
}
}
int main() {
    try {
        aurora::g_config.mem1Size=MEM1_DEFAULT_SIZE; aurora::g_config.mem2Size=64u*1024u*1024u;
        OSInit();
        constexpr unsigned arenaBytes=65536;
        auto* arena=OSAllocFromArenaLo(arenaBytes,64);
        Check(arena!=nullptr,"Real SDK arena allocation failed");
        MemoryAllocator owner{}; owner.Initialize(arena,arenaBytes); Run(owner);
        AuroraOSShutdown();
        std::printf("Original HOME TPL/GX descriptors: %u checks passed (CPU only)\n",checks);
        return 0;
    } catch(const std::exception& error) {
        std::fprintf(stderr,"TPL check %u: %s\n",checks,error.what()); return 1;
    }
}
