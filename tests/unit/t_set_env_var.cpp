/**
 * @file t_set_env_var.cpp
 * @brief Unit test for set_env_var() function
 *
 * Verifies environment variables are set and overwritten correctly.
 */

#include <cstdlib>
#include <iostream>
#include <string>

#include "../lib/utils.h"
#include "unit_tests.h"

int t_set_env_var()
{
    TEST( "t_set_env_var" );

    const std::string var_name = "CPDN_TEST_ENV_VAR";
    const char* original = std::getenv( var_name.c_str() );
    std::string original_value = original ? std::string( original ) : std::string();

    int tests = 0;
    int passed = 0;

    // Set a fresh value
    tests++;
    if ( set_env_var( var_name, "initial_value" ) ) {
        const char* val = std::getenv( var_name.c_str() );
        if ( val && std::string( val ) == "initial_value" ) {
            passed++;
        } else {
            std::cerr << "  Failed to read back initial value\n";
        }
    } else {
        std::cerr << "  set_env_var returned false for initial set\n";
    }

    // Overwrite with a new value
    tests++;
    if ( set_env_var( var_name, "updated_value" ) ) {
        const char* val = std::getenv( var_name.c_str() );
        if ( val && std::string( val ) == "updated_value" ) {
            passed++;
        } else {
            std::cerr << "  Failed to read back updated value\n";
        }
    } else {
        std::cerr << "  set_env_var returned false for overwrite\n";
    }

    // Restore original environment if present, otherwise clear the test var
    if ( !original_value.empty() ) {
        set_env_var( var_name, original_value );
    } else {
#if defined( __unix__ ) || defined( __APPLE__ )
        unsetenv( var_name.c_str() );
#elif defined( _WIN32 )
        _putenv( ( var_name + "=" ).c_str() );
#endif
    }

    std::cout << "  set_env_var: " << passed << "/" << tests << " tests passed\n";
    if ( passed == tests ) {
        SUCCESS;
        return EXIT_SUCCESS;
    } else {
        FAIL;
        return EXIT_FAILURE;
    }
}
