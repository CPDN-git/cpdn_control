#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <string>
#include <thread>

namespace fs = std::filesystem;

namespace {
int get_env_int( const char* name, int default_value )
{
    const char* value = std::getenv( name );
    if ( value == nullptr ) {
        return default_value;
    }
    return std::stoi( value );
}
}    // namespace

int main()
{
    std::this_thread::sleep_for( std::chrono::milliseconds( get_env_int( "CPDN_TIMED_PROCESS_SLEEP_MS", 0 ) ) );

    const char* output_name = std::getenv( "CPDN_TIMED_PROCESS_WRITE_FILE" );
    if ( output_name != nullptr && std::string( output_name ).empty() == false ) {
        fs::path output_path = fs::current_path() / output_name;
        std::ofstream output_file( output_path );
        output_file << "timed_process_helper output\n";
    }

    return get_env_int( "CPDN_TIMED_PROCESS_EXIT_CODE", 0 );
}
