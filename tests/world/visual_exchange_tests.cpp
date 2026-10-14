#include "fo3-visual-exchange.h"
#include <array>
#include <cassert>
#include <cstdlib>
#include <iostream>
#include <new>
#include <string>

static size_t allocationBudget = SIZE_MAX;
void* operator new(size_t size) {
    if (allocationBudget != SIZE_MAX) {
        if (!allocationBudget) throw std::bad_alloc();
        --allocationBudget;
    }
    if (void* p = std::malloc(size ? size : 1)) return p;
    throw std::bad_alloc();
}
void* operator new[](size_t size) { return ::operator new(size); }
void operator delete(void* p) noexcept { std::free(p); }
void operator delete[](void* p) noexcept { std::free(p); }
void operator delete(void* p, size_t) noexcept { std::free(p); }
void operator delete[](void* p, size_t) noexcept { std::free(p); }

// Like GpuObject, these are explicit-disposal resource records. Copies are
// deleted so the production template cannot accidentally copy a live handle.
struct Shape {
    Shape() = default;
    Shape(Shape&&) = default;
    Shape& operator=(Shape&&) = default;
    Shape(const Shape&) = delete;
    Shape& operator=(const Shape&) = delete;
    unsigned handle = 0;
    int pose = 0;
    std::string model;
    std::vector<float> vertices;
};
using Plan = fo3scene::VisualExchange<Shape>;
using Objects = Plan::Objects;
static std::array<unsigned, 64> refs{}, releases{};
static Shape Resource(unsigned id) {
    Shape s; s.handle = id; s.pose = int(id);
    s.model = "An authored model path long enough to require allocation";
    s.vertices = {1,2,3,4}; ++refs.at(id); return s;
}
static void Dispose(Objects& objects) noexcept {
    for (auto& s : objects) if (s.handle) {
        assert(refs.at(s.handle)); --refs.at(s.handle); ++releases.at(s.handle);
        s.handle = 0;
    }
    objects.clear();
}
static Objects World() {
    Objects live; live.push_back(Resource(1)); live.push_back(Resource(2));
    live.push_back(Resource(1)); return live; // two actual references to shared buffers
}
static Objects Incoming() { Objects incoming; incoming.push_back(Resource(3)); return incoming; }
static void Clean() { for (auto count : refs) assert(count == 0); }

int main() {
    {
        auto live = World(), incoming = Incoming(); fo3scene::VisualPublication epoch;
        const auto storage = live.data(); const auto revision = epoch.Revision();
        auto plan = Plan::Prepare(live, epoch, {0,2}, incoming, Dispose);
        assert(plan && incoming.empty() && live.data() == storage && epoch.Revision() == revision);
        assert(refs[1] == 2 && refs[2] == 1 && refs[3] == 1);
        auto competing = Plan::Prepare(live, epoch, {}, incoming, Dispose); assert(competing);
        allocationBudget = 0;
        const bool committed = plan->Commit(live, epoch);
        allocationBudget = SIZE_MAX;
        assert(committed && live.size() == 3 && live[0].handle == 1 && live[1].handle == 1 && live[2].handle == 3);
        assert(!competing->Commit(live, epoch));
        // Kept slots carry their current metadata through both ownership moves.
        // Simulation exclusion is the outer coordinator's responsibility.
        live[0].pose = 77;
        allocationBudget = 0;
        const bool rolledBack = plan->Rollback(live, epoch);
        allocationBudget = SIZE_MAX;
        assert(rolledBack && live.data() == storage && live.size() == 3 && live[0].pose == 77);
        assert(live[1].handle == 2 && live[2].handle == 1 && live[2].vertices.size() == 4);
        assert(epoch.Revision() == revision+2 && !plan->Commit(live, epoch) && !plan->Rollback(live, epoch));
        plan.reset(); competing.reset();
        assert(refs[3] == 0 && refs[1] == 2 && refs[2] == 1);
        Dispose(live);
    }
    Clean();
    {
        auto live = World(), incoming = Incoming(); fo3scene::VisualPublication epoch;
        { auto cancelled = Plan::Prepare(live, epoch, {0}, incoming, Dispose); assert(cancelled); }
        assert(refs[1] == 2 && refs[2] == 1 && refs[3] == 0); // cancel frees only staged resources
        incoming = Incoming();
        { auto applied = Plan::Prepare(live, epoch, {0}, incoming, Dispose);
          assert(applied && applied->Commit(live, epoch)); }
        assert(live.size() == 2 && refs[1] == 1 && refs[2] == 0 && refs[3] == 1);
        Dispose(live); // removed shared slot released once, retained shared slot still valid
    }
    Clean();
    {
        auto live = World(); Objects incoming; incoming.push_back(Resource(1));
        fo3scene::VisualPublication epoch;
        assert(refs[1] == 3);
        auto plan = Plan::Prepare(live, epoch, {0}, incoming, Dispose); assert(plan);
        assert(plan->Commit(live, epoch) && refs[1] == 3);
        allocationBudget = 0; plan.reset(); allocationBudget = SIZE_MAX;
        assert(refs[1] == 2 && refs[2] == 0 && live[0].handle == live[1].handle);
        Dispose(live);
    }
    Clean();
    {
        auto live = World(); Objects incoming; fo3scene::VisualPublication epoch;
        auto plan = Plan::Prepare(live, epoch, {0,1,2}, incoming, Dispose); assert(plan);
        assert(plan->Commit(live, epoch));
        allocationBudget = 0; plan.reset(); allocationBudget = SIZE_MAX;
        assert(refs[1] == 2 && refs[2] == 1 && live.size() == 3); Dispose(live);
    }
    Clean();
    {
        auto live = World(); Objects incoming; fo3scene::VisualPublication epoch;
        const auto storage = live.data();
        auto empty = Plan::Prepare(live, epoch, {}, incoming, Dispose); assert(empty);
        allocationBudget = 0;
        const bool removed = empty->Commit(live, epoch), restored = empty->Rollback(live, epoch);
        allocationBudget = SIZE_MAX;
        assert(removed && restored && live.data() == storage && live.size() == 3);
        empty.reset(); Dispose(live);
        incoming = Incoming();
        auto enable = Plan::Prepare(live, epoch, {}, incoming, Dispose); assert(enable);
        allocationBudget = 0;
        const bool enabled = enable->Commit(live, epoch), disabled = enable->Rollback(live, epoch);
        allocationBudget = SIZE_MAX;
        assert(enabled && disabled && live.empty()); enable.reset();
    }
    Clean();
    {
        auto live = World(), incoming = Incoming(); fo3scene::VisualPublication epoch, other;
        assert(!Plan::Prepare(live, epoch, {0,0}, incoming, Dispose));
        assert(!Plan::Prepare(live, epoch, {2,1}, incoming, Dispose));
        assert(!Plan::Prepare(live, epoch, {3}, incoming, Dispose));
        assert(!Plan::Prepare(live, epoch, {}, live, Dispose));
        assert(!Plan::Prepare(live, epoch, {}, incoming, nullptr));
        auto plan = Plan::Prepare(live, epoch, {0}, incoming, Dispose); assert(plan);
        Objects wrong;
        assert(!plan->Commit(wrong, epoch) && !plan->Commit(live, other));
        epoch.Changed(); assert(!plan->Commit(live, epoch)); plan.reset();
        incoming = Incoming(); plan = Plan::Prepare(live, epoch, {}, incoming, Dispose);
        assert(plan && plan->Commit(live, epoch)); epoch.Changed();
        assert(!plan->Rollback(live, epoch) && live.size() == 1 && live[0].handle == 3);
        plan.reset(); Dispose(live);
    }
    Clean();
    {
        auto live = World(), incoming = Incoming(); fo3scene::VisualPublication epoch;
        auto plan = Plan::Prepare(live, epoch, {}, incoming, Dispose); assert(plan);
        // Extra protection if an unmarked reallocation changed the storage.
        live.reserve(live.capacity()+1); assert(!plan->Commit(live, epoch));
        plan.reset(); Dispose(live);
    }
    Clean();
    {
        auto live = World(), incoming = Incoming();
        alignas(fo3scene::VisualPublication) unsigned char memory[sizeof(fo3scene::VisualPublication)];
        auto* epoch = new(memory) fo3scene::VisualPublication;
        auto plan = Plan::Prepare(live, *epoch, {}, incoming, Dispose); assert(plan);
        epoch->~VisualPublication(); epoch = new(memory) fo3scene::VisualPublication;
        assert(!plan->Commit(live, *epoch)); plan.reset();
        epoch->~VisualPublication(); Dispose(live);
    }
    Clean();
    {
        auto live = World(), incoming = Incoming();
        fo3scene::VisualPublication exhausted(UINT64_MAX-1);
        assert(!Plan::Prepare(live, exhausted, {}, incoming, Dispose));
        fo3scene::VisualPublication finalPair(UINT64_MAX-2);
        auto plan = Plan::Prepare(live, finalPair, {}, incoming, Dispose); assert(plan);
        assert(plan->Commit(live, finalPair) && plan->Rollback(live, finalPair));
        assert(finalPair.Revision() == UINT64_MAX);
        assert(!Plan::Prepare(live, finalPair, {}, incoming, Dispose));
        finalPair.Changed(); assert(finalPair.Revision() == 0);
        plan.reset(); Dispose(live);
    }
    Clean();
    // Fail each preparation allocation in turn: neither handle set transfers
    // until every slot/index allocation has completed successfully.
    bool completed = false;
    for (size_t failAt = 0; failAt < 16 && !completed; ++failAt) {
        auto live = World(), incoming = Incoming(); fo3scene::VisualPublication epoch;
        const std::vector<size_t> retain{0,2}; const auto storage = live.data();
        try {
            allocationBudget = failAt;
            auto plan = Plan::Prepare(live, epoch, retain, incoming, Dispose);
            allocationBudget = SIZE_MAX;
            assert(plan && incoming.empty()); completed = true;
        } catch (const std::bad_alloc&) {
            allocationBudget = SIZE_MAX;
            assert(incoming.size() == 1 && incoming[0].handle == 3 && refs[3] == 1);
        }
        assert(live.data() == storage && live.size() == 3 && epoch.Revision() == 1);
        Dispose(live); Dispose(incoming); Clean();
    }
    assert(completed);
    std::cout << "Visual ownership preparation, shared/unique disposal, allocation-free publication/rollback and stale guards passed\n";
}
