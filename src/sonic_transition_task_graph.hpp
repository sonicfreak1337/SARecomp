#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <utility>
#include <vector>

namespace sonic::transition_task_graph {

// PAL 1.003: 098BE0 creates children with bucket 7 (098BEE: E507).
// 098558/09855A skip bucket insertion for 7; 098BF6..098C0A link them
// under parent+12. Destroy 09859C calls 098DA0, which recursively destroys
// that separate list at 098DC8. Bucket lists alone are not the task graph.
inline constexpr std::size_t no_parent = std::numeric_limits<std::size_t>::max();

struct Reader final {
    const void* context = nullptr;
    bool (*u32)(const void*, std::uint32_t, std::uint32_t&) = nullptr;
    bool (*range)(const void*, std::uint32_t, std::size_t) = nullptr;
    // Caller supplies its existing validated RAM alias/backing semantics.
    // This is identity normalization, not permission to dereference an alias.
    std::uint32_t (*backing_address)(std::uint32_t) = nullptr;
};

enum class Code {
    Ok, InvalidReader, HeadRange, TaskRange, Limit, DuplicateOrCycle,
    PreviousLink, ParentLink, InvalidGraph, CurrentDescendant,
    CallbackOwner, GraphChanged
};
struct Result final {
    Code code = Code::Ok;
    std::uint32_t address = 0;
    [[nodiscard]] explicit operator bool() const noexcept { return code == Code::Ok; }
};

struct Task final {
    std::uint32_t address = 0, backing = 0;
    std::uint32_t next = 0, previous = 0, parent = 0, children = 0;
    std::array<std::uint32_t, 3> callbacks{};
    std::size_t bucket = 0, parent_index = no_parent;
    bool operator==(const Task&) const = default;
};
struct Graph final {
    std::uint32_t bucket_heads = 0;
    std::size_t limit = 0;
    std::uint32_t (*backing_address)(std::uint32_t) = nullptr;
    std::vector<std::uint32_t> heads;
    // Preorder: ancestors always precede descendants. Guest addresses remain
    // intact for the original destructor; indices never retain RAM pointers.
    std::vector<Task> tasks;
};
struct Plan final {
    std::vector<std::size_t> root_indices;
    std::vector<std::size_t> destruction_indices;
};

// Read-only, iterative and bounded; failures leave out unchanged. Caller must
// keep guest execution paused throughout snapshot/plan/revalidate. A snapshot
// does not authorize future callbacks or cache executable owner generations.
[[nodiscard]] inline Result snapshot(const Reader& reader,
    std::uint32_t bucket_heads, std::size_t bucket_count, std::size_t limit,
    Graph& out) {
    if (!reader.u32 || !reader.range || !reader.backing_address ||
        !bucket_count || !limit || bucket_count > limit ||
        bucket_count > (std::numeric_limits<std::uint32_t>::max() - bucket_heads) / 4u)
        return {Code::InvalidReader, bucket_heads};
    Graph graph;
    graph.bucket_heads = bucket_heads;
    graph.limit = limit;
    graph.backing_address = reader.backing_address;
    graph.heads.resize(bucket_count);
    graph.tasks.reserve(limit);
    for (std::size_t b = 0; b < bucket_count; ++b)
        if (!reader.u32(reader.context, bucket_heads + static_cast<std::uint32_t>(4u*b), graph.heads[b]))
            return {Code::HeadRange, bucket_heads + static_cast<std::uint32_t>(4u*b)};
    struct Pending { std::uint32_t address, previous; std::size_t parent, bucket; };
    std::vector<Pending> pending;
    pending.reserve(limit);
    const auto same = [&](std::uint32_t a, std::uint32_t b) {
        return a == 0 || b == 0 ? a == b : reader.backing_address(a) == reader.backing_address(b);
    };
    for (std::size_t b = 0; b < bucket_count; ++b) {
        if (graph.heads[b]) pending.push_back({graph.heads[b], 0, no_parent, b});
        while (!pending.empty()) {
            const auto item = pending.back();
            pending.pop_back();
            if (graph.tasks.size() == limit) return {Code::Limit, item.address};
            if ((item.address & 3u) || item.address > UINT32_MAX - 47u ||
                !reader.range(reader.context, item.address, 48u))
                return {Code::TaskRange, item.address};
            const auto backing = reader.backing_address(item.address);
            for (const auto& old : graph.tasks)
                if (old.backing == backing) return {Code::DuplicateOrCycle, item.address};
            Task task;
            task.address = item.address;
            task.backing = backing;
            task.bucket = item.bucket;
            task.parent_index = item.parent;
            const auto read = [&](std::uint32_t offset, std::uint32_t& value) {
                return reader.u32(reader.context, item.address + offset, value);
            };
            if (!read(0, task.next) || !read(4, task.previous) ||
                !read(8, task.parent) || !read(12, task.children) ||
                !read(16, task.callbacks[0]) || !read(20, task.callbacks[1]) ||
                !read(24, task.callbacks[2])) return {Code::TaskRange, item.address};
            if (!same(task.previous, item.previous)) return {Code::PreviousLink, item.address};
            const auto expected_parent = item.parent == no_parent ? 0u : graph.tasks[item.parent].address;
            if (!same(task.parent, expected_parent)) return {Code::ParentLink, item.address};
            const auto index = graph.tasks.size();
            graph.tasks.push_back(task);
            // LIFO gives child-before-next preorder; each edge is visited once.
            if (task.next) pending.push_back({task.next, task.address, item.parent, b});
            if (task.children) pending.push_back({task.children, 0, index, b});
            if (pending.size() > limit) return {Code::Limit, item.address};
        }
    }
    out = std::move(graph);
    return {};
}

// Predicates retain the adapter's existing executable-range / live-generation
// checks. Every callback in the actual destruction closure is checked, including
// children whose own callbacks do not lie in a retiring executable. Ancestor
// coverage removes duplicate destructor calls, not original child destruction.
template<class IsRetiring, class CallbackOwnerBound>
[[nodiscard]] Result plan_retirement(const Graph& graph, std::uint32_t current_task,
    IsRetiring&& is_retiring, CallbackOwnerBound&& callback_owner_bound, Plan& out) {
    if (!graph.backing_address) return {Code::InvalidGraph, 0};
    Plan plan;
    std::vector<bool> covered(graph.tasks.size(), false);
    for (std::size_t i = 0; i < graph.tasks.size(); ++i) {
        const auto& task = graph.tasks[i];
        if (task.parent_index != no_parent && task.parent_index >= i)
            return {Code::InvalidGraph, task.address};
        const bool inherited = task.parent_index != no_parent && covered[task.parent_index];
        const bool selected = is_retiring(task);
        covered[i] = inherited || selected;
        if (!covered[i]) continue;
        if (current_task && graph.backing_address(current_task) == task.backing)
            return {Code::CurrentDescendant, current_task};
        for (const auto callback : task.callbacks)
            if (callback && !callback_owner_bound(callback))
                return {Code::CallbackOwner, callback};
        if (!inherited) plan.root_indices.push_back(i);
        plan.destruction_indices.push_back(i);
    }
    out = std::move(plan);
    return {};
}

// Detect link/callback/alias changes before the FIRST mutation. After each
// original destructor the graph legitimately changes: take a fresh snapshot
// and plan instead of reusing root indices from the previous graph. A finite
// transaction-wide destruction budget must be enforced by the caller too.
[[nodiscard]] inline Result revalidate(const Reader& reader, const Graph& expected) {
    if (reader.backing_address != expected.backing_address)
        return {Code::InvalidReader, expected.bucket_heads};
    Graph observed;
    const auto result = snapshot(reader, expected.bucket_heads, expected.heads.size(), expected.limit, observed);
    if (!result) return result;
    if (observed.heads != expected.heads || observed.tasks != expected.tasks)
        return {Code::GraphChanged, expected.bucket_heads};
    return {};
}

} // namespace sonic::transition_task_graph
