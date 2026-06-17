//
//  Implementation of WRF DateTime functions.
//  Useful for handling filenames and timestepping.
//       Glenn Carver, CPDN, 2026

#include "wrf_datetime.h"


//  Helpers

namespace {
bool is_leap_year( int year ) { return ( year % 400 == 0 ) || ( year % 4 == 0 && year % 100 != 0 ); }

int days_in_month( int year, int month )
{
    static constexpr int month_lengths[12] = { 31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31 };
    if ( month < 1 || month > 12 ) {
        return 0;
    }
    if ( month == 2 && is_leap_year( year ) ) {
        return 29;
    }
    return month_lengths[month - 1];
}

bool is_valid_datetime( const DateTime& datetime )
{
    if ( datetime.year <= 0 || datetime.month < 1 || datetime.month > 12 || datetime.day < 1 ) {
        return false;
    }

    if ( datetime.hour < 0 || datetime.hour > 23 || datetime.minute < 0 || datetime.minute > 59 || datetime.second < 0 || datetime.second > 59 ) {
        return false;
    }

    return datetime.day <= days_in_month( datetime.year, datetime.month );
}

long long days_before_year( int year )
{
    const long long completed_years = static_cast<long long>( year ) - 1;
    return ( completed_years * 365 ) + ( completed_years / 4 ) - ( completed_years / 100 ) + ( completed_years / 400 );
}

long long datetime_to_absolute_seconds( const DateTime& datetime )
{
    long long days = days_before_year( datetime.year );
    for ( int month = 1; month < datetime.month; ++month ) {
        days += days_in_month( datetime.year, month );
    }
    days += static_cast<long long>( datetime.day ) - 1;

    return ( days * 86400 ) + ( static_cast<long long>( datetime.hour ) * 3600 ) + ( static_cast<long long>( datetime.minute ) * 60 ) +
           static_cast<long long>( datetime.second );
}

}    // namespace


/**
 * @brief  Take an incoming number of seconds and compute the duration in a day/time format.
 * 
 * Note we use the DateTime struct as an offset only; it's not possible to set the month & year.
 * So 'day' in the struct can be > 31.
 * Would be semantically correct to use a 'DayTime' struct but this is more compact.
 */
DateTime secs_to_datetime_duration( long long seconds )
{
    DateTime duration{};
    long long day_seconds = seconds % 86400;

    duration.day = static_cast<int>( seconds / static_cast<long long>( 86400 ) );

    // Assign the time duration into the hours, mins, and secs fields.
    duration.second = static_cast<int>( day_seconds % 60 );
    day_seconds /= 60;
    duration.minute = static_cast<int>( day_seconds % 60 );
    day_seconds /= 60;
    duration.hour = static_cast<int>( day_seconds % 60 );

    return duration;
}


DateTime add_duration_to_datetime( const DateTime& start, const DateTime& duration )
{
    DateTime end_date = start;

    // Add a duration in days/hours/mins/secs to a year-month-day:HH-MM-SS format
    // Note the incoming duration only uses the days/hours/mins/secs as an offset
    // in the DateTime struct, not a real date.

    // Add the intra-day offset and carry any overflow into the next larger field.
    end_date.second += duration.second;
    end_date.minute += duration.minute;
    end_date.hour += duration.hour;
    long elapsed_days = duration.day;

    // Adjust if time components have gone over boundary
    if ( end_date.second >= 60 ) {
        end_date.second -= 60;
        ++end_date.minute;
    }
    if ( end_date.minute >= 60 ) {
        end_date.minute -= 60;
        ++end_date.hour;
    }
    if ( end_date.hour >= 24 ) {
        end_date.hour -= 24;
        ++elapsed_days;
    }

    // Advance the calendar across month and year boundaries using the remaining whole-day offset.
    while ( elapsed_days > 0 ) {
        const int month_days = days_in_month( end_date.year, end_date.month );
        const int days_remaining_in_month = month_days - end_date.day;

        if ( elapsed_days <= static_cast<long>( days_remaining_in_month ) ) {
            end_date.day += static_cast<int>( elapsed_days );
            elapsed_days = 0;
            break;
        }

        elapsed_days -= ( days_remaining_in_month + 1 );
        end_date.day = 1;
        ++end_date.month;
        if ( end_date.month > 12 ) {
            end_date.month = 1;
            ++end_date.year;
        }
    }

    return end_date;
}


/**
 * @brief Convert a WRF timestamp back to elapsed seconds from the model start time.
 * @returns Elapsed seconds, or -1 if either timestamp is invalid or the end time precedes the start.
 */
long long datetime_duration_seconds( const DateTime& start, const DateTime& end )
{
    if ( !is_valid_datetime( start ) || !is_valid_datetime( end ) ) {
        return -1;
    }

    const long long start_seconds = datetime_to_absolute_seconds( start );
    const long long end_seconds = datetime_to_absolute_seconds( end );

    if ( end_seconds < start_seconds ) {
        return -1;
    }

    return end_seconds - start_seconds;
}
