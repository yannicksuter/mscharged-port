#pragma once
#include "NL/nlMath.h"

namespace mscharged
{
// Explicit diagnostic environment: vertex colour, no stadium lights or shadows.
// The original material's unlit TEV recipe still executes in full.
class MaterialPreviewScope
{
  public:
    MaterialPreviewScope(const nlMatrix4 &view, float time);
    ~MaterialPreviewScope();
    MaterialPreviewScope(const MaterialPreviewScope &) = delete;
    MaterialPreviewScope &operator=(const MaterialPreviewScope &) = delete;
};
void RequireUnlitMaterialPreview(bool supported);
const nlMatrix4 &MaterialPreviewView();
float MaterialPreviewTime();
void MaterialNormalMatrix(const nlMatrix4 &modelview, float output[3][4]);
} // namespace mscharged
