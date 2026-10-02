// Typed native forwarding for the GX state calls selected material TUs use.
// Aurora owns the actual state cache and generates/caches shaders from TEV state.
#include "NL/glx/glxGX.h"
#include <dolphin/gx.h>
#include <utility>

namespace
{
unsigned channels, stages, generators;
}
unsigned gxSetNumChans(unsigned n)
{
    GXSetNumChans(n);
    return std::exchange(channels, n);
}
unsigned gxSetNumTevStages(unsigned n)
{
    GXSetNumTevStages(n);
    return std::exchange(stages, n);
}
unsigned gxSetNumTexGens(unsigned n)
{
    GXSetNumTexGens(n);
    return std::exchange(generators, n);
}
unsigned gxGetNumTevStages()
{
    return stages;
}
unsigned gxGetNumTexGens()
{
    return generators;
}
void gxSetTevOrder(int stage, int coord, int map, int colour)
{
    GXSetTevOrder(GXTevStageID(stage), GXTexCoordID(coord), GXTexMapID(map), GXChannelID(colour));
}
void gxSetTevColourIn(int stage, int a, int b, int c, int d)
{
    GXSetTevColorIn(GXTevStageID(stage), GXTevColorArg(a), GXTevColorArg(b), GXTevColorArg(c), GXTevColorArg(d));
}
void gxSetTevAlphaIn(int stage, int a, int b, int c, int d)
{
    GXSetTevAlphaIn(GXTevStageID(stage), GXTevAlphaArg(a), GXTevAlphaArg(b), GXTevAlphaArg(c), GXTevAlphaArg(d));
}
void gxSetTevColourOp(int stage, int op, int bias, int scale, bool clamp, int output)
{
    GXSetTevColorOp(GXTevStageID(stage), GXTevOp(op), GXTevBias(bias), GXTevScale(scale), clamp, GXTevRegID(output));
}
void gxSetTevAlphaOp(int stage, int op, int bias, int scale, bool clamp, int output)
{
    GXSetTevAlphaOp(GXTevStageID(stage), GXTevOp(op), GXTevBias(bias), GXTevScale(scale), clamp, GXTevRegID(output));
}
void gxSetTexCoordGen(int dst, int func, int src, unsigned mtx, bool normalize, unsigned post)
{
    GXSetTexCoordGen2(GXTexCoordID(dst), GXTexGenType(func), GXTexGenSrc(src), mtx, normalize, post);
}
void gxSetTexCoordGen(int dst, int func, int src, unsigned mtx)
{
    gxSetTexCoordGen(dst, func, src, mtx, false, GX_PTIDENTITY);
}
