/**
 * @file t_loop_delay_tuning.cpp
 * @brief Unit tests for dynamic loop-delay helper functions.
 *
 *    Glenn Carver, CPDN, 2026.
 */

#include <cmath>
#include <cstdlib>
#include <iostream>

#include "lib/utils.h"
#include "unit_tests.h"


int t_loop_delay_tuning()
{
    TEST( "t_loop_delay_tuning" );

    int test_count = 0;
    int test_passed = 0;

    StepDeltaAverageWindow window;

    test_count++;
    if ( average_step_delta( window ) == 0.0 && !step_delta_exceeds_average( window, 2 ) ) {
        test_passed++;
    } else {
        std::cerr << "  Test 1 FAILED: empty window should report zero average and no reduction trigger\n";
    }

    for ( int idx = 0; idx < 5; ++idx ) {
        record_step_delta( window, 1 );
    }

    test_count++;
    if ( std::abs( average_step_delta( window ) - 1.0 ) < 1e-9 ) {
        test_passed++;
    } else {
        std::cerr << "  Test 2 FAILED: expected average of 1.0 after five single-step samples\n";
    }

    test_count++;
    if ( step_delta_exceeds_average( window, 2 ) && !step_delta_exceeds_average( window, 1 ) ) {
        test_passed++;
    } else {
        std::cerr << "  Test 3 FAILED: step delta should only trigger when above the current rolling average\n";
    }

    record_step_delta( window, 5 );
    test_count++;
    if ( std::abs( average_step_delta( window ) - 1.8 ) < 1e-9 ) {
        test_passed++;
    } else {
        std::cerr << "  Test 4 FAILED: rolling window should discard the oldest sample when a sixth value is added\n";
    }

    test_count++;
    if ( std::abs( reduce_loop_delay_seconds( 10.0, 0.2, 0.2 ) - 9.8 ) < 1e-9 &&
         std::abs( reduce_loop_delay_seconds( 0.3, 0.2, 0.2 ) - 0.2 ) < 1e-9 ) {
        test_passed++;
    } else {
        std::cerr << "  Test 5 FAILED: delay reduction should subtract by 0.2 and clamp at the minimum\n";
    }

    std::cout << "  loop_delay_tuning: " << test_passed << "/" << test_count << " tests passed\n";

    if ( test_passed == test_count ) {
        TEST_SUCCESS;
        return EXIT_SUCCESS;
    } else {
        TEST_FAIL;
        return EXIT_FAILURE;
    }
}
