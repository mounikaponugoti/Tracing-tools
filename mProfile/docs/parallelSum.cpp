//parallelSum.cpp
//Compile: g++ parallelSum.cpp -o parallelSum -fopenmp
//Run: ./parallelSum [number of threads]
#include <iostream>
#include <cstdlib>
#include <sstream>
#include <omp.h>

#define SIZE 1000000

int main(int argc, char* argv[]) {
    int *arr = new int[SIZE];
    int numThreads = 1;
    long long int i, sum = 0;
    for (i = 0; i < SIZE; i++) {
        arr[i] = i;
    }
    if (argc == 2) {
        std::istringstream ss(argv[1]);
        ss >> numThreads;
    }
    if (argc > 2) {
        std::cout << "Usage: ./parallelSum [number of threads]" << std::endl;
        exit(1);
    }
    omp_set_num_threads(numThreads);
#pragma omp parallel for reduction(+:sum)
    for (i = 0; i < SIZE; i++) {
        sum += arr[i];
    }
    std::cout << "Sum: " << sum << std::endl;
}
