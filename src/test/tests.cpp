#include "tests.h"
#include "../midi_support.h"
#include "../comms.h"
#include "../notes.h"

namespace sonic_field
{
    // Test declarations go here.
    void test_tests();
    void test_midi_smoke(const std::string&);
    void test_comms();
    namespace notes
    {
        void test_notes();
        void test_midi_note(const std::string&);
    }

    test_runner::test_runner(const std::string& data_dir) :
        m_failed{ 0 }, m_ran{ 0 }, m_data_dir{ data_dir }
    {
        // run tests here.
        std::cout << "Running tests with data dir=" << m_data_dir << std::endl;

        //try_run("Dummy test", [] { std::cout << "Dummy test" << std::endl; });
        //try_run("Test tests", [&] { test_tests(); });
        //try_run("Comms tests", [&] { test_comms(); });
        try_run("Midi smoke tests", [&] { test_midi_smoke(m_data_dir); });
        //try_run("Notes tests", [&] { notes::test_notes(); });
        try_run("Midi note tests", [&] { notes::test_midi_note(m_data_dir); });
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

    // Basic midi parser works.
    // More test coverage comes from higher up the stack.
    void test_midi_smoke(const std::string& data_dir)
    {
        SF_MARK_STACK;
        auto inp = join_path({ data_dir , "Test-Midi-Smoke.mid" });
        std::ifstream file{ inp, std::ios_base::in | std::ios_base::binary };
        test_header("Correct header chunk");
        auto h = midi::read_header(file);
        assert_equal(h.m_chunk.m_size, 6, "header chunk size");
        assert_equal(type_of_chunk(h.m_chunk), midi::chunk_type::header, "header chunk type");
        assert_equal(h.m_ntrks, 2, "number of tracks");
        assert_equal(h.m_format, 1, "format");
        assert_equal(is_smtpe(h), true, "is smtpe");
        assert_equal(int(smtpe_type(h)), 127, "smtpe type");
        // m_division comes out as FFC0 which is -1 for the format and 192 resolution.  As this makes no
        // sense to me I can only assume I am doing something wrong here. Interestingly the first event is
        // tempo which maybe has something to do with all this?
        std::cout << "header: " << h << std::endl;

        test_header("Correct first track chunk");
        auto tc = midi::read_chunk(file);
        assert_equal(type_of_chunk(tc), midi::chunk_type::track, "track chunk type");
        std::cout << "Track: " << tc << std::endl;

        auto event1 =  midi::parse_event(file);
        test_header("Correct track events");
        std::cout << "First event: " << *event1 << std::endl;
        assert_equal(int(event1->m_type), int(midi::event_type::tempo), "First track event is set tempo");
        assert_equal(dynamic_cast<midi::event_tempo*>(event1.get())->m_ms_per_quater, 500000, "Expected tempo");

        auto event2 =  midi::parse_event(file);
        std::cout << "Second event: " << *event2 << std::endl;
        assert_equal(int(event2->m_type), int(midi::event_type::key_signature),
                "Second track event is set key signature");
        assert_equal(dynamic_cast<midi::event_key_signature*>(event2.get())->m_flats_sharps, 0, "Expected flats/sharps");
        assert_equal(dynamic_cast<midi::event_key_signature*>(event2.get())->m_major_minor,  0, "Expected major/minor");

        auto event3 =  midi::parse_event(file);
        std::cout << "Third event: " << *event3 << std::endl;
        auto event4 =  midi::parse_event(file);
        std::cout << "Fourth event: " << *event4 << std::endl;
        while(true)
        {
            auto eventx =  midi::parse_event(file);
            std::cout << "X event: " << *eventx << std::endl;
            if (eventx->m_type == midi::event_type::end_of_track)
            {
                std::cout << "End of track" << std::endl;
                break;
            }
        }

        test_header("Correct second track chunk");
        tc = midi::read_chunk(file);
        assert_equal(type_of_chunk(tc), midi::chunk_type::track, "track chunk type");
        std::cout << "Track: " << tc << std::endl;
        midi::event_code prev_code{0};
        while(true)
        {
            auto eventx =  midi::parse_event(file, prev_code);
            prev_code=eventx->m_code;
            std::cout << "X event: " << *(eventx) << std::endl;
            if (eventx->m_type == midi::event_type::end_of_track)
            {
                std::cout << "End of track" << std::endl;
                break;
            }
        }
    }

    void test_comms()
    {
        comms::run_tests();
    }

    void test_tests()
    {
        assert_throws<std::logic_error>(
                []{ throw std::logic_error{"A logic error"};},
                " logic e",
                "Test test_throws");
        assert_throws<assertion_error>(
                []{ assert_equal(1, 2, "Assert Fail Check");},
                "Assert Fail",
                "Test failed assert_equal");
        assert_equal("a", "a", "Assert Equal Pass");
        assert_throws<assertion_error>(
                []
                {
                    assert_throws<std::logic_error>(
                            []{ throw std::logic_error{"A logic error"};},
                            " hairy dogs",
                            "Test test_throws");
                },
                "not found in error message",
                "Check assert_throws checks message");

        assert_throws<assertion_error>(
                []
                {
                    assert_throws<std::invalid_argument>(
                            []{ throw std::logic_error{"A logic error"};},
                            "Assert Fail",
                            "Test test_throws");
                },
                "invalid_argument",
                "Check assert_throws checks type");

    }

    void notes::test_notes()
    {
        using map_t = std::unordered_map<envelope_type, envelope>;
        map_t input{
            {envelope_type::amplitude, {{0, 0}, {100, 1}}},
            {envelope_type::pitch, {{0, 0}, {100, 1}}}
        };
        note n{input};
        assert_equal(n.start(), 0, "Note start correct");
        assert_equal(n.end(), 100.0, "Note end correct");
        assert_equal(n.get_envelope(envelope_type::pitch), envelope{{0, 0}, {100, 1}}, "Get envelope ok");

        assert_true(n.has_envelope(envelope_type::amplitude), "Has amplitude envelope");
        assert_true(n.has_envelope(envelope_type::pitch), "Has pitch envelope");

        assert_throws<std::invalid_argument>(
                [n]{n.must_have_envelope(envelope_type::pan);},
                "Envelope 'pan' not present",
                "Must_have raises approreately");

        assert_throws<std::invalid_argument>(
                [n]{n.get_envelope(envelope_type::pan);},
                "Envelope 'pan' not present",
                "Get raises approreately");

        assert_throws<std::invalid_argument>(
                []{note{map_t{}};},
                "amplitude",
                "Amplitude must be present");

        assert_throws<std::invalid_argument>(
                []{note{map_t{
                    {envelope_type::amplitude, {{0, 0}, {100, 1}}}
                }};},
                "pitch",
                "Pitch must be present");

        assert_throws<std::invalid_argument>(
                []{note{map_t{
                    {envelope_type::amplitude, {{0, 0}, {100, 1}}},
                    {envelope_type::pitch, {{1, 0}, {100, 1}}}
                }};},
                "Envelope starts",
                "Envelope start check");

        assert_throws<std::invalid_argument>(
                []{note{map_t{
                    {envelope_type::amplitude, {{0, 0}, {101, 1}}},
                    {envelope_type::pitch, {{0, 0}, {100, 1}}}
                }};},
                "Envelope ends",
                "Envelope end check");

        assert_throws<std::invalid_argument>(
                []{note{map_t{
                    {envelope_type::amplitude, {{0, 0}, }},
                    {envelope_type::pitch, {{0, 0}, {100, 1}}}
                }};},
                "Needs 2 as",
                "Envelope length check");
    }

    void notes::test_midi_note(const std::string& data_dir)
    {
        SF_MARK_STACK;
        auto inp = join_path({ data_dir , "Test-Track-Reader-1.mid" });
        midi_file_reader reader{inp}; 
        auto track0 = reader.track(0);
        assert_equal(track0.size(), 4, "Track zero correct size");
    }
}
