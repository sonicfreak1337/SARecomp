#include "sonic_transition_task_graph.hpp"

#include <iostream>
#include <stdexcept>
#include <string>
#include <unordered_map>

namespace tg = sonic::transition_task_graph;
namespace {
constexpr std::uint32_t heads = 0x8C100000u;
constexpr std::uint32_t root = 0x8C100100u, child = 0x8C100200u;
constexpr std::uint32_t grand = 0x8C100300u, sibling = 0x8C100400u;
constexpr std::uint32_t other = 0x8C100500u, callback = 0x8C098380u;
std::uint32_t backing(std::uint32_t address) { return address & 0x1FFFFFFFu; }
void require(bool value, const char* message) {
    if (!value) throw std::runtime_error(message);
}
struct Fixture {
    std::unordered_map<std::uint32_t, std::uint32_t> words;
    void set(std::uint32_t address, std::uint32_t value) { words[backing(address)] = value; }
    void node(std::uint32_t address, std::uint32_t parent = 0) {
        for (std::uint32_t offset = 0; offset < 48; offset += 4) set(address + offset, 0);
        set(address + 8, parent);
        set(address + 16, callback);
    }
    Fixture() {
        for (std::uint32_t i = 0; i < 8; ++i) set(heads + 4*i, 0);
        node(root); node(child, root); node(grand, child);
        node(sibling, root); node(other);
        set(heads, root); set(heads + 12, other);
        set(root + 12, child); set(child + 12, grand);
        set(child, sibling); set(sibling + 4, child);
    }
    tg::Reader reader() const {
        return {this,
            [](const void* context, std::uint32_t address, std::uint32_t& value) {
                const auto& words = static_cast<const Fixture*>(context)->words;
                const auto it = words.find(backing(address));
                if (it == words.end()) return false;
                value = it->second;
                return true;
            },
            [](const void* context, std::uint32_t address, std::size_t bytes) {
                const auto& words = static_cast<const Fixture*>(context)->words;
                for (std::size_t offset = 0; offset < bytes; offset += 4)
                    if (!words.contains(backing(address + static_cast<std::uint32_t>(offset)))) return false;
                return true;
            }, &backing};
    }
    tg::Graph graph() const {
        tg::Graph out;
        require(static_cast<bool>(tg::snapshot(reader(), heads, 8, 4096, out)), "fixture snapshot");
        return out;
    }
};
bool owner(std::uint32_t address) { return address == callback; }
auto selected(std::initializer_list<std::uint32_t> addresses) {
    return [values = std::vector<std::uint32_t>(addresses)](const tg::Task& task) {
        for (const auto address : values) if (backing(address) == task.backing) return true;
        return false;
    };
}
void snapshot_reject(const Fixture& fixture, tg::Code code, std::size_t limit = 4096) {
    tg::Graph output;
    output.bucket_heads = 123;
    output.tasks.push_back(tg::Task{.address = 456});
    const auto result = tg::snapshot(fixture.reader(), heads, 8, limit, output);
    require(result.code == code, "wrong snapshot rejection");
    require(output.bucket_heads == 123 && output.tasks.size() == 1 && output.tasks[0].address == 456,
        "snapshot rejection mutated output");
}
void plan_reject(const tg::Graph& graph, std::uint32_t current,
    std::initializer_list<std::uint32_t> retiring, tg::Code code) {
    tg::Plan output{{123}, {456}};
    const auto result = tg::plan_retirement(graph, current, selected(retiring), owner, output);
    require(result.code == code, "wrong plan rejection");
    require(output.root_indices == std::vector<std::size_t>{123} &&
        output.destruction_indices == std::vector<std::size_t>{456}, "plan rejection mutated output");
}
template<class Body> void run(const char* name, Body&& body, std::size_t& count) {
    try { body(); ++count; }
    catch (const std::exception& error) { throw std::runtime_error(std::string(name) + ": " + error.what()); }
}
} // namespace

int main() {
    std::size_t count = 0;
    try {
        run("separate child lists and preorder", [] {
            const auto graph = Fixture{}.graph();
            require(graph.tasks.size() == 5, "children missing");
            const std::array expected{root, child, grand, sibling, other};
            for (std::size_t i = 0; i < expected.size(); ++i)
                require(graph.tasks[i].address == expected[i], "wrong original list order");
            require(graph.tasks[2].parent_index == 1 && graph.tasks[3].parent_index == 0 &&
                graph.tasks[4].parent_index == tg::no_parent, "wrong ancestry");
        }, count);
        run("selected ancestor across unselected parent", [] {
            tg::Plan plan;
            require(static_cast<bool>(tg::plan_retirement(Fixture{}.graph(), 0,
                selected({root, grand}), owner, plan)), "plan rejected");
            require(plan.root_indices == std::vector<std::size_t>{0} &&
                plan.destruction_indices == std::vector<std::size_t>({0,1,2,3}), "duplicate destructor root");
        }, count);
        run("selected child leaves parent and sibling alive", [] {
            tg::Plan plan;
            require(static_cast<bool>(tg::plan_retirement(Fixture{}.graph(), sibling,
                selected({child}), owner, plan)), "unrelated current rejected");
            require(plan.root_indices == std::vector<std::size_t>{1} &&
                plan.destruction_indices == std::vector<std::size_t>({1,2}), "wrong child closure");
        }, count);
        run("current unselected descendant", [] {
            plan_reject(Fixture{}.graph(), grand, {root}, tg::Code::CurrentDescendant);
        }, count);
        run("current descendant alias", [] {
            plan_reject(Fixture{}.graph(), backing(grand), {root}, tg::Code::CurrentDescendant);
        }, count);
        run("unknown callback in unselected descendant", [] {
            Fixture fixture; fixture.set(grand + 24, 0x8C900000u);
            plan_reject(fixture.graph(), 0, {root}, tg::Code::CallbackOwner);
        }, count);
        run("unknown callback outside closure is untouched", [] {
            Fixture fixture; fixture.set(other + 24, 0x8C900000u);
            tg::Plan plan;
            require(static_cast<bool>(tg::plan_retirement(fixture.graph(), 0, selected({child}), owner, plan)),
                "unrelated callback rejected");
        }, count);
        run("all three callback slots checked", [] {
            for (std::uint32_t offset : {16u,20u,24u}) {
                Fixture fixture; fixture.set(sibling + offset, 0x8C900000u);
                plan_reject(fixture.graph(), 0, {root}, tg::Code::CallbackOwner);
            }
        }, count);
        run("mixed physical P1 P2 links", [] {
            Fixture fixture;
            fixture.set(heads, backing(root));
            fixture.set(root + 12, child | 0x20000000u);
            fixture.set(child + 8, backing(root));
            fixture.set(sibling + 4, child | 0x20000000u);
            require(fixture.graph().tasks.size() == 5, "valid aliases rejected");
        }, count);
        run("child must never also occur in buckets", [] {
            Fixture fixture; fixture.set(heads + 12, backing(grand));
            snapshot_reject(fixture, tg::Code::DuplicateOrCycle);
        }, count);
        run("sibling cycle", [] {
            Fixture fixture; fixture.set(child, child);
            snapshot_reject(fixture, tg::Code::DuplicateOrCycle);
        }, count);
        run("child cycle through alias", [] {
            Fixture fixture; fixture.set(grand + 12, backing(root));
            snapshot_reject(fixture, tg::Code::DuplicateOrCycle);
        }, count);
        run("wrong parent", [] {
            Fixture fixture; fixture.set(child + 8, other);
            snapshot_reject(fixture, tg::Code::ParentLink);
        }, count);
        run("bucket root cannot claim parent", [] {
            Fixture fixture; fixture.set(root + 8, other);
            snapshot_reject(fixture, tg::Code::ParentLink);
        }, count);
        run("wrong sibling backlink", [] {
            Fixture fixture; fixture.set(sibling + 4, grand);
            snapshot_reject(fixture, tg::Code::PreviousLink);
        }, count);
        run("missing complete task range", [] {
            Fixture fixture; fixture.words.erase(backing(grand + 44));
            snapshot_reject(fixture, tg::Code::TaskRange);
        }, count);
        run("bounded deep child chain", [] {
            Fixture fixture;
            fixture.set(heads, 0x8C200000u); fixture.set(heads + 12, 0);
            for (std::uint32_t i = 0; i < 1000; ++i) {
                const auto address = 0x8C200000u + i*64u;
                fixture.node(address, i ? address - 64u : 0u);
                if (i != 999) fixture.set(address + 12, address + 64u);
            }
            require(fixture.graph().tasks.size() == 1000, "deep iterative traversal");
            snapshot_reject(fixture, tg::Code::Limit, 999);
        }, count);
        run("unchanged graph revalidates", [] {
            Fixture fixture;
            require(static_cast<bool>(tg::revalidate(fixture.reader(), fixture.graph())), "stable graph rejected");
        }, count);
        run("callback changed since snapshot", [] {
            Fixture fixture; const auto graph = fixture.graph();
            fixture.set(child + 20, callback);
            require(tg::revalidate(fixture.reader(), graph).code == tg::Code::GraphChanged, "callback change missed");
        }, count);
        run("valid relink invalidates old root indices", [] {
            Fixture fixture; const auto graph = fixture.graph();
            fixture.set(root + 12, sibling); fixture.set(sibling + 4, 0);
            fixture.set(sibling, child); fixture.set(child + 4, sibling); fixture.set(child, 0);
            require(tg::revalidate(fixture.reader(), graph).code == tg::Code::GraphChanged, "relink missed");
            tg::Plan plan;
            require(static_cast<bool>(tg::plan_retirement(fixture.graph(), 0, selected({child}), owner, plan)),
                "fresh plan rejected");
            require(plan.root_indices == std::vector<std::size_t>{2}, "stale root index reused");
        }, count);
        run("fresh graph after another destructor removed a root", [] {
            Fixture fixture; const auto old = fixture.graph();
            fixture.set(heads + 12, 0);
            require(tg::revalidate(fixture.reader(), old).code == tg::Code::GraphChanged, "removed root missed");
            tg::Plan plan;
            require(static_cast<bool>(tg::plan_retirement(fixture.graph(), 0, selected({other}), owner, plan)) &&
                plan.root_indices.empty(), "removed root retained");
        }, count);
        run("invalid caller supplied ancestry", [] {
            auto graph = Fixture{}.graph(); graph.tasks[1].parent_index = 2;
            plan_reject(graph, 0, {root}, tg::Code::InvalidGraph);
        }, count);
        run("empty retirement plan", [] {
            tg::Plan plan{{123}, {456}};
            require(static_cast<bool>(tg::plan_retirement(Fixture{}.graph(), grand, selected({}), owner, plan)) &&
                plan.root_indices.empty() && plan.destruction_indices.empty(), "empty plan");
        }, count);
        std::cout << "PASS transition task graph: " << count << " cases\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "FAIL after " << count << " cases: " << error.what() << '\n';
        return 1;
    }
}
