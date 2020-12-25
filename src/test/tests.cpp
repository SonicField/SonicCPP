#include "tests.h"
#include "../midi_support.h"

namespace sonic_field
{
    test_runner::test_runner(const std::string& data_dir) :
        m_failed{ 0 }, m_ran{ 0 }, m_data_dir{ data_dir }
    {
        // run tests here.
        try_run("Dummy test", [] { std::cout << "Dummy test" << std::endl; });
        try_run("Midi test a", [&] { test_midi_a(m_data_dir); });
        std::cerr << "\n";
        std::cerr << "****************************************\n";
        std::cerr << "* Failed tests: " << m_failed << "\n";
        std::cerr << "* Total  tests: " << m_ran << std::endl;
        std::cerr << "****************************************\n";
    }

    bool test_runner::ok() const
    {
        return m_failed == 0;
    }


    // Test definitions go here:
    // =========================
    void test_midi_a(const std::string& data_dir)
    {
        SF_MARK_STACK;
        auto inp = join_path({ data_dir , "test_a.mid" });
        std::ifstream file{ inp, std::ios_base::in | std::ios_base::binary };
        test_header("Correct header chunk");
        auto h = midi::read_header(file);
        assertEqual(h.m_chunk.m_size, 6, "header chunk size");
        assertEqual(type_of_chunk(h.m_chunk), midi::header_type, "header chunk type");
        assertEqual(h.m_ntrks, 2, "number of tracks");
        assertEqual(h.m_format, 1, "format");
        assertEqual(is_smtpe(h), true, "is smtpe");
        // TODO: this makes no sense - beed to see if this is because it is a 'dummy' track or some such.
        assertEqual(int(smtpe_type(h)), 127, "smtpe type");
        auto tc = midi::read_chunk(file);
        assertEqual(type_of_chunk(tc), midi::track_type, "track chunk type");
    }
}
