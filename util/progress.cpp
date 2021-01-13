//
// Created by amir on 1/9/21.
//
#include <iostream>
#include "util/progress.hpp"

namespace ts {

size_t progress_bar::it;
size_t progress_bar::total;
size_t progress_bar::bar_len;
size_t progress_bar::bar_step;

void progress_bar::init(size_t total_iterations, size_t len ) {
    progress_bar::it = 0;
    progress_bar::total = total_iterations;
    progress_bar::bar_len = len;
    progress_bar::bar_step = 0;
}

void progress_bar::iter() {

#pragma omp critical
    {
        it++;
        auto step = (it * bar_len ) / total;
        while (step > bar_step)  {
            bar_step ++;
            std::cout << "#" << std::flush;
        }
    }
}

};


