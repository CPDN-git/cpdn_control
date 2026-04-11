#pragma once

#include <cstring>

constexpr int MD5_LEN = 33;

inline int md5_file( const char*, char* md5_buf, double& nbytes )
{
    nbytes = 0.0;
    if ( md5_buf != nullptr ) {
        std::strncpy( md5_buf, "00000000000000000000000000000000", MD5_LEN );
        md5_buf[MD5_LEN - 1] = '\0';
    }
    return 0;
}
