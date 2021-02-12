#pragma once

#include "util/multivec.hpp"
#include "util/timer.hpp"

#include <gflags/gflags.h>

#include <algorithm>
#include <cassert>
#include <cmath>
#include <numeric>
#include <vector>

DECLARE_string(dist);

namespace ts { // ts = Tensor Sketch

/**
 * Extracts k-mers from a sequence. The k-mer is treated as a number in base alphabet_size and then
 * converted to decimal, i.e. the sequence s1...sk is converted to s1*S^(k-1) + s2*S^(k-2) + ... +
 * sk, where k is the k-mer size.
 * @tparam chr types of elements in the sequence
 * @tparam kmer type that stores a kmer
 * @param seq the sequence to extract kmers from
 * @param kmer_size number of characters in a kmer
 * @param alphabet_size size of the alphabet
 * @return the extracted kmers, as integers converted from base #alphabet_size
 */
template <class chr, class kmer>
std::vector<kmer> seq2kmer(const std::vector<chr> &seq, uint8_t kmer_size, uint8_t alphabet_size) {
    Timer timer("seq2kmer");
    if (seq.size() < (size_t)kmer_size) {
        return std::vector<kmer>();
    }

    std::vector<kmer> result(seq.size() - kmer_size + 1, 0);

    kmer c = 1;
    for (uint8_t i = 0; i < kmer_size; i++) {
        result[0] += c * seq[i];
        c *= alphabet_size;
    }
    c /= alphabet_size;

    for (size_t i = 0; i < result.size() - 1; i++) {
        kmer base = result[i] - seq[i];
        assert(base % alphabet_size == 0);
        result[i + 1] = base / alphabet_size + seq[i + kmer_size] * c;
    }
    return result;
}

template <class T>
T l1_dist(const std::vector<T> &a, const std::vector<T> &b) {
    assert(a.size() == b.size());
    T res = 0;
    for (size_t i = 0; i < a.size(); i++) {
        auto el = std::abs(a[i] - b[i]);
        res += el;
    }
    return res;
}


template <class T>
T l2_dist(const std::vector<T> &a, const std::vector<T> &b) {
    assert(a.size() == b.size());
    T res = 0;
    for (size_t i = 0; i < a.size(); i++) {
        auto el = std::abs(a[i] - b[i]);
        res += el * el;
    }
    return res;
}

/**
 * Return the probability p that maximizes the probability of the given observations under the
 * assumption that the input is i.i.d. distributed as A~Norm(0, sigma), and the output is
 * distributed as B ~ p * A + Norm(0, sigma * sqrt(1-p)).
 *
 * For symmetry, we maximise the likelyhood of getting both A from B and B from A.
 */
template <class T>
T most_likely_distance(const std::vector<T> &a, const std::vector<T> &b) {
    // p minimizes 1/(1-p) * ( sum_i a_i^2  - 2 p sum_i a_i b_i + p^2 sum_i b^2)
    // p = ||a-b||_2 / sqrt(||a||_2 + ||b||_2)
    assert(a.size() == b.size());
    T aa = 0;
    T ab = 0;
    T bb = 0;
    for (size_t i = 0; i < a.size(); i++) {
        aa += a[i] * a[i];
        ab += (a[i] - b[i]) * (a[i] - b[i]);
        bb += b[i] * b[i];
    }
    // NOTE: Instead of dividing by the average norm^2 of a and b, it's also possible to divide by
    // either |a| or |b| to get an asymmetric distance that computes the most likely p to transition
    // from a to b.
    return std::sqrt(ab / ((aa + bb) / 2));
}

template <class T>
T sketch_dist(const std::vector<T> &a, const std::vector<T> &b) {
    assert(a.size() == b.size());
    if (FLAGS_dist == "l1")
        return l1_dist(a, b);
    if (FLAGS_dist == "l2")
        return l2_dist(a, b);
    if (FLAGS_dist == "exp")
        return most_likely_distance(a, b);
    assert(false && "Value of dist flag is not a known value. Must be one of l1|l2|exp.");
}


template <class T>
T l1_dist2D_minlen(const Vec2D<T> &a, const Vec2D<T> &b) {
    auto len = std::min(a.size(), b.size());
    T val = 0;
    for (size_t i = 0; i < len; i++) {
        for (size_t j = 0; j < a[i].size() and j < b[i].size(); j++) {
            auto el = std::abs(a[i][j] - b[i][j]);
            val += el;
        }
    }
    return val;
}

template <class T>
T l2_dist2D_minlen(const Vec2D<T> &a, const Vec2D<T> &b) {
    auto len = std::min(a.size(), b.size());
    T val = 0;
    for (size_t i = 0; i < len; i++) {
        for (size_t j = 0; j < a[i].size() and j < b[i].size(); j++) {
            auto el = (a[i][j] - b[i][j]);
            val += el * el;
        }
    }
    return val;
}


template <class T>
T hamming_dist(const std::vector<T> &a, const std::vector<T> &b) {
    assert(a.size() == b.size());
    T diff = 0;
    for (size_t i = 0; i < a.size(); i++) {
        if (a[i] != b[i]) {
            diff++;
        }
    }
    return diff;
}

template <class seq_type>
int lcs(const std::vector<seq_type> &s1, const std::vector<seq_type> &s2) {
    size_t m = s1.size();
    size_t n = s2.size();
    //        int L[m + 1][n + 1];
    Vec2D<int> L(m + 1, std::vector<int>(n + 1, 0));
    for (size_t i = 0; i <= m; i++) {
        for (size_t j = 0; j <= n; j++) {
            if (i == 0 || j == 0) {
                L[i][j] = 0;
            } else if (s1[i - 1] == s2[j - 1]) {
                L[i][j] = L[i - 1][j - 1] + 1;
            } else {
                L[i][j] = std::max(L[i - 1][j], L[i][j - 1]);
            }
        }
    }
    return L[m][n];
}

template <class seq_type>
size_t lcs_distance(const std::vector<seq_type> &s1, const std::vector<seq_type> &s2) {
    return s1.size() + s2.size() - 2 * lcs(s1, s2);
}

template <class seq_type>
size_t edit_distance(const std::vector<seq_type> &s1, const std::vector<seq_type> &s2) {
    Timer timer("edit_distance");
    const size_t m(s1.size());
    const size_t n(s2.size());

    if (m == 0)
        return n;
    if (n == 0)
        return m;

    auto costs = std::vector<size_t>(n + 1);

    for (size_t k = 0; k <= n; k++)
        costs[k] = k;

    size_t i = 0;
    for (auto it1 = s1.begin(); it1 != s1.end(); ++it1, ++i) {
        costs[0] = i + 1;
        size_t corner = i;

        size_t j = 0;
        for (auto it2 = s2.begin(); it2 != s2.end(); ++it2, ++j) {
            size_t upper = costs[j + 1];
            if (*it1 == *it2) {
                costs[j + 1] = corner;
            } else {
                size_t t(upper < corner ? upper : corner);
                costs[j + 1] = (costs[j] < t ? costs[j] : t) + 1;
            }

            corner = upper;
        }
    }

    size_t result = costs[n];

    return result;
}

template <class seq_type>
std::pair<std::vector<int>, std::vector<int>> full_edit_distance(const std::vector<seq_type> &s1,
                                                                 const std::vector<seq_type> &s2) {
    Timer timer("edit_distance");
    const size_t m(s1.size());
    const size_t n(s2.size());

    if (m == 0)
        return {};
    if (n == 0)
        return {};

    Vec2D<int> costs = new2D<int>(m + 1, n + 1);

    for (size_t k = 0; k <= n; k++)
        costs[0][k] = k;

    size_t i = 0;
    for (auto it1 = s1.begin(); it1 != s1.end(); ++it1, ++i) {
        costs[i + 1][0] = i + 1;

        size_t j = 0;
        for (auto it2 = s2.begin(); it2 != s2.end(); ++it2, ++j) {
            if (*it1 == *it2) {
                costs[i + 1][j + 1] = costs[i][j];
            } else {
                costs[i + 1][j + 1]
                        = std::min(costs[i][j], std::min(costs[i][j + 1], costs[i + 1][j])) + 1;
            }
        }
    }

    std::vector<int> v1, v2;
    for (int i = m, j = n; i > 0 && j > 0;) {
        if (costs[i][j] == costs[i - 1][j - 1]) {
            v1.push_back(i);
            v2.push_back(j);
            --i, --j;
        } else if (costs[i][j] == costs[i - 1][j - 1] + 1) {
            --i, --j;
        } else if (costs[i][j] == costs[i - 1][j] + 1) {
            --i;
        } else if (costs[i][j] == costs[i][j - 1] + 1) {
            --j;
        } else {
            assert(false);
        }
    }

    std::reverse(v1.begin(), v1.end());
    std::reverse(v2.begin(), v2.end());
    return { std::move(v1), std::move(v2) };
}


// Computes the lengths of runs of consecutive matches, i.e. the distances between consecutive
// mutations.
// TODO: Add some tests for this.
template <class seq_type>
std::vector<int> matches_to_distance(const std::vector<seq_type> &s,
                                     const std::vector<int> &matches) {
    std::vector<int> distances;
    int run_length = 0;
    int last_match = -1;
    for (auto x : matches) {
        if (x == last_match + 1) {
            run_length += 1;
        } else {
            // abort current run, and add additional 0s for consecutive non-matches.
            distances.push_back(run_length);
            for (int i = 0; i < x - last_match - 2; ++i)
                distances.push_back(0);
            run_length = 1;
        }
        last_match = x;
    }
    distances.push_back(run_length);
    for (int i = 0; i < int(s.size()) - last_match - 1; ++i)
        distances.push_back(0);

    return distances;
}


template <class seq_type>
std::pair<std::vector<int>, std::vector<int>> mutation_distances(const std::vector<seq_type> &s1,
                                                                 const std::vector<seq_type> &s2) {
    auto [v1, v2] = full_edit_distance(s1, s2);
    return { matches_to_distance(s1, v1), matches_to_distance(s2, v2) };
}


template <class T, class = is_u_integral<T>>
T int_pow(T x, T pow) {
    T result = 1;
    for (;;) {
        if (pow & 1)
            result *= x;
        pow >>= 1;
        if (!pow)
            break;
        x *= x;
    }

    return result;
}

std::string
flag_values(char delimiter = ' ', bool skip_empty = false, bool include_flagfile = true);

// If the -o output flag is set, this writes a small shell script <output>.meta containing the
// command line used to generate the output.
void write_output_meta();

// A simple wrapper around std::apply that applies a given lambda on each element of a tuple.
template <typename F, typename T>
void apply_tuple(F &&f, T &tuple_t) {
    std::apply([&](auto &...t) { (f(t), ...); }, tuple_t);
}


// A simple wrapper around std::apply that applies f on pairs of elements of two tuples.
template <typename F, typename T, typename U>
void apply_tuple(F &&f, T &tuple_t, U &tuple_u) {
    std::apply([&](auto &...t) { std::apply([&](auto &...u) { (f(t, u), ...); }, tuple_u); },
               tuple_t);
}


std::pair<double, double> avg_stddev(const std::vector<double> &v);

// v must be sorted.
double median(const std::vector<double> &v);

} // namespace ts
