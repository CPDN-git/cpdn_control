/**
 * @file t_model_frac_done.cpp
 * @brief Unit test for model_frac_done() function
 * 
 * Tests the progress calculation function which:
 * - Calculates fraction of model run completed (0.0 to 1.0)
 * - Maintains state between calls via static variables
 * - Incorporates multi-threading speedup factor
 * - Returns monotonically increasing values
 * - Clamps output to valid range [0.0, 1.0]
 * 
 *    Glenn Carver, CPDN, 2025.
 */

#include <string>
#include <iostream>
#include <cstdlib>
#include <cmath>

#include "../src/cpdn_control.h"
#include "unit_tests.h"


/**
 * @brief Test: model_frac_done() function for progress calculation
 */
int t_model_frac_done()
{
    TEST("t_model_frac_done");
    
    int test_count = 0;
    int test_passed = 0;
    double result;
    double prev_result = 0.0;

    // Test 1: Starting state - step 0 of 100 total steps
    test_count++;
    result = model_frac_done(0.0, 100.0, 1);
    if (result >= 0.0 && result <= 0.01) {  // Should be very small but not negative
        test_passed++;
    } else {
        std::cerr << "  Test 1 FAILED: Starting step should be near 0.0, got " << result << "\n";
    }
    prev_result = result;

    // Test 2: Mid-run progress - step 50 of 100
    test_count++;
    result = model_frac_done(50.0, 100.0, 1);
    if (result >= 0.45 && result <= 0.55) {  // Should be around 0.5
        test_passed++;
    } else {
        std::cerr << "  Test 2 FAILED: Mid-run step 50/100 should be around 0.5, got " << result << "\n";
    }
    prev_result = result;

    // Test 3: Near completion - step 99 of 100
    test_count++;
    result = model_frac_done(99.0, 100.0, 1);
    if (result >= 0.95) {  // Should be near 1.0 but not exceed it
        test_passed++;
    } else {
        std::cerr << "  Test 3 FAILED: Near completion should be near 1.0, got " << result << "\n";
    }
    prev_result = result;

    // Test 4: Completion - step 100 of 100
    test_count++;
    result = model_frac_done(100.0, 100.0, 1);
    if (result >= 0.99 && result <= 1.0) {  // Should be at or very close to 1.0
        test_passed++;
    } else {
        std::cerr << "  Test 4 FAILED: Completion should be at 1.0, got " << result << "\n";
    }

    // Test 5: Very small total steps (1 step)
    test_count++;
    result = model_frac_done(0.0, 1.0, 1);
    if (result >= 0.0 && result <= 1.0) {  // Should be valid in range
        test_passed++;
    } else {
        std::cerr << "  Test 5 FAILED: Single step progress should be in [0.0, 1.0], got " << result << "\n";
    }

    // Test 6: Large total steps (10000)
    test_count++;
    result = model_frac_done(5000.0, 10000.0, 1);
    if (result >= 0.45 && result <= 0.55) {  // Should be around 0.5
        test_passed++;
    } else {
        std::cerr << "  Test 6 FAILED: Mid-run 5000/10000 should be around 0.5, got " << result << "\n";
    }

    // Test 7: Multi-threading effect - 2 threads
    // The function applies a speedup factor based on nthreads
    // So same step with 2 threads should show more progress than with 1 thread
    test_count++;
    double result_1thread = model_frac_done(10.0, 100.0, 1);
    double result_2threads = model_frac_done(10.0, 100.0, 2);
    if (result_2threads > result_1thread || result_2threads == result_1thread) {
        // Progress should be >= with more threads (speedup factor applied)
        test_passed++;
    } else {
        std::cerr << "  Test 7 FAILED: Multi-thread should increase progress, "
                  << "1-thread=" << result_1thread << ", 2-threads=" << result_2threads << "\n";
    }

    // Test 8: 4 threads should show even more progress
    test_count++;
    double result_4threads = model_frac_done(10.0, 100.0, 4);
    if (result_4threads >= result_2threads) {
        test_passed++;
    } else {
        std::cerr << "  Test 8 FAILED: 4 threads should show >= progress than 2 threads\n";
    }

    // Test 9: Very high thread count (8 threads)
    test_count++;
    result = model_frac_done(25.0, 100.0, 8);
    if (result >= 0.2 && result <= 0.4) {  // Should reflect the speedup and progress
        test_passed++;
    } else {
        std::cerr << "  Test 9 FAILED: 8-thread execution should produce valid progress\n";
    }

    // Test 10: Result should never be negative
    test_count++;
    result = model_frac_done(-5.0, 100.0, 1);  // Negative step (shouldn't happen but test bounds)
    if (result >= 0.0) {
        test_passed++;
    } else {
        std::cerr << "  Test 10 FAILED: Progress should never be negative, got " << result << "\n";
    }

    // Test 11: Result should never exceed 1.0
    test_count++;
    result = model_frac_done(150.0, 100.0, 1);  // Step > total (shouldn't happen)
    if (result <= 1.0) {
        test_passed++;
    } else {
        std::cerr << "  Test 11 FAILED: Progress should never exceed 1.0, got " << result << "\n";
    }

    // Test 12: Fractional steps
    test_count++;
    result = model_frac_done(33.33, 100.0, 1);
    if (result >= 0.3 && result <= 0.4) {  // Should be around 0.333
        test_passed++;
    } else {
        std::cerr << "  Test 12 FAILED: Fractional step 33.33/100 should be around 0.33, got " << result << "\n";
    }

    // Test 13: Very small progress
    test_count++;
    result = model_frac_done(0.1, 10000.0, 1);
    if (result >= 0.0 && result < 0.01) {  // Should be very small
        test_passed++;
    } else {
        std::cerr << "  Test 13 FAILED: 0.1/10000 should be very small, got " << result << "\n";
    }

    // Test 14: Consistent results for same inputs within reasonable tolerance
    test_count++;
    double result_a = model_frac_done(50.0, 100.0, 2);
    double result_b = model_frac_done(50.0, 100.0, 2);
    if (std::abs(result_a - result_b) < 0.001) {  // Should be very close or identical
        test_passed++;
    } else {
        std::cerr << "  Test 14 FAILED: Repeated calls should give consistent results, "
                  << "got " << result_a << " then " << result_b << "\n";
    }

    // Test 15: Different thread counts produce valid results in valid range
    test_count++;
    bool all_valid = true;
    for (int nthreads = 1; nthreads <= 8; nthreads++) {
        result = model_frac_done(50.0, 100.0, nthreads);
        if (result < 0.0 || result > 1.0) {
            std::cerr << "  Test 15 FAILED: nthreads=" << nthreads << " produced out-of-range result " << result << "\n";
            all_valid = false;
            break;
        }
    }
    if (all_valid) {
        test_passed++;
    }

    // Summary
    std::cout << "  model_frac_done: " << test_passed << "/" << test_count << " tests passed\n";

    if (test_passed == test_count) {
        SUCCESS;
        return EXIT_SUCCESS;
    } else {
        FAIL;
        return EXIT_FAILURE;
    }
}
