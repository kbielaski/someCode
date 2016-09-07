#ifndef __HELPERS__
#define __HELPERS__ 1

#include <stdexcept>
#include <cstdio>

#define dief(fmt, ...) { char diebuf[128]; snprintf(diebuf,sizeof(diebuf),fmt, ##__VA_ARGS__); throw std::runtime_error(diebuf); }
#define die(msg) { throw std::runtime_error(msg); }

#endif
