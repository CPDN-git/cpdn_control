#include <chrono>
#include <csignal>
#include <cstdlib>
#include <string>
#include <thread>

int main()
{
    const char* signal_mode = std::getenv("CPDN_LAUNCH_PROCESS_SIGNAL");
    if (signal_mode && std::string(signal_mode) == "TERM") {
        std::this_thread::sleep_for(std::chrono::milliseconds(500));
        std::raise(SIGTERM);
        return 1;
    }

    std::this_thread::sleep_for(std::chrono::seconds(5));
    return 0;
}
