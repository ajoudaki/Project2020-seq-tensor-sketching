#include "sequence/fasta_io.hpp"
#include "util/multivec.hpp"
#include "util/utils.hpp"

#include <gflags/gflags.h>

#include <filesystem>
#include <memory>
#include <random>
#include <sstream>
#include <utility>
#include "util/progress.hpp"

DEFINE_string(o, "", "Output file");

DEFINE_string(i, "", "Input directory");

DEFINE_int32(t, 4, "Thread count");

DEFINE_int32(b, 32, "Block size");

static bool ValidateInput(const char * /*unused*/, const std::string &value) {
    if (!value.empty()) {
        return true;
    }
    std::cerr << "Please specify a fasta input file using '-i <input_file>'" << std::endl;
    return false;
}
DEFINE_validator(i, &ValidateInput);


using namespace ts;

namespace fs = std::filesystem;
using Tensor23 = std::pair<std::vector<double>, std::vector<double>>;
Tensor23 tensor23(std::vector<uint8_t> &seq) {
    Tensor23 result({ std::vector<double>(16, 0), std::vector<double>(64, 0) });
    std::transform(seq.begin(), seq.end(), seq.begin(), [](uint8_t a) { return (a > 3) ? 0 : a; });
    for (uint32_t i = 0; i < seq.size(); ++i) {
        const uint32_t idx1 = 16 * seq[i];
        for (uint32_t j = i + 1; j < seq.size(); ++j) {
            const uint32_t idx2 = idx1 + 4 * seq[j];
            for (uint32_t k = j + 1; k < seq.size(); ++k) {
                result.second[idx2 + seq[k]]++;
            }
            result.first[(seq[i] << 2) + seq[j]]++;
        }
    }
    double l1_t2 = std::accumulate(result.first.begin(), result.first.end(),decltype(result.first)::value_type(0));
    for (auto &e : result.first)  e = e / l1_t2;

    double l1_t3 = std::accumulate(result.second.begin(), result.second.end(),decltype(result.second)::value_type(0));
    for (auto &e : result.second)   e = e / l1_t3;

    return result;
}

int main(int argc, char *argv[]) {
    gflags::ParseCommandLineFlags(&argc, &argv, true);
    init_alphabet("DNA4");

    std::vector<std::string> names;
    for (auto &p : fs::directory_iterator(FLAGS_i)) {
        if (p.is_directory()
            || (p.path().filename().string().find(".fa") == std::string::npos
                && p.path().filename().string().find(".fna.gz") == std::string::npos)) {
            continue;
        }
        names.push_back(p.path().string());
    }
    std::cout << "Found " << names.size() << " fasta files" << std::endl;
    std::vector<std::pair<std::vector<uint8_t>, std::string>> sequences(names.size());
#pragma omp parallel for num_threads(FLAGS_t)
    for (uint32_t i = 0; i < names.size(); ++i) {
        FastaFile<uint8_t> ff = read_fasta<uint8_t>(names[i], "fasta");
        sequences[i] = { ff.sequences[0], fs::path(names[i]).filename() };
    }
    std::sort(sequences.begin(), sequences.end(),
              [](auto &a, auto &b) { return a.first.size() < b.first.size(); }); // sort the strings to length
    std::cout << "Read " << sequences.size() << " fasta files" << std::endl;

    if (sequences.empty()) {
        std::exit(0);
    }

    fs::remove_all(FLAGS_o);
    const size_t BLOCK_SIZE = FLAGS_b;
    std::vector<Tensor23> tensors(sequences.size());
    std::ofstream f(FLAGS_o + "_ed");
    f << "seq1,seq2,len1,len2,Ten2,Ten3,ED" << std::endl;

    for (uint32_t epoch = 0; epoch < (sequences.size() - 1) / BLOCK_SIZE + 1; ++epoch) {
        size_t max_ind = std::min((epoch + 1) * BLOCK_SIZE, sequences.size());
        std::cout << "epoch " << epoch << std::endl;
        std::cout << "computing sketches ... " << std::flush;
        progress_bar::init(max_ind - epoch * BLOCK_SIZE );
#pragma omp parallel for num_threads(FLAGS_t)
        for (uint32_t i = epoch * BLOCK_SIZE; i < max_ind; ++i) {
            progress_bar::iter();
            tensors[i] = tensor23(sequences[i].first);
        }

        std::cout << "computing distances ... " << std::flush;
        Vec3D<double> dists = new3D<double>(BLOCK_SIZE, BLOCK_SIZE, 7, 0);
        progress_bar::init(max_ind * (max_ind-1) /2 );
#pragma omp parallel for num_threads(FLAGS_t)
        for (uint32_t j = epoch * BLOCK_SIZE; j < max_ind; ++j) {
            for (uint32_t k = j + 1; k < max_ind; ++k) {
                progress_bar::iter();
                auto &row = dists[j-epoch*BLOCK_SIZE][k-BLOCK_SIZE * epoch];
                row[0] = j;
                row[1] = k;
                row[2] = sequences[j].first.size();
                row[3] = sequences[k].first.size();
                row[4] = l2_dist(tensors[j].first, tensors[k].first);
                row[5] = l2_dist(tensors[j].second, tensors[k].second);
                row[6] = edit_distance(sequences[j].first, sequences[k].first);
            }
        }
        for (auto &arr : dists) {
            for (auto & row : arr) {
                if (row[0] >= row[1])
                    continue;

                for (size_t i = 0; i<7; i++ ) {
                    if (i<4)
                        f << (int)(row[i]) << ",";
                    else if (i<=5)
                        f << row[i] << ",";
                    else if (i==6)
                        f << row[i] << std::endl;
                }
            }
        }
        std::cout << "Epoch " << epoch << " done." << std::endl;
    }
}
