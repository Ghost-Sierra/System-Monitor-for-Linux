#ifndef STATS_HPP
#define STATS_HPP

#include <string>

// Structure to hold CPU time data from /proc/stat
struct CPU_Times {
    long long user;
    long long nice;
    long long system;
    long long idle;
};

// Function to get the current CPU usage percentage
// This needs to be called periodically to be meaningful
double get_cpu_usage();

#endif // STATS_HPP