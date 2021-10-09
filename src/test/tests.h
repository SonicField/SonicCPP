#pragma once
#include "../sonic_field.h"
#include <sstream>
namespace sonic_field
{

    // Test machinery.
    class assertion_error : public std::runtime_error
    {
    public:
        using std::runtime_error::runtime_error;
    };

    inline void assert_equal(const auto& a, const auto& b, const auto& msg)
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

    inline void assert_true(const auto& a, const auto& msg)
    {
        if (!a)
        {
            std::stringstream s{};
            s << "Assertion '" << msg << "' failed: ";
            s << a << " not true ";
            SF_THROW(assertion_error{ s.str() });
        }
        std::cerr << "Assertion pass (" << msg << "): " << a << " is true" << std::endl;
    }

    inline void assert_false(const auto& a, const auto& msg)
    {
        if (a)
        {
            std::stringstream s{};
            s << "Assertion '" << msg << "' failed: ";
            s << a << " not false ";
            SF_THROW(assertion_error{ s.str() });
        }
        std::cerr << "Assertion pass (" << msg << "): " << a << " is false" << std::endl;
    }

    template<typename E>
    inline void assert_throws(auto to_run, const std::string& to_find, const auto msg)
    {
        try
        {
            to_run();
        }
        catch(E& e)
        {
            std::string what = e.what();
            if (what.find(to_find) == std::string::npos)
                SF_THROW(assertion_error{"'" + to_find + "' not found in error message got '" + what + "'"});
            std::cerr << "Assertion pass (" << typeid(E).name() << ": " << msg << ") thrown" << std::endl;
            return;
        }
        catch(std::exception& u)
        {
            std::stringstream s{};
            s << "Assertion '" << msg
                << "' failed: expected exception '" << typeid(E).name()
                << "' not thrown but '" << typeid(u).name() << "' was";
            SF_THROW(assertion_error{ s.str() });
        }
        std::stringstream s{};
        s << "Assertion '" << msg << "' failed: expected exception '" << typeid(E).name() << "' not thrown";
        SF_THROW(assertion_error{ s.str() });
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
            ++m_ran;
            try
            {
                to_run();
                std::cerr << "OK!!! " << std::endl;
            }
            catch (std::exception& err)
            {
                ++m_failed;
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
