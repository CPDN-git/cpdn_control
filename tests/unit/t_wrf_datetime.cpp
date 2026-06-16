// Unit tests for WRF datetime helpers used to form timestamped output filenames.
//
//  Glenn Carver, CPDN, 2026

#include <array>
#include <cstdlib>
#include <cstdio>
#include <iostream>
#include <string>

#include "../models/wrf/wrf_datetime.h"
#include "unit_tests.h"

namespace {

std::string format_wrf_timestamp( const DateTime& datetime )
{
    std::array<char, 20> timestamp_buffer{};
    const int timestamp_len = std::snprintf( timestamp_buffer.data(), timestamp_buffer.size(), "%04d-%02d-%02d_%02d:%02d:%02d", datetime.year,
                                             datetime.month, datetime.day, datetime.hour, datetime.minute, datetime.second );
    if ( timestamp_len != 19 ) {
        return {};
    }

    return std::string( timestamp_buffer.data(), static_cast<std::size_t>( timestamp_len ) );
}

bool check_timestamp_case( const char* label, const DateTime& start, long long elapsed_seconds, const char* expected_timestamp )
{
    const DateTime duration = secs_to_datetime_duration( elapsed_seconds );
    const DateTime output_date = add_duration_to_datetime( start, duration );
    const std::string actual_timestamp = format_wrf_timestamp( output_date );

    if ( actual_timestamp == expected_timestamp ) {
        return true;
    }

    std::cerr << "  " << label << " expected " << expected_timestamp << " but got " << actual_timestamp << '\n';
    return false;
}

}    // namespace

int t_wrf_datetime()
{
    TEST( "t_wrf_datetime" );

    int test_count = 0;
    int test_passed = 0;

    test_count++;
    if ( check_timestamp_case( "simple duration", DateTime{ 2022, 7, 1, 0, 0, 0 }, 5400, "2022-07-01_01:30:00" ) ) {
        test_passed++;
    }

    test_count++;
    if ( check_timestamp_case( "month boundary", DateTime{ 2022, 1, 31, 23, 0, 0 }, 7200, "2022-02-01_01:00:00" ) ) {
        test_passed++;
    }

    test_count++;
    if ( check_timestamp_case( "leap year february boundary", DateTime{ 2024, 2, 28, 23, 30, 0 }, 90000, "2024-03-01_00:30:00" ) ) {
        test_passed++;
    }

    test_count++;
    if ( check_timestamp_case( "new year boundary", DateTime{ 2023, 12, 31, 23, 59, 30 }, 90, "2024-01-01_00:01:00" ) ) {
        test_passed++;
    }

    std::cout << "  wrf_datetime: " << test_passed << "/" << test_count << " tests passed\n";
    if ( test_passed == test_count ) {
        TEST_SUCCESS;
        return EXIT_SUCCESS;
    }

    TEST_FAIL;
    return EXIT_FAILURE;
}
