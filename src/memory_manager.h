#pragma once
#include <cstring>
#include <vector>
#include <cstdint>
#include <string>
#include <functional>
#include <utility>
#include <unordered_map>
#include <unordered_set>
#include <fstream>
#include <iostream>
#include <time.h>
#include <limits>
#include <tuple>
#include <memory>
#include <typeinfo>

void DUMP_STACK(const char*, const char*);
void SF_PRINT_TRACKED_MEMORY();
void SF_TRACK_MEMORY_ON();
void SF_TRACK_MEMORY_OFF();

struct SF_STACK_ENTRY
{
    explicit SF_STACK_ENTRY(const char* msg);
    ~SF_STACK_ENTRY();
};

struct SF_TRACK_SUPPR
{
    explicit SF_TRACK_SUPPR();
    ~SF_TRACK_SUPPR();
};

#define _JOIN_1(_x, _y) _x##_y
#define _JOIN_2(_p, _q) _JOIN_1(_p, _q)
#define _STRINGY(_s) #_s
#define STRINGY(_s) _STRINGY(_s)
#define SF_MARK_STACK       SF_STACK_ENTRY _JOIN_2(SF_STACK_MARKER, __LINE__){__FILE__ ":" STRINGY(__LINE__) }
#define SF_MESG_STACK(_msg) SF_STACK_ENTRY _JOIN_2(SF_STACK_MARKER, __LINE__){_msg}
#define SF_NO_TRACK         SF_TRACK_SUPPR _JOIN_2(SF_STACK_MARKER, __LINE__){}
#define SF_SCOPE(_msg)      sonic_field::scope _JOIN_2(SF_SCOPE, __LINE__){}; SF_MESG_STACK(_msg);
// TODO: Print out stack trace before throwing.
#define SF_THROW(...)      do{SF_MARK_STACK; auto e = __VA_ARGS__; DUMP_STACK(typeid(e).name(), e.what()); throw e;}while(0)

// This are used in memory assignement etc. so we define them here which is 'above' the rest of sonic field.
namespace sonic_field
{
    constexpr uint64_t SAMPLES_PER_SECOND = 128000;
    constexpr uint64_t BLOCK_SIZE = SAMPLES_PER_SECOND / 1000;
    constexpr uint64_t WIRE_BLOCK_SIZE = BLOCK_SIZE >> 1;
    constexpr uint64_t SF_BLOCK_POOL_MAX = 64;
    double* new_block(bool init = true);
    void free_block(double* block);
}