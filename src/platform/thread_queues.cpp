#include "platform/thread_queues.h"
#include "platform/interrupts.h"
#include "platform/thread.h"

#include <dolphin/os.h>
#include <dolphin/os/OSThread.h>

#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <mutex>
#include <stdexcept>
#include <unordered_map>

namespace {
using namespace mscharged::platform;

struct NativeThread;
struct Threads {
    std::mutex latch;
    std::unordered_map<OSThread*, NativeThread*> live;
};
Threads& State() { static Threads state; return state; }

class Mask {
public:
    Mask() : previous_(OSDisableInterrupts()) {}
    ~Mask() { OSRestoreInterrupts(previous_); }
    Mask(const Mask&) = delete;
    Mask& operator=(const Mask&) = delete;
private:
    BOOL previous_;
};

struct NativeThread {
    OSThread sdk{};
    std::condition_variable changed;
    bool waiting{}, servicing{};
    NativeThreadWaitService service{};

    NativeThread() {
        // The original default thread uses priority 16 and detached state.
        // Execution/register switching belongs to the native OS; the canonical
        // SDK object retains actual native stack bounds and opaque context data.
        const auto stack = mscharged::CurrentThreadStackLimits();
        sdk.state = OS_THREAD_STATE_RUNNING;
        sdk.attr = OS_THREAD_ATTR_DETACH;
        sdk.priority = sdk.base = 16;
        sdk.val = reinterpret_cast<void*>(std::uintptr_t{0xffffffffu});
        sdk.stackBase = reinterpret_cast<u8*>(stack.high);
        sdk.stackEnd = reinterpret_cast<u8*>(stack.low);
        if (auto* context = OSGetCurrentContext()) sdk.context = *context;
        NativeInterruptGuard exclusion;
        auto& state = State();
        std::lock_guard lock(state.latch);
        state.live.emplace(&sdk, this);
    }

    ~NativeThread() {
        // A returning native thread cannot retain a source critical section.
        // No source waiter is completed by thread retirement.
        OSEnableInterrupts();
        NativeInterruptGuard exclusion;
        auto& state = State();
        std::lock_guard lock(state.latch);
        if (waiting || sdk.queue) std::terminate();
        sdk.state = OS_THREAD_STATE_MORIBUND;
        state.live.erase(&sdk);
    }
};

NativeThread& Current() { thread_local NativeThread thread; return thread; }

NativeThread& Find(Threads& state, OSThread* thread) {
    const auto found = state.live.find(thread);
    if (found == state.live.end())
        throw std::logic_error("SDK thread is not a live native caller");
    return *found->second;
}

void ValidateQueue(Threads& state, OSThreadQueue* queue) {
    OSThread* previous{};
    std::size_t count{};
    for (auto* thread = queue->head; thread; thread = thread->link.next) {
        if (++count > state.live.size())
            throw std::logic_error("Native SDK wait queue has a cycle");
        auto& native = Find(state, thread);
        if (!native.waiting || thread->queue != queue ||
                thread->state != OS_THREAD_STATE_WAITING ||
                thread->link.prev != previous || thread->suspend != 0)
            throw std::logic_error("Native SDK wait queue ownership is unqualified");
        if (previous && previous->priority > thread->priority)
            throw std::logic_error("Native SDK wait queue priority order is invalid");
        previous = thread;
    }
    if (previous != queue->tail)
        throw std::logic_error("Native SDK wait queue tail is invalid");
}

void Remove(OSThreadQueue* queue, OSThread* thread) {
    if (thread->link.prev) thread->link.prev->link.next = thread->link.next;
    else queue->head = thread->link.next;
    if (thread->link.next) thread->link.next->link.prev = thread->link.prev;
    else queue->tail = thread->link.prev;
    thread->queue = nullptr;
    thread->link = {};
}

void Insert(OSThreadQueue* queue, OSThread* thread) {
    // Source SDK order: ascending priority, behind existing equal priorities.
    auto* next = queue->head;
    while (next && next->priority <= thread->priority) next = next->link.next;
    auto* previous = next ? next->link.prev : queue->tail;
    thread->queue = queue;
    thread->link = {next, previous};
    if (previous) previous->link.next = thread;
    else queue->head = thread;
    if (next) next->link.prev = thread;
    else queue->tail = thread;
}
} // namespace

extern "C" OSThread* OSGetCurrentThread() { return &Current().sdk; }

extern "C" void OSInitThreadQueue(OSThreadQueue* queue) {
    if (!queue) throw std::invalid_argument("SDK thread queue is null");
    Mask mask;
    auto& state = State();
    std::lock_guard lock(state.latch);
    // Constructor input need not be initialized. Only existing live borrowed
    // waiters forbid reinitialization; unknown old bytes are not dereferenced.
    for (const auto& entry : state.live)
        if (entry.second->waiting && entry.first->queue == queue)
            throw std::logic_error("Cannot reinitialize a borrowed native wait queue");
    queue->tail = nullptr;
    queue->head = nullptr;
}

extern "C" void OSWakeupThread(OSThreadQueue* queue) {
    if (!queue) throw std::invalid_argument("SDK thread queue is null");
    Mask mask;
    auto& state = State();
    std::lock_guard lock(state.latch);
    ValidateQueue(state, queue);
    while (auto* thread = queue->head) {
        auto& native = Find(state, thread);
        Remove(queue, thread);
        thread->state = OS_THREAD_STATE_READY;
        native.waiting = false;
        // The native OS owns its actual run queue. This notification releases
        // a real blocked caller rather than completing a source operation.
        native.changed.notify_one();
    }
}

extern "C" void OSSleepThread(OSThreadQueue* queue) {
    if (!queue) throw std::invalid_argument("SDK thread queue is null");
    if (!NativeInterruptWaitAllowed())
        throw std::logic_error("Cannot sleep a native SDK interrupt/host exclusion scope");
    Mask mask;
    auto& native = Current();
    auto& state = State();
    {
        std::lock_guard lock(state.latch);
        if (native.waiting || native.servicing || native.sdk.queue ||
                native.sdk.state != OS_THREAD_STATE_RUNNING || native.sdk.suspend != 0 ||
                native.sdk.mutex || native.sdk.queueMutex.head || native.sdk.queueMutex.tail)
            throw std::logic_error("Native SDK thread wait state is unqualified");
        ValidateQueue(state, queue);
        native.sdk.state = OS_THREAD_STATE_WAITING;
        native.waiting = true;
        Insert(queue, &native.sdk);
    }

    // A descheduled Wii thread does not keep hardware globally masked. Enqueue
    // first, then release the native mask; the wait predicate prevents a lost
    // wake in the gap before the actual condition-variable wait.
    OSEnableInterrupts();
    try {
        std::unique_lock lock(state.latch);
        while (native.waiting) {
            if (!native.service) {
                native.changed.wait(lock, [&] { return !native.waiting; });
                break;
            }
            if (native.changed.wait_for(lock, std::chrono::milliseconds(1),
                    [&] { return !native.waiting; })) break;
            native.servicing = true;
            const auto service = native.service;
            lock.unlock();
            try { service(); }
            catch (...) {
                lock.lock();
                native.servicing = false;
                throw;
            }
            lock.lock();
            native.servicing = false;
        }
        lock.unlock();
        OSDisableInterrupts();
        lock.lock();
        native.sdk.state = OS_THREAD_STATE_RUNNING;
    } catch (...) {
        // Unwind a failed host service without leaving borrowed source storage
        // linked or manufacturing successful source completion.
        OSDisableInterrupts();
        std::lock_guard lock(state.latch);
        if (native.waiting) Remove(queue, &native.sdk);
        native.waiting = native.servicing = false;
        native.sdk.state = OS_THREAD_STATE_RUNNING;
        throw;
    }
}

extern "C" BOOL OSSetThreadPriority(OSThread* thread, OSPriority priority) {
    if (priority < OS_PRIORITY_MIN || priority > OS_PRIORITY_MAX) return FALSE;
    Mask mask;
    auto& state = State();
    std::lock_guard lock(state.latch);
    auto& native = Find(state, thread);
    if (thread->mutex || thread->queueMutex.head || thread->queueMutex.tail || thread->suspend)
        throw std::logic_error("Native SDK mutex/suspension scheduling is unqualified");
    if (thread->base != priority) {
        auto* queue = native.waiting ? thread->queue : nullptr;
        if (queue) { ValidateQueue(state, queue); Remove(queue, thread); }
        thread->base = thread->priority = priority;
        if (queue) Insert(queue, thread);
    }
    return TRUE;
}

extern "C" s32 OSGetThreadPriority(OSThread* thread) {
    Mask mask;
    auto& state = State();
    std::lock_guard lock(state.latch);
    (void)Find(state, thread);
    return thread->priority;
}

namespace mscharged::platform {
NativeThreadWaitService SetNativeThreadWaitService(NativeThreadWaitService service) {
    if (NativeInterruptDispatchActive())
        throw std::logic_error("Cannot replace native wait service inside an interrupt");
    Mask mask;
    auto& native = Current();
    auto& state = State();
    std::lock_guard lock(state.latch);
    if (native.waiting || native.servicing)
        throw std::logic_error("Cannot replace an active native wait service");
    const auto previous = native.service;
    native.service = service;
    return previous;
}
} // namespace mscharged::platform
