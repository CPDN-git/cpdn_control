#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
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
    const char* check_var_name = std::getenv( "CPDN_TIMED_PROCESS_CHECK_VAR_NAME" );
    if ( check_var_name != nullptr && std::string( check_var_name ).empty() == false ) {
        const char* expected_value = std::getenv( "CPDN_TIMED_PROCESS_CHECK_VAR_VALUE" );
        const char* actual_value = std::getenv( check_var_name );
        if ( actual_value == nullptr ) {
            return 9;
        }
        if ( expected_value != nullptr && std::string( actual_value ) != expected_value ) {
            return 10;
        }
    }

    std::this_thread::sleep_for( std::chrono::milliseconds( get_env_int( "CPDN_TIMED_PROCESS_SLEEP_MS", 0 ) ) );

    const char* output_name = std::getenv( "CPDN_TIMED_PROCESS_WRITE_FILE" );
    if ( output_name != nullptr && std::string( output_name ).empty() == false ) {
        fs::path output_path = fs::current_path() / output_name;
        std::ofstream output_file( output_path );
        output_file << "timed_process_helper output\n";
    }

    const char* stdout_text = std::getenv( "CPDN_TIMED_PROCESS_STDOUT_TEXT" );
    if ( stdout_text != nullptr && std::string( stdout_text ).empty() == false ) {
        std::cout << stdout_text << '\n';
    }

    const char* stderr_text = std::getenv( "CPDN_TIMED_PROCESS_STDERR_TEXT" );
    if ( stderr_text != nullptr && std::string( stderr_text ).empty() == false ) {
        std::cerr << stderr_text << '\n';
    }

    return get_env_int( "CPDN_TIMED_PROCESS_EXIT_CODE", 0 );
}
