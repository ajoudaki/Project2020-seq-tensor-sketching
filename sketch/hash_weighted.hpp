#pragma once

#include "hash_base.hpp"

#include "util/timer.hpp"
#include "util/utils.hpp"

#include <iostream>
#include <random>

namespace ts { // ts = Tensor Sketch

/**
 * Naive implementation of weighted min-hash sketching. For more efficient implementations, see
 * https://static.googleusercontent.com/media/research.google.com/en//pubs/archive/36928.pdf and
 * https://www.microsoft.com/en-us/research/wp-content/uploads/2010/06/ConsistentWeightedSampling2.pdf
 *
 * Given a set S, and a sequence s=s1...sn with elements from S, this class computes a vector
 * {hmin_1(s), hmin_2(s), ..., hmin_sketch_size(s)}, where hmin_k(s)=s_i, such that h_k(s_i, #s_i)
 * is the smallest of h_k(s_1, 1..#s_1), h_k(s_2, 1..#s_2), ..., h_k(s_n, 1..#s_n) and
 * h_k:Sx{1..n} -> {1..#set_size} is a random permuation of the elements in S and #s_i denotes the
 * number of occurences of s_i in the sequence s.
 * @tparam T the type of S's elements
 */
template <class T>
class WeightedMinHash : public HashBase<T> {
  public:
    WeightedMinHash() {}

    /**
     * Constructs a weighted min-hasher for the given alphabet size which constructs sketches of the
     * given set size, dimension and maximum length.
     * @param set_size the number of elements in S,
     * @param sketch_dim the number of components (elements) in the sketch vector.
     * @param max_len maximum sequence length to be hashed.
     */
    WeightedMinHash(T set_size, size_t sketch_dim, size_t max_len)
        : HashBase<T>(set_size, sketch_dim, max_len * set_size), max_len(max_len) {}

    Vec<T> compute(const std::vector<T> &kmers) {
        if (kmers.size() > max_len) {
            throw std::invalid_argument("Sequence too long. Maximum sequence length is "
                                        + std::to_string(max_len)
                                        + ". Set --max_length to a higher value.");
        }
        Timer::start("weighted_minhash");
        Vec<T> sketch = Vec<T>(this->sketch_dim);
        if (kmers.empty()) {
            Timer::stop();
            return sketch;
        }
        for (size_t si = 0; si < this->sketch_dim; si++) {
            T min_char;
            size_t min_rank = this->hashes[0].size() + 1;
            Vec<size_t> cnts(this->set_size, 0);
            for (const auto s : kmers) {
                auto r = this->hashes[si][s + cnts[s] * this->set_size];
                cnts[s]++;
                if (r < min_rank) {
                    min_rank = r;
                    min_char = s;
                }
            }
            sketch[si] = min_char;
        }
        Timer::stop();

        return sketch;
    }

    /**
     * Computes the ordered min-hash sketch for the given sequence.
     * @param sequence the sequence to compute the ordered min-hash for
     * @param k-mer length; the sequence will be transformed into k-mers and the k-mers will be
     * hashed
     * @param number of characters in the alphabet over which sequence is defined
     * @return the ordered min-hash sketch of #sequence
     * @tparam C the type of characters in #sequence
     */
    template <typename C>
    Vec<T> compute(const std::vector<C> &sequence, uint32_t k, uint32_t alphabet_size) {
        Timer::start("compute_sequence");
        Vec<T> kmers = seq2kmer<C, T>(sequence, k, alphabet_size);
        Vec<T> sketch = compute(kmers);
        Timer::stop();
        return sketch;
    }

  private:
    size_t max_len;
};

} // namespace ts