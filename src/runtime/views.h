#pragma once
#include "runtime/lighting.h"
#include "NL/gl/glView.h"

namespace mscharged
{
// Owns the original root graph and target registry for one graphics session.
// A null drain callback permits CPU lifecycle checks; rendering requires GX.
class OriginalViews
{
    bool live_ = false;
public:
    OriginalViews(unsigned width, unsigned height, void (*drain)() = nullptr);
    ~OriginalViews();
    void Release();
    OriginalViews(const OriginalViews&) = delete;
    OriginalViews& operator=(const OriginalViews&) = delete;
};

class ViewMatrices : public GLViewInterface
{
public:
    nlMatrix4 view, projection;
    ViewMatrices() { view.SetIdentity(); projection.SetIdentity(); }
    void GetViewMatrix(nlMatrix4& out) const override { out = view; }
    void GetProjectionMatrix(nlMatrix4& out) const override { out = projection; }
    void GetInverseViewMatrix(nlMatrix4& out) const override;
    void GetViewProjectionMatrix(nlMatrix4& out) const override;
    const nlMatrix4* GetViewMatrix() const override { return &view; }
    const nlMatrix4* GetProjectionMatrix() const override { return &projection; }
};

// Child views render before their parents, using the original packet callbacks.
void RenderOriginalViews(float time, const GameLighting& lighting);
void InitializeNativeTargets(unsigned width, unsigned height, void (*drain)());
void ShutdownNativeTargets();
// Selected game layer owners release their views before the containing graph.
void SetViewLayerShutdown(void (*shutdown)());
}
