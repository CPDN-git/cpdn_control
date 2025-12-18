//
// Reimplementation of the BOINC linux_cpu_time function using modern C++.
// This version checks the clock resolution to ensure accurate CPU time calculation,
// unlike the original BOINC implementation which used a hardcoded value.
//  This code is cross-platform and works on Linux, macOS, and Windows (with help from Codex)
//
//   Glenn Carver, CPDN, 2025.

#include <iostream>
#include <fstream>
#include <string>
#include <sstream>
#include <cstdint>

#if defined(__APPLE__)
  #include <libproc.h>
#elif defined(_WIN32)
  #include <windows.h>
#else
  #include <unistd.h> // Required for sysconf(_SC_CLK_TCK)
#endif


// Define the function outside of a class for direct replacement of the original
double cpdn_cpu_time(long pid) {

#if defined(__APPLE__)
    // Use proc_pid_rusage to obtain CPU times (nanoseconds) for the given pid.
    rusage_info_v2 ri{};
    if (proc_pid_rusage(static_cast<int>(pid), RUSAGE_INFO_V2, reinterpret_cast<rusage_info_t*>(&ri)) != 0) {
        return 0.0;
    }
    constexpr double NS_PER_SEC = 1'000'000'000.0;
    return (static_cast<double>(ri.ri_user_time) + static_cast<double>(ri.ri_system_time)) / NS_PER_SEC;

#elif defined(_WIN32)
    HANDLE process = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION | PROCESS_QUERY_INFORMATION, FALSE, static_cast<DWORD>(pid));
    if (!process) {
        return 0.0;
    }

    FILETIME create_time{}, exit_time{}, kernel_time{}, user_time{};
    if (!GetProcessTimes(process, &create_time, &exit_time, &kernel_time, &user_time)) {
        CloseHandle(process);
        return 0.0;
    }

    ULARGE_INTEGER k{}, u{};
    k.HighPart = kernel_time.dwHighDateTime;
    k.LowPart  = kernel_time.dwLowDateTime;
    u.HighPart = user_time.dwHighDateTime;
    u.LowPart  = user_time.dwLowDateTime;

    CloseHandle(process);

    // FILETIME units are 100-ns.
    constexpr double HNS_PER_SEC = 10'000'000.0;
    return (static_cast<double>(k.QuadPart) + static_cast<double>(u.QuadPart)) / HNS_PER_SEC;

// Linux
#else
    std::ifstream file;
    std::string line;
    std::string file_path = "/proc/" + std::to_string(pid) + "/stat";

    // file.close() is implicitly called by the std::ifstream destructor (RAII)
    file.open(file_path);

    if (!file.is_open()) {
        return 0.0;             // Process might not exist or permission denied
    }

    // Read and parse the stat line using a stringstream.
    // The stat file contains 52 fields. We need the 14th (utime) and 15th (stime).
    // Must skip first 13 fields (including the command name which is field 2, enclosed in parentheses) 
    if (!std::getline(file, line)) {
        return 0.0;
    }
    std::stringstream ss(line);
    std::string       temp_field;
    unsigned long     utime = 0, stime = 0;
    
    // The first field is PID, which we don't need to read.
    // The second field is the command name, which can contain spaces, making parsing difficult.
    // A robust way to skip the first 13 fields:
    // Skip 13 fields to get to utime: 
    // Field 1: PID
    // Field 2: Command name (tricky, so we use a loop and carefully handle it)
    for (int i = 0; i < 13; ++i) {
        ss >> temp_field;
    }
    if (!(ss >> utime >> stime)) {
        return 0.0; 
    }

    // Determine the correct clock ticks per second
    // sysconf(_SC_CLK_TCK) retrieves the number of clock ticks per second (usually 100 or 1000).
    long ticks_per_second = sysconf(_SC_CLK_TCK);
    if (ticks_per_second <= 0) {
        ticks_per_second = 100;             // Fallback to a default value if sysconf fails     
    }

    // Calculate total CPU time in seconds
    unsigned long total_ticks = utime + stime;
    double total_cpu_time = static_cast<double>(total_ticks) / ticks_per_second;

    return total_cpu_time;
#endif
}
