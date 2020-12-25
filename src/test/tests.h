#pragma once
#include "../sonic_field.h"
#include <sstream>
namespace sonic_field
{
    // Test declarations go here.
    void test_midi_a(const std::string&);

    // Test machinery.
    class assertion_error : public std::runtime_error
    {
    public:
        using std::runtime_error::runtime_error;
    };

    template<typename A, typename B, typename C>
    inline void assertEqual(const A& a, const B& b, const C& msg)
    {
        if (a != b)
        {
            std::stringstream s{};
            s << "Assertion '" << msg << "' failed: ";
            s << a << " != " << b;
            SF_THROW(assertion_error{ s.str() });
        }
        std::cerr << "Assertion pass (" << msg << "): " << a << " == " << b << std::endl;
    }

    template<typename T>
    inline void test_header(T name)
    {
        std::cerr << "Sub-test: " << name << std::endl;
    }

    class test_runner
    {
        uint64_t m_failed;
        uint64_t m_ran;
        const std::string& m_data_dir;
    public:
        explicit test_runner() = delete;
        explicit test_runner(const std::string& data_dir);

        template<typename T>
        void try_run(const std::string& name, T to_run)
        {
            SF_MARK_STACK;
            std::cerr << "\nRunning: " << name << std::endl;
            try
            {
                to_run();
                std::cerr << "OK!!! " << std::endl;
            }
            catch (std::exception& err)
            {
                std::cerr << "FAIL! " << typeid(err).name()  << ": " <<err.what() << std::endl;
            }
        }

        bool ok() const;
    };

    // Insert test to be run in this method.
    inline int run_tests(const std::string& data_dir)
    {
        SF_MARK_STACK;
        test_runner runner{data_dir};
        return runner.ok() ? 0 : 1;
    }
}
