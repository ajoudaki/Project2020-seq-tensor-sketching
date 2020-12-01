
#include "sketch/hash_weighted.hpp"

#include <gtest/gtest.h>
#include <gmock/gmock.h>

#include <random>

namespace {

using namespace ts;
using namespace ::testing;

TEST(WeightedMinHash, Empty) {
    WeightedMinHash<uint8_t> under_test(4 * 4 * 4, 3, 100);
    Vec<uint8_t> sketch = under_test.compute(std::vector<uint8_t>());
    ASSERT_THAT(sketch, ElementsAre(0, 0, 0));
}

TEST(WeightedMinHash, Repeat) {
    WeightedMinHash<uint8_t> under_test(4 * 4 * 4, 3, 100);
    std::vector<uint8_t> sequence = { 0, 1, 2, 3, 4, 5 };
    Vec<uint8_t> sketch1 = under_test.compute(sequence);
    Vec<uint8_t> sketch2 = under_test.compute(sequence);
    ASSERT_THAT(sketch1, ElementsAreArray(sketch2));
}

TEST(WeightedMinHash, Permute) {
    WeightedMinHash<uint8_t> under_test(4 * 4 * 4, 3, 100);
    std::vector<uint8_t> sequence1 = { 0, 1, 2, 3, 4, 5 };
    std::vector<uint8_t> sequence2 = { 5, 4, 3, 2, 1, 0 };
    Vec<uint8_t> sketch1 = under_test.compute(sequence1);
    Vec<uint8_t> sketch2 = under_test.compute(sequence2);
    ASSERT_THAT(sketch1, ElementsAreArray(sketch2));
}

Vec2D<uint8_t> hash_init(uint32_t set_size, uint32_t sketch_dim, uint32_t max_len) {
    Vec2D<uint8_t> hashes(sketch_dim, Vec<uint8_t>(set_size * max_len, 0));
    for (size_t m = 0; m < sketch_dim; m++) {
        std::iota(hashes[m].begin(), hashes[m].end(), 0);
    }
    return hashes;
}

TEST(WeightedMinHash, PresetHash) {
    WeightedMinHash<uint8_t> under_test(4 * 4, 3, 100);
    under_test.set_hashes_for_testing(hash_init(4 * 4, 3, 100));
    for (uint32_t i = 0; i < 4 * 4; ++i) {
        std::vector<uint8_t> sequence(4 * 4 - i);
        std::iota(sequence.begin(), sequence.end(), i);
        Vec<uint8_t> sketch = under_test.compute(sequence);
        ASSERT_THAT(sketch, ElementsAreArray({ i, i, i }));
    }
}

TEST(WeightedMinHash, PresetHashRepeat) {
    constexpr uint32_t set_size = 4 * 4; // corresponds to k-mers of length 2 over the DNA alphabet
    WeightedMinHash<uint8_t> under_test(set_size, 3, 100);
    under_test.set_hashes_for_testing(hash_init(set_size, 3, 100));
    for (uint32_t i = 0; i < set_size; ++i) {
        std::vector<uint8_t> sequence(2 * (set_size - i));
        std::iota(sequence.begin(), sequence.begin() + sequence.size() / 2, i);
        std::iota(sequence.begin() + sequence.size() / 2, sequence.end(), i);
        Vec<uint8_t> sketch = under_test.compute(sequence);
        ASSERT_THAT(sketch, ElementsAreArray({ i, i, i }));
    }
}

TEST(WeightedMinhash, SequenceTooLong) {
    constexpr uint32_t set_size = 4 * 4; // corresponds to k-mers of length 2 over the DNA alphabet
    WeightedMinHash<uint8_t> under_test(set_size, 3, 100);
    std::vector<uint8_t> sequence(100 + 1);
    ASSERT_THROW(under_test.compute(sequence), std::invalid_argument);
}

} // namespace