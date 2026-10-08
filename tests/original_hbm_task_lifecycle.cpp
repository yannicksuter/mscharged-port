#include "platform/interrupts.h"
#include "platform/thread_queues.h"
#include "revolution/hbm/nw4hbm/snd/TaskManager.h"
#include "revolution/hbm/nw4hbm/snd/TaskThread.h"
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <dlfcn.h>
#include <stdexcept>
#include <thread>

// This observer alone uses -fno-access-control. Original classes and TUs stay
// unchanged, and no observation supplies or mutates a source owner or field.
namespace {
using nw4hbm::snd::detail::TaskManager;
using nw4hbm::snd::detail::TaskThread;
using namespace mscharged::platform;
using namespace std::chrono_literals;
unsigned checks{};

void Check(bool value, const char* message) {
    ++checks;
    if (!value) throw std::runtime_error(message);
}
class Mask {
public:
    Mask() : previous_(OSDisableInterrupts()) {}
    ~Mask() { OSRestoreInterrupts(previous_); }
private:
    BOOL previous_;
};
template<class F> void Until(F condition) {
    const auto limit = std::chrono::steady_clock::now() + 2s;
    while (!condition()) {
        if (std::chrono::steady_clock::now() >= limit)
            throw std::runtime_error("Original TaskThread did not reach its actual wait boundary");
        std::this_thread::yield();
    }
}
bool Blocked(TaskThread& source) {
    Mask mask;
    return source.mThread.state == OS_THREAD_STATE_WAITING
        && source.mThread.queue == &source.mMsgQueue.queueReceive
        && source.mMsgQueue.queueReceive.head == &source.mThread
        && source.mMsgQueue.queueReceive.tail == &source.mThread
        && source.mMsgQueue.usedCount == 0;
}
template<class F> F Symbol(void* module, const char* name) {
    dlerror();
    auto address = dlsym(module, name);
    const auto* error = dlerror();
    if (error || !address) throw std::runtime_error(error ? error : "Missing HBM task test export");
    return reinterpret_cast<F>(address);
}
}

int main(int argc, char** argv) {
    try {
        Check(argc == 2, "Expected the actual selected-source module path");
        auto* module = dlopen(argv[1], RTLD_NOW | RTLD_LOCAL);
        if (!module) throw std::runtime_error(dlerror());
        const auto owner = Symbol<void*(*)()>(module, "charged_hbm_task_thread_owner");
        const auto create = Symbol<bool(*)(s32)>(module, "charged_hbm_task_thread_create");
        const auto destroy = Symbol<BOOL(*)()>(module, "charged_hbm_task_thread_destroy");
        const auto wake = Symbol<void(*)()>(module, "charged_hbm_task_thread_wake");
        const auto manager_owner = Symbol<void*(*)()>(module, "charged_hbm_task_manager_owner");
        const auto free_blocks = Symbol<u32(*)()>(module, "charged_hbm_task_free_blocks");
        const auto block_size = Symbol<u32(*)()>(module, "charged_hbm_task_block_size");
        auto* caller = OSGetCurrentThread();
        auto* context = OSGetCurrentContext();
        auto& source = *static_cast<TaskThread*>(owner());
        Check(!destroy(), "Cold original Destroy fabricated a worker");
        u32 initial_blocks{}, initial_block_size{};
        for (unsigned cycle = 0; cycle < 4; ++cycle) {
            Check(create(12), "Original TaskThread Create failed");
            // A selected-source observation, not Create-return readiness. No
            // wait is added to source callers to hide the scheduling boundary.
            Until([&] { return Blocked(source); });
            {
                Mask mask;
                Check(source.mCreateFlag && source.mThread.suspend == 0 && source.mThread.base == 12,
                    "Original Create/Resume did not establish the real source owner");
                Check(source.mMsgQueue.msgArray == source.mMsgBuffer && source.mMsgQueue.msgCount == 8,
                    "Original worker did not initialize its own message queue");
                Check(!source.mThread.queueJoin.head && !source.mThread.queueJoin.tail,
                    "A source join was invented before Destroy");
            }
            const auto* descriptor = &source.mThread;
            Check(create(12) && descriptor == &source.mThread && Blocked(source),
                "Repeated original Create changed its real worker lifetime");
            s32 before{};
            {
                Mask mask;
                before = source.mMsgQueue.firstIndex;
            }
            wake();
            Until([&] {
                if (!Blocked(source)) return false;
                Mask mask;
                return source.mMsgQueue.firstIndex == (before + 1) % 8;
            });
            auto& manager = *static_cast<TaskManager*>(manager_owner());
            {
                Mask mask;
                Check(manager.mHeapHandle != MEM_HEAP_INVALID_HANDLE && manager.mHeapHandle->signature == 0x554e5448,
                    "Original Execute did not construct its real unit heap");
                Check(!manager.mCurrentTask && !manager.mMutex.thread && manager.mMutex.count == 0,
                    "Original empty Execute retained a task or mutex owner");
                for (unsigned priority = 0; priority < TaskManager::PRIORITY_MAX; ++priority)
                    Check(manager.mTaskList[priority].IsEmpty(), "Original empty priority list changed");
                const auto size = block_size(), count = free_blocks();
                const auto first = reinterpret_cast<std::uintptr_t>(manager.mHeapHandle->heapStart);
                const auto end = reinterpret_cast<std::uintptr_t>(manager.mHeapHandle->heapEnd);
                Check(size >= 64 && end >= first && count == (end - first) / size && count > 0,
                    "Original unit heap did not describe its actual free storage");
                if (!cycle) { initial_blocks = count; initial_block_size = size; }
                Check(count == initial_blocks && size == initial_block_size,
                    "Empty original worker changed the existing task storage");
            }
            Check(destroy(), "Original MSG_DONE/Destroy did not join its real worker");
            {
                Mask mask;
                Check(!source.mCreateFlag && source.mThread.state == OS_THREAD_STATE_EXITED
                    && !source.mThread.queue && source.mMsgQueue.usedCount == 0,
                    "Original return/join did not retire source execution before its flag clear");
                Check(!source.mMsgQueue.queueReceive.head && !source.mMsgQueue.queueReceive.tail
                    && !source.mMsgQueue.queueSend.head && !source.mMsgQueue.queueSend.tail
                    && !source.mThread.queueJoin.head && !source.mThread.queueJoin.tail,
                    "Original voluntary join retained message or join waiters");
            }
            Check(OSIsThreadTerminated(&source.mThread) && !destroy(),
                "Actual joined worker was confused with a new source lifetime");
            Check(OSGetCurrentThread() == caller && OSGetCurrentContext() == context && NativeInterruptsEnabled(),
                "Original voluntary lifecycle changed the caller context or mask");
        }
        DrainNativeThreadLifetimes();
        Check(dlclose(module) == 0, "Joined source module could not be unloaded");
        std::printf("Original HBM TaskThread: %u checks, four genuine receive/Execute/DONE/join cycles, "
            "RTLD_NOW; source unit heap %u blocks of %u bytes. Create-return readiness, full HBM and "
            "active cancellation remain unqualified.\n", checks, initial_blocks, initial_block_size);
        return 0;
    } catch (const std::exception& error) {
        std::fprintf(stderr, "Original HBM TaskThread failed: %s\n", error.what());
        std::fflush(nullptr);
        std::_Exit(1); // Retain a failed live owner, rather than invent cancellation or teardown.
    }
}
