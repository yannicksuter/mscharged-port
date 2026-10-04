#pragma once
#include "resources/frontend_scene.h"
#include <memory>

namespace mscharged::resources
{
// Bounded original presentation/slide animation over an owned FEN snapshot.
// Constructor/Reset restore exported assets, reset the presentation clock as
// SetActiveSlide(resetTime=true), then execute Update(0). No handlers/transitions.
// Advance commits the entire snapshot only on success. Scene references expire
// on the next successful Advance/Reset; retained layout texture/font handles do not.
class FrontendAnimationPlayback
{
    struct Impl;
    std::unique_ptr<Impl> impl_;
public:
    FrontendAnimationPlayback(const FrontendScene&,FrontendReference selected={});
    ~FrontendAnimationPlayback();
    FrontendAnimationPlayback(const FrontendAnimationPlayback&)=delete;
    FrontendAnimationPlayback& operator=(const FrontendAnimationPlayback&)=delete;
    void Advance(float delta);
    void Reset();
    const FrontendScene& Scene() const;
    float PresentationTime() const;
    std::size_t ChannelsEvaluated() const; // Last committed update, incl shared slide visits.
};
}
