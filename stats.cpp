#include "stats.hpp"
#include <fstream>
#include <string>
#include <sstream>
#include <vector>
#include <unistd.h>

// Helper function to get CPU times from /proc/stat
static CPU_Times get_cpu_times() {
    std::ifstream proc_stat("/proc/stat");
    std::string line;
    std::getline(proc_stat, line);
    
    std::string cpu_label;
    CPU_Times times = {};
    
    std::stringstream ss(line);
    ss >> cpu_label >> times.user >> times.nice >> times.system >> times.idle;

    return times;
}

double get_cpu_usage() {
    // We need two samples to calculate a percentage
    static CPU_Times last_times = {0, 0, 0, 0};
    
    CPU_Times current_times = get_cpu_times();

    long long last_idle = last_times.idle;
    long long last_total = last_times.user + last_times.nice + last_times.system + last_times.idle;

    long long current_idle = current_times.idle;
    long long current_total = current_times.user + current_times.nice + current_times.system + current_times.idle;

    long long total_diff = current_total - last_total;
    long long idle_diff = current_idle - last_idle;

    last_times = current_times;

    if (total_diff == 0) {
        return 0.0;
    }

    return 100.0 * (1.0 - (double)idle_diff / (double)total_diff);
}