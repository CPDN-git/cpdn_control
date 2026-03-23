/**
 * Utility/library functions for the CPDN task controller
 * 
 *    Glenn Carver, CPDN, 2025.
 */

#include <algorithm>
#include <cctype>
#include <cerrno>
#include <charconv>    // for std::from_chars
#include <chrono>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <limits>
#include <optional>
#include <string>
#include <thread>
#include <vector>

#if defined( _WIN32 ) || defined( _WIN64 )
#include <Windows.h>    // for SetFileAttributes
#else
#include <csignal>
#include <sys/wait.h>
#include <unistd.h>    // for access
#endif
#include <sys/stat.h>    // for chmod

#include "cpdn_cpu_time.h"
#include "utils.h"

namespace fs = std::filesystem;

// Anonymous namespacw to give file-local internal linkage to helper functions: get_file_mtime, file_was_updated, and
// terminate_child_process in lib/utils.cpp. Without it, those functions would have external linkage by default,
// meaning they would be visible outside that translation unit and could collide with similarly named helpers
// elsewhere.The anonymous namespace makes it explicit that they are private implementation details of utils.cpp.

namespace {

std::optional<fs::file_time_type> get_file_mtime( const fs::path& path )
{
    std::error_code ec;
    if ( !fs::exists( path, ec ) || ec ) {
        return std::nullopt;
    }

    auto write_time = fs::last_write_time( path, ec );
    if ( ec ) {
        return std::nullopt;
    }
    return write_time;
}

bool file_was_updated( const fs::path& path, const std::optional<fs::file_time_type>& previous_mtime )
{
    auto current_mtime = get_file_mtime( path );
    if ( !current_mtime.has_value() ) {
        return false;
    }

    if ( !previous_mtime.has_value() ) {
        return true;
    }

    return current_mtime.value() != previous_mtime.value();
}

#if !defined( _WIN32 ) && !defined( _WIN64 )
bool terminate_child_process( pid_t pid )
{
    int status = 0;
    pid_t wait_result = waitpid( pid, &status, WNOHANG );
    if ( wait_result == pid ) {
        return true;
    }
    if ( wait_result == -1 ) {
        return false;
    }

    constexpr auto kGracePeriod = std::chrono::seconds( 2 );
    auto deadline = std::chrono::steady_clock::now() + kGracePeriod;

    if ( kill( pid, SIGTERM ) != 0 && errno != ESRCH ) {
        return false;
    }

    while ( std::chrono::steady_clock::now() < deadline ) {
        wait_result = waitpid( pid, &status, WNOHANG );
        if ( wait_result == pid ) {
            return true;
        }
        if ( wait_result == -1 ) {
            return false;
        }
        std::this_thread::sleep_for( std::chrono::milliseconds( 100 ) );
    }

    if ( kill( pid, SIGKILL ) != 0 && errno != ESRCH ) {
        return false;
    }

    wait_result = waitpid( pid, &status, 0 );
    return wait_result == pid || ( wait_result == -1 && errno == ECHILD );
}
#endif

}    // namespace


/** 
 * @brief Sets incoming arg/value as environment variable
 *        Do not use putenv, it stores the pointer of memory passed in (see multiple stackexchange posts on this issue)
 */
bool set_env_var( const std::string& name, const std::string& val )
{
    return ( setenv( name.c_str(), val.c_str(), 1 ) == 0 );    // 1 = overwrite existing value, true on success.
}


/**
 * @brief  Check whether a filesystem path exists (file, directory, or symlink)
 */
bool path_exists( std::string_view pathname )
{
    std::ifstream infile{ std::string( pathname ) };    // GC. C++20 allows string_view directly.
    return infile.good();
}


/** 
 * @brief Check whether file is zero bytes long
 * from: https://stackoverflow.com/questions/2390912/checking-for-an-empty-file-in-c
 * returns True if file is zero bytes, otherwise False.
 */
bool file_is_empty( std::string_view fpath ) { return ( fs::file_size( fpath ) == 0 ); }


/**
 * @brief Set executable permissions on a file
 *        This is a workaround currently as cpdn_unzip does not set unix permissions correctly.
 */
bool set_exec_perms( const std::string& filepath )
{
    // 0755 is a standard permission set:
    // Owner: Read, Write, Execute
    // Group: Read, Execute
    // Others: Read, Execute

#if defined( __unix__ ) || defined( __APPLE__ ) || defined( __linux__ )
    if ( chmod( filepath.c_str(), S_IRWXU | S_IRGRP | S_IXGRP | S_IROTH | S_IXOTH ) != 0 ) {
        return false;
    }
#endif

    return true;
}


/**
 * @brief Trim leading characters from a string in place.
 *        This only strips the left side; rtrim() and trim_whitespace()
 *        handle the right side and both sides respectively.
 */
static void ltrim( std::string& text, const char* trim_chars )
{
    auto start = text.find_first_not_of( trim_chars );
    if ( start == std::string::npos ) {
        text.clear();
        return;
    }
    text.erase( 0, start );
}


/**
 * @brief Trim trailing characters from a string in place.
 *        This only strips the right side; ltrim() and trim_whitespace()
 *        handle the left side and both sides respectively.
 */
static void rtrim( std::string& text, const char* trim_chars )
{
    auto end = text.find_last_not_of( trim_chars );
    if ( end == std::string::npos ) {
        text.clear();
        return;
    }
    text.erase( end + 1 );
}


/**
 * @brief Trim leading and trailing whitespace in place.
 */
void trim_whitespace( std::string& text )
{
    ltrim( text, " \t\n\r" );
    rtrim( text, " \t\n\r" );
}


/**
 * @brief Remove surrounding matching quotes and re-trim whitespace.
 *        This is separate from trim_whitespace() to keep quote handling explicit.
 */
static void strip_quotes_and_trim( std::string& text )
{
    if ( text.size() < 2 ) {
        return;
    }
    if ( ( text.front() == '"' && text.back() == '"' ) || ( text.front() == '\'' && text.back() == '\'' ) ) {
        text = text.substr( 1, text.size() - 2 );
        trim_whitespace( text );
    }
}


/**
 * @brief Split a line into key/value by delimiter with minimal processing.
 *        This trims outer whitespace and validates the delimiter, but does not
 *        apply comment rules or value normalization. Normally called via 
 *        parse_key_value() rather than directly.
 */
static bool split_key_value( const std::string& line, std::string& key, std::string& value, char delimiter )
{
    std::string working_line = line;

    trim_whitespace( working_line );
    if ( working_line.empty() ) {
        return false;
    }

    auto delim_pos = working_line.find( delimiter );
    if ( delim_pos == std::string::npos || delim_pos == 0 ) {
        return false;
    }

    key = working_line.substr( 0, delim_pos );
    value = working_line.substr( delim_pos + 1 );

    trim_whitespace( key );
    trim_whitespace( value );

    return true;
}


/**
 * @brief Attempts to parse a single line as a key/value pair.
 *        Handles common shell formats like "VAR=VALUE".
 *        Lines commented out with # are ignored (but not lines starting with !).
 *
 * @param line      The line of text to parse.
 * @param key       Returned parameter for the key.
 * @param value     Returned parameter for the value.
 * @param delimiter The character separating the key from the value.
 * @return true if successful, false if the line is empty, a comment, or invalid.
 */
bool parse_key_value( const std::string& line, std::string& key, std::string& value, char delimiter )
{
    std::string work_line = line;

    ltrim( work_line, " \t\n\r" );

    if ( work_line.empty() || work_line[0] == '#' ) {
        return false;
    }
    if ( !split_key_value( work_line, key, value, delimiter ) ) {
        return false;
    }
    strip_quotes_and_trim( value );

    return true;
}


bool parse_key_value( const std::string& line, std::string& key, std::string& value ) { return parse_key_value( line, key, value, '=' ); }


/**
 * @brief Parse key/value pairs from a Fortran namelist line.
 *        This treats lines starting with '!' (after whitespace) as comments,
 *        unlike parse_key_value(), which only treats '#' as a comment.
 * 
 *  TODO: Consider adding check for &nam string to indicate valid start to a fortran namelist.
 */
bool parse_namelist_key_value( const std::string& line, std::string& key, std::string& value )
{
    std::string working_line = line;

    ltrim( working_line, " \t\n\r" );
    if ( working_line.empty() || working_line[0] == '!' ) {
        return false;
    }

    // Namelist values can terminate with , or ! followed by comment, so strip these first
    if ( auto comment_pos = working_line.find( '!' ); comment_pos != std::string::npos ) {
        working_line = working_line.substr( 0, comment_pos );
    }
    if ( auto comma_pos = working_line.find( ',' ); comma_pos != std::string::npos ) {
        working_line = working_line.substr( 0, comma_pos );
    }

    return parse_key_value( working_line, key, value );
}


/**
 * @brief Opens a file if exists and uses circular buffer to read & print last lines of file to stderr
 * @return zero : either can't open file or file is empty
 *          > 0  : no. of lines in file (may be less than maxlines)
 */
int print_last_lines( const std::string& filename, const int maxlines )
{
    int count = 0;
    std::vector<std::string> lines( maxlines );

    if ( std::ifstream filein( filename ); filein.is_open() ) {
        while ( getline( filein, lines[count % maxlines] ) )
            count++;
    }

    if ( count > 0 ) {
        // find the oldest lines first in the buffer, will not be at start if count > maxlines
        int start = count > maxlines ? ( count % maxlines ) : 0;
        int end = std::min( maxlines, count );

        std::cerr << "\n~~~~~ Printing last " << end << " lines from file: " << filename << " ~~~~~\n";
        for ( int i = 0; i < end; i++ ) {
            std::cerr << lines[( start + i ) % maxlines] << '\n';
        }
        std::cerr << "~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~\n" << std::endl;
    }

    return count;
}


/**
 * @brief Reads and returns the last line of a file.
 * 
 * This function maintains its state between calls to track the last read position
 * in the file, allowing it to return only new lines added since the last call.
 * It behaves similarly to the 'tail -f' command.
 * 
 * TODO: This function assumes that the file being read is always the same file!
 * Currently, it is as it's always the ifs.stat file but in future we may want to
 * generalize this to handle multiple files by storing state per filename.
 * 
 *     Glenn Carver, CPDN, 2025.
 * 
 * @param fname   The name of the file to read.
 * @param logline Last line read from file; stored between calls.
 * @return        True if a new line was read; returns false and logline unchanged
 *                if no new line was read; returns false and empty logline
 *                if the file does not exist.
 */
bool fread_last_line( const std::string& fname, std::string& logline )
{
    static std::streamoff last_offset = 0;
    static std::string last_line;
    std::string line;
    bool new_line_read = false;

    // Check file exists and non-empty
    std::ifstream logfile( fname, std::ios::in );
    if ( !logfile.is_open() ) {
        logline.clear();
        last_offset = 0;
        last_line.clear();
        std::cerr << ".. fread_last_line(): warning, " << fname << " does not exist." << std::endl;
        return false;
    }

    // Detect file truncation: if our last offset is beyond the current file size,
    // the file has been truncated (e.g., due to model restart). Reset to beginning.
    logfile.seekg( 0, std::ios::end );

    if ( std::streamoff file_size = logfile.tellg(); last_offset > file_size ) {
        last_offset = 0;
        last_line.clear();
    }

    // Seek to last offset and read lines to file end
    logfile.seekg( last_offset, std::ios::beg );

    while ( std::getline( logfile, line ) ) {
        last_line = line;
        new_line_read = true;
    }

    // Update last_offset for next call
    last_offset = logfile.tellg();
    if ( last_offset == -1 ) {
        // At EOF, set to file size
        logfile.clear();                      // must clear stream error before attempting to read again
        logfile.seekg( 0, std::ios::end );    // seek backwards to start of file to get size.
        last_offset = logfile.tellg();
    }

    logfile.close();

    // Only return true and update logline if we actually read a new line
    if ( new_line_read ) {
        logline = last_line;
        return true;
    }
    return false;    // no new line read and arg logline unchanged
}


/**
 * @brief Get current date/time string formatted for logging
 * 
 * @return Formatted date/time string
 */
std::string getDateTime()
{
    auto now = std::chrono::system_clock::now();
    auto in_time_t = std::chrono::system_clock::to_time_t( now );

    std::stringstream ss;
    std::tm tm_buf;
#if defined( _WIN32 ) || defined( _WIN64 )
    localtime_s( &tm_buf, &in_time_t );    // use thread-safe otherwise put_time can fail
#else
    localtime_r( &in_time_t, &tm_buf );
#endif
    ss << std::put_time( &tm_buf, "[%d/%m %H:%M:%S] " );
    return ss.str();
}


/** 
 * @brief Get list of output files in current directory with specified suffix
 * 
 * @param suffix  File suffix to search for (e.g., ".grb", ".nc")
 * @return Vector<std::string> of filenames matching the suffix
 */
std::vector<std::string> get_out_files( const std::string& suffix )
{

    std::vector<std::string> outFiles;
    std::string currentPath = fs::current_path().string();

    for ( const auto& entry : fs::directory_iterator( currentPath ) ) {
        if ( entry.is_regular_file() && entry.path().extension() == suffix ) {
            outFiles.push_back( entry.path().filename().string() );
        }
    }
    return outFiles;
}


/**
 * @brief Sleeps for the specified number of seconds.
 *        Seconds may be fractional.
 *        Handles very large sleep durations by breaking them into smaller chunks.
 */
void sleep_seconds( double seconds )
{
    if ( seconds < 0.0 || seconds > std::numeric_limits<double>::max() / 1000.0 ) {
        return;    // Or throw an exception?
    }
    if ( seconds == 0.0 ) {
        return;
    }

    // internally chrono::duration stores values as milliseconds, so find the double
    // corresponding to the largest long long (in millisecs)
    double max_sleep = static_cast<double>( std::numeric_limits<long long>::max() ) / 1000.0;

    if ( seconds <= max_sleep ) {
        std::chrono::duration<double> duration( seconds );
        std::this_thread::sleep_for( duration );
    } else {
        // Handle very large sleeps that might overflow max std::chrono::duration
        // This will split the sleep into smaller chunks.
        double remaining = seconds;

        while ( remaining > max_sleep ) {
            std::chrono::duration<double> duration( max_sleep );
            std::this_thread::sleep_for( duration );
            remaining = remaining - max_sleep;
        }
        std::chrono::duration<double> duration( remaining );
        std::this_thread::sleep_for( duration );
    }
}


/**
 * @brief Parse string & extract int value
 * @param value : input string_view to be parsed, may be trimmed.
 * @param out   : integer value updated on exit if successful
 * @param err_msg : error string if unsuccessful
 * @return true on success, false if failed to convert 'value' to int.
 */
bool parse_int( std::string& value, int& out, std::string& err_msg )
{
    err_msg.clear();

    // 'from_chars' below needs no leading whitespace before or after the value to be converted.
    trim_whitespace( value );
    if ( value.empty() ) {
        err_msg = "Empty integer value";
        return false;
    }

    const char* begin = value.data();
    const char* end = value.data() + value.size();

    auto result = std::from_chars( begin, end, out );

    if ( result.ec != std::errc{} ) {
        err_msg = std::make_error_code( result.ec ).message();
        return false;
    }
    if ( result.ptr != end ) {
        err_msg = "Invalid trailing characters in integer";
        return false;
    }
    return true;
}

/**
 * @brief Version of parse_int that only checks conversion is valid
 */
bool parse_int( std::string& value )
{
    int out;
    std::string err_msg;
    return parse_int( value, out, err_msg );
}

/**
 * @brief Run an external executable with a timeout and optional output-file freshness check.
 *        The child is started in the requested working directory with the supplied arguments and is terminated if it exceeds the timeout.
 *        Child stdout/stderr are inherited from the parent process unless a combined output file is supplied.
 */
TimedProcessResult run_process_with_timeout( const std::string& executable, const std::vector<std::string>& args,
                                             const std::string& working_dir, int timeout_seconds,
                                             const std::filesystem::path& expected_output_file,
                                             const std::vector<std::pair<std::string, std::string>>& child_env_vars,
                                             const std::filesystem::path& combined_output_file )
{
    TimedProcessResult result;

#if defined( _WIN32 ) || defined( _WIN64 )
    (void)executable;
    (void)args;
    (void)working_dir;
    (void)timeout_seconds;
    (void)expected_output_file;
    (void)child_env_vars;
    (void)combined_output_file;
    return result;
#else
    if ( executable.empty() || timeout_seconds <= 0 ) {
        return result;
    }

    auto previous_output_mtime = get_file_mtime( expected_output_file );

    pid_t pid = fork();
    if ( pid == -1 ) {
        return result;
    }

    if ( pid == 0 ) {
        if ( !working_dir.empty() && chdir( working_dir.c_str() ) != 0 ) {
            _exit( 126 );
        }

        if ( !combined_output_file.empty() ) {
            FILE* combined_stdout = freopen( combined_output_file.c_str(), "a", stdout );
            if ( combined_stdout == nullptr ) {
                _exit( 124 );
            }
            if ( freopen( combined_output_file.c_str(), "a", stderr ) == nullptr ) {
                _exit( 123 );
            }
        }

        for ( const auto& [name, value] : child_env_vars ) {
            if ( !set_env_var( name, value ) ) {
                _exit( 125 );
            }
        }

        std::vector<char*> argv;
        argv.reserve( args.size() + 2 );
        argv.push_back( const_cast<char*>( executable.c_str() ) );
        for ( const auto& arg : args ) {
            argv.push_back( const_cast<char*>( arg.c_str() ) );
        }
        argv.push_back( nullptr );

        execv( executable.c_str(), argv.data() );
        _exit( 127 );
    }

    auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds( timeout_seconds );
    int status = 0;

    while ( true ) {
        pid_t wait_result = waitpid( pid, &status, WNOHANG );
        if ( wait_result == pid ) {
            if ( WIFEXITED( status ) ) {
                result.exit_code = WEXITSTATUS( status );
                result.status = ( result.exit_code == 0 ) ? TimedProcessStatus::success : TimedProcessStatus::child_failed;
            } else {
                result.status = TimedProcessStatus::child_failed;
            }
            break;
        }

        if ( wait_result == -1 ) {
            result.status = TimedProcessStatus::wait_failed;
            return result;
        }

        if ( std::chrono::steady_clock::now() >= deadline ) {
            terminate_child_process( pid );
            result.status = TimedProcessStatus::timed_out;
            return result;
        }

        std::this_thread::sleep_for( std::chrono::milliseconds( 100 ) );
    }

    result.output_updated = expected_output_file.empty() ? true : file_was_updated( expected_output_file, previous_output_mtime );
    return result;
#endif
}

/**
 * @brief Return a short string describing a TimedProcessStatus value.
 *        This is intended for logging and test diagnostics rather than user-facing messages.
 */
const char* timed_process_status_to_string( TimedProcessStatus status )
{
    switch ( status ) {
    case TimedProcessStatus::success:
        return "success";
    case TimedProcessStatus::spawn_failed:
        return "spawn_failed";
    case TimedProcessStatus::wait_failed:
        return "wait_failed";
    case TimedProcessStatus::timed_out:
        return "timed_out";
    case TimedProcessStatus::child_failed:
        return "child_failed";
    }

    return "unknown";
}
