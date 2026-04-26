#pragma once

#include <iosfwd>
#include <memory>

class Timestamped {

  public:
    explicit Timestamped( std::ostream& stream );
    ~Timestamped();

    Timestamped( const Timestamped& ) = delete;
    Timestamped& operator=( const Timestamped& ) = delete;
    Timestamped( Timestamped&& ) = delete;
    Timestamped& operator=( Timestamped&& ) = delete;

  private:
    std::ostream& stream;
    std::streambuf* original_buf = nullptr;
    class TimestampedStreambuf;
    std::unique_ptr<TimestampedStreambuf> wrapped_buf;
};
