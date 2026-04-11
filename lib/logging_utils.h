#pragma once

#include <iosfwd>
#include <memory>

class TimestampedCerrGuard {

  public:
    explicit TimestampedCerrGuard( std::ostream& stream );
    ~TimestampedCerrGuard();

    TimestampedCerrGuard( const TimestampedCerrGuard& ) = delete;
    TimestampedCerrGuard& operator=( const TimestampedCerrGuard& ) = delete;
    TimestampedCerrGuard( TimestampedCerrGuard&& ) = delete;
    TimestampedCerrGuard& operator=( TimestampedCerrGuard&& ) = delete;

  private:
    std::ostream& stream;
    std::streambuf* original_buf = nullptr;
    class TimestampedStreambuf;
    std::unique_ptr<TimestampedStreambuf> wrapped_buf;
};
