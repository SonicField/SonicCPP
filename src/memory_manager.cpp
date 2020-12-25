#include "memory_manager.h"

thread_local std::unordered_map<void*, std::pair<std::size_t, std::string> > SF_MEMORY_TRACKER{};
thread_local std::vector<const char*> SF_V_STACK{};
thread_local uint64_t SF_IN_SF_MEMORY_TRACKER = 0;
thread_local bool SF_TRACK_MEMORY = false;

SF_TRACK_SUPPR::SF_TRACK_SUPPR()
{
    ++SF_IN_SF_MEMORY_TRACKER;
}

SF_TRACK_SUPPR::~SF_TRACK_SUPPR()
{
    --SF_IN_SF_MEMORY_TRACKER;
}

SF_STACK_ENTRY::SF_STACK_ENTRY(const char* msg)
{
    ++SF_IN_SF_MEMORY_TRACKER;
    SF_V_STACK.push_back(msg);
    --SF_IN_SF_MEMORY_TRACKER;
}

SF_STACK_ENTRY::~SF_STACK_ENTRY()
{
    SF_NO_TRACK;
    SF_V_STACK.pop_back();
}

void DUMP_STACK(const char* exp, const char* what)
{
    auto idx = SF_V_STACK.size();
    std::cerr << "Error encountered: " << exp << "{" << what << "}\n";
    while (idx > 0)
    {
        --idx;
        std::cerr << "... " << SF_V_STACK[idx] << '\n';
    }
    std::cerr.flush();
}

void SF_TRACK_MEMORY_ON()
{
    SF_MEMORY_TRACKER.clear();
    SF_TRACK_MEMORY = true;
}

void SF_TRACK_MEMORY_OFF()
{
    SF_TRACK_MEMORY = false;
    SF_MEMORY_TRACKER.clear();
}

inline void* _do_new(std::size_t sz)
{
    auto ret = std::malloc(sz);
    if (!ret) throw std::bad_alloc();
    if (SF_TRACK_MEMORY && !SF_IN_SF_MEMORY_TRACKER)
    {
        SF_NO_TRACK;
        std::string stack{};
        for (auto& entry : SF_V_STACK)
        {
            if (stack.size())
                stack += "\n";
            stack += entry;
        }
        SF_MEMORY_TRACKER.insert({ ret, {sz, stack } });
    }
    return ret;
}

void* operator new(std::size_t sz)
{
    return _do_new(sz);
}

void* operator new[](std::size_t sz)
{
    return _do_new(sz);
}

inline void _do_delete(void* ptr) noexcept
{
    if (SF_TRACK_MEMORY && !SF_IN_SF_MEMORY_TRACKER)
    {
        SF_NO_TRACK;
        SF_MEMORY_TRACKER.erase(ptr);
    }
    std::free(ptr);
}

void operator delete(void* ptr) noexcept
{
    _do_delete(ptr);
}

void operator delete[](void* ptr) noexcept
{
    _do_delete(ptr);
}

void SF_PRINT_TRACKED_MEMORY()
{
    for (const auto& kvp : SF_MEMORY_TRACKER)
    {
        std::cerr << "LEAK: " << kvp.first << " of: " << kvp.second.first << "\n"
            << kvp.second.second << std::endl;
    }
}

namespace sonic_field
{
    thread_local std::vector<std::unique_ptr<double>> SF_BLOCK_POOL;
    double* new_block(bool init)
    {
        double* block;
        if (SF_BLOCK_POOL.size())
        {
            block = SF_BLOCK_POOL.back().release();
            SF_BLOCK_POOL.pop_back();
        }
        else
        {
            block = new double[BLOCK_SIZE];
        }
        if (init) memset(block, 0, sizeof(double) * BLOCK_SIZE);
        return block;
    }

    void free_block(double* block)
    {
        SF_BLOCK_POOL.emplace_back(std::unique_ptr<double>{block});
        if (SF_BLOCK_POOL.size() > SF_BLOCK_POOL_MAX)
        {
            SF_THROW(std::logic_error{"Blocks appear to be being leaked"});
        }
    }
}