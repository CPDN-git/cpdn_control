#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <thread>

#ifdef _WIN32
#include <Windows.h>
#else
#include <unistd.h>
#endif

int main( int argc, char** argv )
{
    if ( const char* ready_file = std::getenv( "CPDN_LAUNCH_PROCESS_READY_FILE" ) ) {
#ifdef _WIN32
        std::ofstream( ready_file ) << GetCurrentProcessId() << '\n';
#else
        std::ofstream( ready_file ) << getpid() << '\n';
#endif
    }
    // Descendant cases exercise ownership even after the original leader exits.
    if ( const char* pid_file = std::getenv( "CPDN_LAUNCH_PROCESS_DESCENDANT_FILE" ) ) {
        if ( argc > 1 ) {
#ifdef _WIN32
            const auto pid = GetCurrentProcessId();
#else
            const auto pid = getpid();
#endif
            std::ofstream( pid_file ) << pid << '\n';
            std::this_thread::sleep_for( std::chrono::seconds( 30 ) );
            return 0;
        }
#ifdef _WIN32
        std::string command = "\"" + std::string( argv[0] ) + "\" --descendant";
        STARTUPINFOA startup{};
        startup.cb = sizeof( startup );
        PROCESS_INFORMATION process{};
        if ( !CreateProcessA( argv[0], command.data(), nullptr, nullptr, FALSE, 0, nullptr, nullptr, &startup, &process ) ) {
            return 2;
        }
        CloseHandle( process.hThread );
        CloseHandle( process.hProcess );
#else
        const pid_t pid = fork();
        if ( pid < 0 ) {
            return 2;
        }
        if ( pid == 0 ) {
            execl( argv[0], argv[0], "--descendant", nullptr );
            _exit( 127 );
        }
#endif
        const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds( 5 );
        while ( !std::filesystem::exists( pid_file ) && std::chrono::steady_clock::now() < deadline ) {
            std::this_thread::sleep_for( std::chrono::milliseconds( 10 ) );
        }
        if ( std::getenv( "CPDN_LAUNCH_PROCESS_LEADER_EXIT" ) ) {
            return 0;
        }
    }
    int sleep_ms = 1000;
    if ( const char* sleep_text = std::getenv( "CPDN_LAUNCH_PROCESS_SLEEP_MS" ) ) {
        try {
            sleep_ms = std::stoi( sleep_text );
        } catch ( ... ) {
            sleep_ms = 1000;
        }
    }

    if ( const char* stdout_text = std::getenv( "CPDN_LAUNCH_PROCESS_STDOUT_TEXT" ) ) {
        std::cout << stdout_text << '\n';
    }
    if ( const char* stderr_text = std::getenv( "CPDN_LAUNCH_PROCESS_STDERR_TEXT" ) ) {
        std::cerr << stderr_text << '\n';
    }

    std::this_thread::sleep_for( std::chrono::milliseconds( sleep_ms ) );
    return 0;
}
