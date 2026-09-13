#include "sonic_descriptor_index.hpp"

#include <array>
#include <iostream>
#include <random>
#include <stdexcept>
#include <utility>

using Index = sonic::texture::DescriptorCandidateIndex;
using Candidate = Index::Candidate;
using Source = Index::Source;

static void require(bool value, const char* message) {
    if (!value) throw std::runtime_error(message);
}

struct Model {
    std::vector<std::uint32_t> pvm;
    std::vector<std::vector<std::uint32_t>> bindings;
    Index index;
};

static std::vector<Candidate> linear(const Model& model, std::uint32_t key) {
    std::vector<Candidate> result;
    for (std::size_t i = 0; i < model.pvm.size(); ++i)
        if (model.pvm[i] == key) result.push_back({key, Source::Pvm, i, 0});
    for (std::size_t owner = 0; owner < model.bindings.size(); ++owner)
        for (std::size_t ordinal = 0; ordinal < model.bindings[owner].size(); ++ordinal)
            if (model.bindings[owner][ordinal] == key)
                result.push_back({key, Source::Binding, owner, ordinal});
    return result;
}

static void verify(Model& model) {
    require(model.index.prepare([&](const auto& add) {
        // Deliberately enumerate backwards; the index must restore the complete
        // original source/owner/ordinal order, including duplicate keys.
        for (std::size_t owner = model.bindings.size(); owner-- != 0;)
            for (std::size_t ordinal = model.bindings[owner].size(); ordinal-- != 0;)
                add(model.bindings[owner][ordinal], Source::Binding, owner, ordinal);
        for (std::size_t i = model.pvm.size(); i-- != 0;)
            add(model.pvm[i], Source::Pvm, i, 0);
    }), "index admission");
    for (std::uint32_t key = 0; key != 33; ++key) {
        const auto expected = linear(model, key);
        const auto actual = model.index.candidates(key);
        require(std::ranges::equal(actual, expected), "linear/index candidate order differs");
    }
}

int main() {
    try {
        Model model{{7, 7, 1}, {{7, 2, 7}, {}, {7}}, {}};
        verify(model);
        require(model.index.candidates(7).size() == 5, "later duplicate/conflict candidate lost");
        require(model.index.prepare([](const auto&) {
            throw std::runtime_error("warm index enumerated again");
        }), "warm index rejected");

        // Same size/storage, changed key: revision invalidation must defeat ABA.
        const auto* storage = model.pvm.data();
        auto revision = model.index.revision();
        model.index.invalidate();
        require(model.index.revision() != revision && !model.index.ready(), "revision did not change");
        require(model.index.candidates(7).empty(), "invalidated candidates exposed");
        model.pvm[0] = 3;
        require(model.pvm.data() == storage, "test did not retain storage");
        verify(model);
        model.index.invalidate(); model.pvm[0] = 7; verify(model);

        // Only positions are cached: reallocation without key/order changes is safe.
        model.pvm.reserve(256); model.bindings.reserve(64);
        for (auto& binding : model.bindings) binding.reserve(128);
        verify(model);

        std::mt19937 random(0x6084f6u);
        const auto random_key = [&] { return static_cast<std::uint32_t>(random() % 32u); };
        for (unsigned step = 0; step != 1024; ++step) {
            model.index.invalidate();
            switch (step % 7) {
            case 0: model.pvm.push_back(random_key()); break;
            case 1: if (!model.pvm.empty()) model.pvm.erase(model.pvm.begin()); break;
            case 2: model.bindings.push_back({random_key(), 7, 7}); break;
            case 3: if (!model.bindings.empty()) model.bindings.erase(model.bindings.begin()); break;
            case 4: if (!model.pvm.empty()) model.pvm[random() % model.pvm.size()] = random_key(); break;
            case 5: if (!model.bindings.empty()) {
                std::vector<std::uint32_t> rebound{7, random_key(), 7};
                model.bindings[random() % model.bindings.size()].swap(rebound);
            } break;
            case 6: {
                auto next = model.bindings;
                std::reverse(next.begin(), next.end());
                model.bindings.swap(next);
            } break;
            }
            verify(model);
        }

        // Rebuild failure must expose no partial candidate set and allow retry.
        model.index.invalidate();
        require(!model.index.prepare([](const auto& add) {
            add(7, Source::Pvm, 0, 0);
            throw std::bad_alloc{};
        }), "allocation failure admitted a partial index");
        require(!model.index.ready() && model.index.candidates(7).empty(), "partial index exposed");
        verify(model);

        // Move/copy preserve positions alongside the lists; full reset/restoration
        // cannot reuse the former state's cached candidates or revision readiness.
        Model copied = model; verify(copied);
        Model moved = std::move(copied); verify(moved);
        model = {};
        require(!model.index.ready(), "state reset retained index");
        verify(model);
        model.index.invalidate(); model.pvm.push_back(7);
        model.index.invalidate(); model.bindings.push_back({7, 7});
        verify(model);
        std::cout << "DESCRIPTOR_INDEX_TEST_OK differential_keys=33 mutation_steps=1024 order_duplicates_aba_reset_restore_allocation_failure\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "DESCRIPTOR_INDEX_TEST_FAIL " << error.what() << '\n';
        return 1;
    }
}
