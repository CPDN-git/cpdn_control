//
//  WRF DateTime implementation
//     Glenn Carver, CPDN, 2026

#pragma once

// Struct to hold WRF-style date & time; used on output & restart filenames
struct DateTime {
    int year = 0;
    int month = 0;
    int day = 0;
    int hour = 0;
    int minute = 0;
    int second = 0;
};

DateTime secs_to_datetime_duration( long long seconds );
DateTime add_duration_to_datetime( const DateTime& start, const DateTime& duration );
long long datetime_duration_seconds( const DateTime& start, const DateTime& end );
