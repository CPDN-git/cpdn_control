//  C function to do a simple check if a process is running.
//
//      Glenn Carver, CPDN,   May/2026
//      Initially coded by Gemini

#include <errno.h>

#if defined(_WIN32) || defined(_WIN64)
#include <windows.h>
int cpdn_is_process_running(int pid) {
    HANDLE hProcess = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, (DWORD)pid);
    if (hProcess) {
        DWORD exitCode;
        if (GetExitCodeProcess(hProcess, &exitCode)) {
            CloseHandle(hProcess);
            return (exitCode == STILL_ACTIVE);
        }
        CloseHandle(hProcess);
    }
    return 0;
}
#else
#include <signal.h>
int cpdn_is_process_running(int pid) {
    // kill with signal 0 checks for existence without sending a signal
    if (kill(pid, 0) == 0) return 1;
    // EPERM means it exists but you don't have permission to signal it
    if (errno == EPERM) return 1;
    return 0;
}
#endif
