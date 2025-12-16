// Test to check cpdn_linux_cpu_time against the original BOINC implementation.
// The boinc impplmentation can be found in lib/util.cpp and is 
// duplicated here to avoid including the BOINC header and linking against BOINC libraries.
// Our version uses sysconf to get the clock ticks per second dynamically and should 
// provide a more accurate CPU time calculation. It's a drop-in replacement for the BOINC version.
//
//   Glenn Carver, CPDN, 2025.

#include <iostream>
#include <fstream>
#include <string>
#include <sstream>
#include <chrono>
#include <thread>
#include <unistd.h> // For getpid(), sysconf(_SC_CLK_TCK)

#include "unit_tests.h"
#include "../lib/cpdn_linux_cpu_time.h"

// --- BOINC Implementation
double boinc_cpu_time(int pid) { 
    FILE* file;
    char file_name[24];
    unsigned long utime = 0, stime = 0;
    int n;

    snprintf(file_name, sizeof(file_name), "/proc/%d/stat", pid);
    if ((file = fopen(file_name, "r")) != NULL) {
        // Skips the first 13 fields (including PID, command, state, parent PID, etc.)
        // and reads the 14th (utime) and 15th (stime) fields.
        n = fscanf(file, "%*s%*s%*s%*s%*s%*s%*s%*s%*s%*s%*s%*s%*s%lu%lu", &utime, &stime);
        fclose(file);
        if (n != 2) return 0;
    }
    // Note: The original BOINC code used / 100 which is a hardcoded clock tick approximation.
    return (double)(utime + stime) / 100;
}


int t_cputime_comparison()
{
    TEST("t_cputime_comparison");

    // Use the current process' ID (guaranteed to exist) for the test
    auto current_pid = getpid(); 

    std::cout << "--- CPU Time Comparison Test ---\n";
    std::cout << "Target PID: " << current_pid << "\n";

    auto test_time = 2.5;   // seconds
    std::cout << "Do something for " << test_time << " seconds to accumulate CPU time..." << std::endl;
    // Simple busy-wait loop to accumulate CPU time
    auto start_time = std::chrono::high_resolution_clock::now();
    while (true) {
        auto elapsed = std::chrono::high_resolution_clock::now() - start_time;
        if (std::chrono::duration_cast<std::chrono::milliseconds>(elapsed).count() >= test_time*1000) {
            break;
        }
    }
    
    auto cpdn_time  = cpdn_linux_cpu_time(current_pid);
    auto boinc_time = boinc_cpu_time(current_pid);
    auto delta      = cpdn_time - boinc_time;
    
    std::cout << "--------------------------------" <<
                 "\nCPDN CPU Time (seconds): " << cpdn_time <<
                 "\nBOINC CPU Time (seconds): " << boinc_time <<
                 "\nSystem clock resolution (ticks/sec): " << sysconf(_SC_CLK_TCK) <<
                 "\nDifference (CPDN - BOINC): " << delta << std::endl;
    
    std::cout << "--------------------------------" << std::endl;
    
    // Simple check to ensure they are close (allowing for the difference in clock resolution)
    if (std::abs(delta) < 0.01) {
        std::cout << "RESULT: Implementations agree within 10 milliseconds." << std::endl;
        SUCCESS; return EXIT_SUCCESS;
    } else {
        std::cout << "RESULT: Implementations show a noticeable difference. Check clock resolution." << std::endl;
        // In a real test, you might return 1 here if you expected them to be closer.
        FAIL; return EXIT_FAILURE; 
    }
}
