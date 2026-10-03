#pragma once
#include "runtime/world_objects.h"
#include "NL/nlMath.h"
#include <array>

class GLView;
namespace mscharged
{
// Six inward-facing, normalized world-space planes supplied by the caller.
// This copies and checks the planes; it does not derive or advance a camera.
class StaticWorldFrustum
{
    std::array<nlVector4, 6> planes_;
public:
    explicit StaticWorldFrustum(const std::array<nlVector4, 6>& planes);
    bool Visible(const resources::StaticWorldObject& object) const;
};

struct StaticWorldSubmission
{
    std::size_t objects = 0, visible = 0, opaque_packets = 0, alpha_packets = 0;
};

// The static branches of World::Render / WorldDrawable::DrawToView. Uses the
// owner's persistent instance matrices and original GLView packet sorting.
// Select world.Pool() before submission and keep it selected through dispatch
// (texture IDs resolve in that pool). Both views and the frustum must describe
// the same camera. The caller orders opaque before alpha in the view graph.
// Nothing is ticked or dispatched here. Keep the owner alive until submitted
// packets are reset and GPU work is drained. On allocation/submission failure,
// discard the frame's packet lists before releasing or retrying the batch.
StaticWorldSubmission SubmitStaticWorld(const StaticWorldObjects& world,
    GLView& opaque, GLView& alpha, const StaticWorldFrustum& frustum);
}
