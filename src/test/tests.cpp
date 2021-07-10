#include "tests.h"
#include "../midi_support.h"
#include "../comms.h"

namespace sonic_field
{
    test_runner::test_runner(const std::string& data_dir) :
        m_failed{ 0 }, m_ran{ 0 }, m_data_dir{ data_dir }
    {
        // run tests here.
        std::cout << "Running tests with data dir=" << m_data_dir << std::endl;

        try_run("Dummy test", [] { std::cout << "Dummy test" << std::endl; });
        try_run("Midi test a", [&] { test_midi_a(m_data_dir); });
        //try_run("Comms tests", [&] { test_comms(); });
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
        // tempo which maybe has something to do with all this?
        std::cout << "header: " << h << std::endl;

        test_header("Correct first track chunk");
        auto tc = midi::read_chunk(file);
        assertEqual(type_of_chunk(tc), midi::chunk_type::track, "track chunk type");
        std::cout << "Track: " << tc << std::endl;

        auto event1 =  midi::parse_event(file);
        test_header("Correct track events");
        std::cout << "First event: " << *event1 << std::endl;
        assertEqual(int(event1->m_type), int(midi::event_type::tempo), "First track event is set tempo");
        assertEqual(dynamic_cast<midi::event_tempo*>(event1.get())->m_ms_per_quater, 500000, "Expected tempo");

        auto event2 =  midi::parse_event(file);
        std::cout << "Second event: " << *event2 << std::endl;
        assertEqual(int(event2->m_type), int(midi::event_type::key_signature),
                "Second track event is set key signature");
        assertEqual(dynamic_cast<midi::event_key_signature*>(event2.get())->m_flats_sharps, 0, "Expected flats/sharps");
        assertEqual(dynamic_cast<midi::event_key_signature*>(event2.get())->m_major_minor,  0, "Expected major/minor");

        auto event3 =  midi::parse_event(file);
        std::cout << "Third event: " << *event3 << std::endl;
        auto event4 =  midi::parse_event(file);
        std::cout << "Fourth event: " << *event4 << std::endl;
        while(true)
        {
            auto event5 =  midi::parse_event(file);
            std::cout << "Fith event: " << *event5 << std::endl;
        }
    }

    void test_comms()
    {
        comms::run_tests();
    }
}
