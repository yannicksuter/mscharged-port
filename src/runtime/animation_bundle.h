#pragma once
#include "runtime/hierarchy_assets.h"
#include "runtime/sanim_assets.h"
#include <array>
#include <string>

namespace mscharged
{
// World::LoadChunks assigns following animations to the most recent hierarchy,
// retaining that association across resident then temporary data. Node count
// equality and similarly named standalone files do not establish this identity.
// This owner selects one authored direct-index set, not a pose/controller. Its
// native objects remain read-only and valid while their individual handles live.
class AnimationBundle
{
    HierarchyAsset::Handle hierarchy_;
    std::vector<SAnimAsset::Handle> animations_;
    AnimationBundle() = default;
public:
    using Handle = std::shared_ptr<const AnimationBundle>;
    static Handle Decode(resources::Bytes resident, resources::Bytes temporary, std::uint32_t hierarchy_hash);
    HierarchyAsset::Handle Hierarchy() const { return hierarchy_; }
    std::size_t Size() const { return animations_.size(); }
    SAnimAsset::Handle At(std::size_t index) const { return animations_.at(index); }
    // Original cInventory::AddStart/Find: the last file entry wins hash aliases.
    SAnimAsset::Handle Find(std::uint32_t hash) const;
    // cPN_SAnimController::Evaluate mirrors the target index before identity
    // mapping. Cross-signature/remapped sets require a qualified AnimRetarget
    // table and are rejected here; this method never synthesizes a pose.
    std::size_t AnimationNode(std::size_t node, bool mirror = false) const;
};

struct AnimationBundleRequest
{
    std::string resident, temporary; // Real NL paths, optionally ending .zlib.
    std::uint32_t hierarchy_hash = 0;
};
enum class AnimationBundleState { Idle, Loading, Ready, Failed, Cancelled };

// Two real NL reads form one transaction. Begin replaces any pending request;
// only complete validation replaces Current(). Failures/cancellation retain the
// previous bundle. Unload drops this owner's current handle, not external ones.
// Service/mutate/destroy on the creating thread before NL/arena shutdown.
class AnimationBundleLoad
{
    struct Implementation;
    std::unique_ptr<Implementation> impl_;
public:
    static constexpr std::size_t MaximumRetainedBytes = 32 * 1024 * 1024;
    AnimationBundleLoad();
    ~AnimationBundleLoad();
    AnimationBundleLoad(const AnimationBundleLoad&) = delete;
    AnimationBundleLoad& operator=(const AnimationBundleLoad&) = delete;
    void Begin(AnimationBundleRequest request);
    void Poll();
    void Service();
    void Cancel();
    void Unload();
    AnimationBundleState State() const;
    unsigned CompletedFiles() const;
    AnimationBundle::Handle Current() const;
    // Reports the latest transaction's error or pending/cancelled state.
    AnimationBundle::Handle Result() const;
};
}
