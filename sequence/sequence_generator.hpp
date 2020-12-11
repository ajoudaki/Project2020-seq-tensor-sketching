#pragma once

#include "util/utils.hpp"

#include <algorithm>
#include <functional>
#include <iostream>
#include <memory>
#include <random>

namespace ts { // ts = Tensor Sketch

class SeqGen {
  public:
    SeqGen(uint8_t alphabet_size,
           bool fix_len,
           uint32_t max_num_blocks,
           uint32_t min_num_blocks,
           uint32_t num_seqs,
           uint32_t seq_len,
           float mutation_rate,
           float block_mutation_rate)
        : alphabet_size(alphabet_size),
          fix_len(fix_len),
          max_num_blocks(max_num_blocks),
          min_num_blocks(min_num_blocks),
          num_seqs(num_seqs),
          seq_len(seq_len),
          mutation_rate(mutation_rate),
          block_mutate_rate(block_mutation_rate) {}

    /**
     * Generate sequences in a linear fashion, ie s1->s2, s2->s3, ...
     * @tparam T sequence type
     */
    template <class T>
    Vec2D<T> genseqs_linear() {
        Vec2D<T> seqs(num_seqs);
        gen_seq(seqs[0]);
        for (uint32_t si = 1; si < num_seqs; si++) {
            point_mutate(seqs[si - 1], seqs[si]);
            block_permute(seqs[si]);
            if (fix_len)
                make_fix_len(seqs[si]);
        }
        return seqs;
    }
    template <class T>
    Vec2D<T> genseqs_pairs() {
        Vec2D<T> seqs(num_seqs);
        assert(num_seqs % 2 == 0);
        for (size_t si = 0; si < seqs.size(); si++) {
            gen_seq(seqs[si]);
        }
        for (uint32_t si = 0; si < num_seqs; si += 2) {
            int lcs = si * seq_len / num_seqs;
            std::vector<int> perm(seq_len), perm2(seq_len);
            std::iota(perm.begin(), perm.end(), 0);
            std::shuffle(perm.begin(), perm.end(), gen);
            std::iota(perm2.begin(), perm2.end(), 0);
            std::shuffle(perm2.begin(), perm2.end(), gen);

            std::sort(perm.begin(), perm.begin() + lcs);
            std::sort(perm2.begin(), perm2.begin() + lcs);
            for (int i = 0; i < lcs; i++) {
                seqs[si][perm[i]] = seqs[si + 1][perm2[i]];
            }
        }
        return seqs;
    }


    template <class T>
    Vec2D<T> genseqs_tree(int sequence_seeds) {
        Vec2D<T> seqs(sequence_seeds);
        for (int i = 0; i < sequence_seeds; i++) {
            gen_seq(seqs[i]);
        }
        std::vector<std::vector<T>> children;
        while (seqs.size() < num_seqs) {
            for (auto &seq : seqs) {
                std::vector<T> ch1, ch2;
                point_mutate(seq, ch1);
                block_permute(ch1);
                point_mutate(seq, ch2);
                block_permute(ch2);
                ch1 = seq;
                children.push_back(ch1);
                children.push_back(ch2);
            }
            std::swap(seqs, children);
        }
        seqs.resize(num_seqs);
        for (auto &seq : seqs)
            if (fix_len)
                make_fix_len(seq);
        return seqs;
    }


    template <class T>
    Vec2D<T> genseqs_tree2() {
        // TODO get this to get input from command line
        Vec2D<T> seqs = Vec2D<T>(1);
        gen_seq(seqs[0]);

        std::vector<std::vector<T>> children;
        while (seqs.size() < num_seqs) {
            for (auto &seq : seqs) {
                std::vector<T> child(seq);
                children.push_back(seq);
                children.push_back(child);
            }
            std::swap(seqs, children);
        }
        seqs.resize(num_seqs);
        for (auto &seq : seqs)
            if (fix_len)
                make_fix_len(seq);
    }

  private:
    template <class T>
    void block_permute(std::vector<T> &seq) {
        std::uniform_real_distribution<double> mute(0, 1);
        if (mute(gen) > block_mutate_rate) {
            return;
        }
        std::uniform_int_distribution<T> unif(0, alphabet_size - 1);
        std::uniform_int_distribution<T> blocks(min_num_blocks, max_num_blocks);
        int num_blocks = blocks(gen);
        std::vector<size_t> perm(num_blocks);
        std::iota(perm.begin(), perm.end(), 0);
        std::shuffle(perm.begin(), perm.end(), gen);
        while (seq.size() % num_blocks != 0) { // make length divisible by num_blocks
            seq.push_back(unif(gen));
        }
        std::vector<T> result(seq.size());
        size_t block_size = seq.size() / num_blocks;
        for (size_t i = 0; i < block_size; i++) {
            for (int pi = 0; pi < num_blocks; pi++) {
                size_t bi = pi * block_size + i;
                size_t bj = perm[pi] * block_size + i;
                result[bj] = seq[bi];
            }
        }
        seq = result;
    }

    template <class T>
    void gen_seq(std::vector<T> &seq) {
        seq.clear();
        std::uniform_int_distribution<T> unif(0, alphabet_size - 1);
        for (uint32_t i = 0; i < seq_len; i++) {
            seq.push_back(unif(gen));
        }
    }

    template <class T>
    void point_mutate(const std::vector<T> &ref, std::vector<T> &seq) {
        float rate = mutation_rate;
        std::discrete_distribution<int> mut { 1 - rate, rate / 3, rate / 3, rate / 3 };
        std::uniform_int_distribution<T> unif(0, alphabet_size - 2);
        // TODO: add alignment
        for (size_t i = 0; i < ref.size(); i++) {
            switch (mut(gen)) {
                case 0: { // no mutation
                    seq.push_back(ref[i]);
                    break;
                }
                case 1: { // insert
                    seq.push_back(unif(gen));
                    i--; // init_tensor_slide_params negate the increment
                    break;
                }
                case 2: { // delete
                    break;
                }
                case 3: { // substitute
                    auto c = unif(gen);
                    c = (c >= ref[i]) ? c + 1 : c; // increment if not changed
                    seq.push_back(c);
                    break;
                }
            }
        }
    }

    template <class T>
    void random_edit(const std::vector<T> &ref) {
        std::discrete_distribution<int> mut { 1.0 / 3, 1.0 / 3, 1.0 };
        std::uniform_int_distribution<T> rchar(0, alphabet_size - 1);
        std::uniform_int_distribution<size_t> rpos_inc(
                0, seq_len); // inclusinve of seq_len, insertion to the very end
        std::uniform_int_distribution<size_t> rpos_exc(
                0, seq_len - 1); // inclusinve of seq_len, insertion to the very end
        switch (mut(gen)) {
            case 0: { // insert
                rpos_inc(gen);
                auto c = rchar(gen);
                ref.insert(ref.begin(), c);
                break;
            }
            case 1: { // delete
                rpos_exc(gen);
                break;
            }
            case 2: { // substitute
                auto pos = rpos_exc(gen);
                auto c = rchar(gen);
                if (c == ref[pos]) {
                    c++;
                    c = (c % seq_len);
                }
                ref[pos] = c;
                break;
            }
        }
    }

    template <class T>
    void make_fix_len(std::vector<T> &seq) {
        std::uniform_int_distribution<T> unif(0, alphabet_size - 1),
                blocks(min_num_blocks, max_num_blocks);
        if (seq.size() > seq_len) {
            seq = std::vector<T>(seq.begin(), seq.end());
        } else if (seq.size() < seq_len) {
            while (seq.size() < seq_len) {
                seq.push_back(unif(gen));
            }
        }
    }

  private:
    std::random_device rd;
    std::mt19937 gen = std::mt19937(rd());

    uint8_t alphabet_size;
    bool fix_len;
    uint32_t max_num_blocks;
    uint32_t min_num_blocks;
    uint32_t num_seqs;
    uint32_t seq_len;
    float mutation_rate;
    float block_mutate_rate;
};

} // namespace ts
