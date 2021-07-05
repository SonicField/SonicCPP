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
        assertEqual(type_of_chunk(h.m_chunk), midi::chunk_type::header, "header chunk type");
        assertEqual(h.m_ntrks, 2, "number of tracks");
        assertEqual(h.m_format, 1, "format");
        assertEqual(is_smtpe(h), true, "is smtpe");
        assertEqual(int(smtpe_type(h)), 127, "smtpe type");
        // m_division comes out as FFC0 which is -1 for the format and 192 resolution.  As this makes no
        // sense to me I can only assume I am doing something wrong here. Interestingly the first event is
        // set_tempo which maybe has something to do with all this?
        std::cout << "header: " << h << std::endl;
        auto tc = midi::read_chunk(file);
        assertEqual(type_of_chunk(tc), midi::chunk_type::track, "track chunk type");
        std::cout << "Track: " << tc << std::endl;
        auto event1 =  midi::parse_event(file);
        std::cout << "First event: " << *event1 << std::endl;
        assertEqual(int(event1->m_type), int(midi::event_type::set_tempo), "First track event is set temp");
        std::cout << "Second event: " << *midi::parse_event(file) << std::endl;
    }
}
