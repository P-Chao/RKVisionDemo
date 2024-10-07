#include <sys/time.h>
#include <iostream>

uint32_t get_time_t0() {
    struct timeval tv;
    gettimeofday(&tv, nullptr);
    return (uint32_t)(tv.tv_usec / 1000) + 1000 * tv.tv_sec;
}

uint32_t get_time_t1(uint32_t t0) {
    return get_time_t0() - t0;
}
