#include "logging_utils.h"

#include <chrono>
#include <ctime>
#include <iomanip>
#include <ostream>
#include <sstream>
#include <streambuf>
#include <string>

namespace {

std::string get_current_datetime_prefix()
{
    const auto now = std::chrono::system_clock::now();
    const auto now_time = std::chrono::system_clock::to_time_t( now );

    std::tm tm_buf{};
#if defined(_WIN32) || defined(_WIN64)
    localtime_s( &tm_buf, &now_time );
#else
    localtime_r( &now_time, &tm_buf );
#endif

    std::ostringstream oss;
    oss << std::put_time( &tm_buf, "[%Y-%m-%d %H:%M:%S] " );
    return oss.str();
}

}    // namespace


class TimestampedCerrGuard::TimestampedStreambuf : public std::streambuf {

  public:
    explicit TimestampedStreambuf( std::streambuf* destination ) : destination( destination ) {}

  protected:
    int overflow( int ch ) override
    {
        if ( traits_type::eq_int_type( ch, traits_type::eof() ) ) {
            return destination->pubsync() == 0 ? traits_type::not_eof( ch ) : traits_type::eof();
        }

        const char c = traits_type::to_char_type( ch );

        if ( at_line_start ) {
            if ( c == '\n' ) {
                if ( traits_type::eq_int_type( destination->sputc( c ), traits_type::eof() ) ) {
                    return traits_type::eof();
                }
                return ch;
            }

            const std::string prefix = get_current_datetime_prefix();
            if ( destination->sputn( prefix.data(), static_cast<std::streamsize>( prefix.size() ) ) !=
                 static_cast<std::streamsize>( prefix.size() ) ) {
                return traits_type::eof();
            }
            at_line_start = false;
        }

        if ( traits_type::eq_int_type( destination->sputc( c ), traits_type::eof() ) ) {
            return traits_type::eof();
        }

        if ( c == '\n' ) {
            at_line_start = true;
        }

        return ch;
    }

    int sync() override { return destination->pubsync(); }

  private:
    std::streambuf* destination;
    bool at_line_start = true;
};


TimestampedCerrGuard::TimestampedCerrGuard( std::ostream& stream ) : stream( stream ), original_buf( stream.rdbuf() )
{
    wrapped_buf = std::make_unique<TimestampedStreambuf>( original_buf );
    stream.rdbuf( wrapped_buf.get() );
}


TimestampedCerrGuard::~TimestampedCerrGuard()
{
    stream.flush();
    stream.rdbuf( original_buf );
}
