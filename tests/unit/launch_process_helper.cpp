#include <chrono>
#include <cstdlib>
#include <string>
#include <thread>

int main()
{
    int sleep_ms = 1000;
    if ( const char* sleep_text = std::getenv( "CPDN_LAUNCH_PROCESS_SLEEP_MS" ) ) {
        try {
            sleep_ms = std::stoi( sleep_text );
        } catch ( ... ) {
            sleep_ms = 1000;
        }
    }

    std::this_thread::sleep_for( std::chrono::milliseconds( sleep_ms ) );
    return 0;
}
