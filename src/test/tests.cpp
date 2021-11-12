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
        void test_midi_tracks(const std::string&);
        void test_dump_midi(const std::string&, const std::string&);
        void test_midi_notes_1(const std::string&);
        void test_midi_notes_2(const std::string&);
        void test_midi_notes_3(const std::string&);
        void test_midi_notes_4(const std::string&);
    }

    test_runner::test_runner(const std::string& data_dir) :
        m_failed{ 0 }, m_ran{ 0 }, m_data_dir{ data_dir }
    {
        // run tests here.
        std::cout << "Running tests with data dir=" << m_data_dir << std::endl;

        //try_run("Dummy test", [] { std::cout << "Dummy test" << std::endl; });
        //try_run("Test tests", [&] { test_tests(); });
        //try_run("Comms tests", [&] { test_comms(); });
        //try_run("Midi smoke tests", [&] { test_midi_smoke(m_data_dir); });
        //try_run("Notes tests", [&] { notes::test_notes(); });
        // This one is not really a test but an easy way to look into a midi track.
        try_run("Midi dump", [&] { notes::test_dump_midi(m_data_dir, "Test-Notes-4.mid"); });
        //try_run("Midi track tests", [&] { notes::test_midi_tracks(m_data_dir); });
        //try_run("Midi note tests 1", [&] { notes::test_midi_notes_1(m_data_dir); });
        //try_run("Midi note tests 2", [&] { notes::test_midi_notes_2(m_data_dir); });
        //try_run("Midi note tests 3", [&] { notes::test_midi_notes_3(m_data_dir); });
        //try_run("Midi note tests 4", [&] { notes::test_midi_notes_4(m_data_dir); });
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
        assert_equal(dynamic_cast<midi::event_tempo*>(event1.get())->m_us_per_quater, 500000, "Expected tempo");

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

    void notes::test_dump_midi(const std::string& data_dir, const std::string& file_name)
    {
        SF_MARK_STACK;
        auto inp = join_path({ data_dir , file_name });
        midi_file_reader reader{inp};
    }

    void notes::test_midi_tracks(const std::string& data_dir)
    {
        SF_MARK_STACK;
        auto inp = join_path({ data_dir , "Test-Track-Reader-1.mid" });
        midi_file_reader reader{inp};
        assert_equal(reader.track_count(), 4, "Correct number of tracks");
        auto track0 = reader.track(0);
        auto track1 = reader.track(1);
        auto track2 = reader.track(2);
        auto track3 = reader.track(3);
        assert_equal(track0.size(), 5, "Track zero correct size");
        assert_equal(track1.size(), 9, "Track one correct size");
        assert_equal(track2.size(), 9, "Track two correct size");
        assert_equal(track3.size(), 9, "Track three correct size");
        auto merged = merge_midi_tracks({track0, track1});
        assert_equal(merged.size(), 13, "Merged track correct size");
        auto end_count = std::count_if(merged.begin(), merged.end(), [](const auto& e)
        {
            return e->m_type == midi::event_type::end_of_track;
        });
        assert_equal(end_count, 1, "Merged end_of_track dedupe worked");
        uint64_t offset{0};
        merged = merge_midi_tracks({track0, track1, track2, track3});
        for(const auto& e: merged)
        {
            assert_less_or_equal(offset, e->m_offset, "Events are in ascending order");
            offset = e->m_offset;
        }
    }

    void notes::test_midi_notes_1(const std::string& data_dir)
    {
        SF_MARK_STACK;
        auto inp = join_path({ data_dir , "Test-Notes-1.mid" });
        midi_file_reader reader{inp};
        assert_equal(reader.track_count(), 2, "Correct number of tracks");
        auto track0 = reader.track(0);
        auto track1 = reader.track(1);
        auto merged = merge_midi_tracks({track0, track1});
        assert_equal(merged.size(), 17, "Merged track correct size");
        track_notes notes{merged, 6000, equal_temperament{}};
        assert_equal(notes.size(), 3, "Three notes created");
        for(const auto& note: notes)
        {
            auto pth_env = note.get_envelope(envelope_type::pitch);
            std::cerr << "PITCH: " << pth_env[0].amplitude() << " @: " << pth_env[0].position() << std::endl;
            assert_equal(pth_env.size(), 2, "Pitch env correct length");
            assert_equal(pth_env[0].amplitude(), pth_env[1].amplitude(), "Pitch env flat");

            auto amp_env = note.get_envelope(envelope_type::amplitude);
            std::cerr << "AMPLI: " << amp_env[0].amplitude() << " @: " << amp_env[0].position() << std::endl;
            assert_equal(amp_env.size(), 2, "Amplitude env correct length");
            assert_equal(scale_10000(amp_env[0].amplitude()), 6299, "Correct amplitude - start");
            assert_equal(scale_10000(amp_env[1].amplitude()), 6299, "Correct amplitude - end");
        }
        assert_equal(scale_1000(notes[0].get_envelope(envelope_type::pitch)[0].amplitude()),
                440000, "Note 0 pitch correct");
        assert_equal(scale_1000(notes[1].get_envelope(envelope_type::pitch)[0].amplitude()),
                220000, "Note 0 pitch correct");
        assert_equal(scale_1000(notes[2].get_envelope(envelope_type::pitch)[0].amplitude()),
                261626, "Note 0 pitch correct");
        assert_equal(scale_1(notes[0].get_envelope(envelope_type::amplitude)[0].position()),
                0, "Note 0 start");
        assert_equal(scale_1(notes[0].get_envelope(envelope_type::amplitude)[1].position()),
                500, "Note 0 end");
        assert_equal(scale_1(notes[1].get_envelope(envelope_type::amplitude)[0].position()),
                2000, "Note 1 start");
        assert_equal(scale_1(notes[1].get_envelope(envelope_type::amplitude)[1].position()),
                2500, "Note 1 end");
        assert_equal(scale_1(notes[2].get_envelope(envelope_type::amplitude)[0].position()),
                4000, "Note 2 start");
        assert_equal(scale_1(notes[2].get_envelope(envelope_type::amplitude)[1].position()),
                4500, "Note 2 end");
    }

    void notes::test_midi_notes_2(const std::string& data_dir)
    {
        SF_MARK_STACK;
        auto inp = join_path({ data_dir , "Test-Notes-2.mid" });
        midi_file_reader reader{inp};
        assert_equal(reader.track_count(), 2, "Correct number of tracks");
        auto track0 = reader.track(0);
        auto track1 = reader.track(1);
        auto merged = merge_midi_tracks({track0, track1});
        assert_equal(merged.size(), 21, "Merged track correct size");
        track_notes notes{merged, 10660, equal_temperament{}};
        assert_equal(notes.size(), 4, "Three notes created");
        for(const auto& note: notes)
        {
            auto pth_env = note.get_envelope(envelope_type::pitch);
            std::cerr << "PITCH: " << pth_env[0].amplitude() << " @: " << pth_env[0].position() << std::endl;
            assert_equal(pth_env.size(), 2, "Pitch env correct length");
            assert_equal(pth_env[0].amplitude(), pth_env[1].amplitude(), "Pitch env flat");

            auto amp_env = note.get_envelope(envelope_type::amplitude);
            std::cerr << "AMPLI: " << amp_env[0].amplitude() << " @: " << amp_env[0].position() << std::endl;
            assert_equal(amp_env.size(), 2, "Amplitude env correct length");
            assert_equal(scale_10000(amp_env[0].amplitude()), 6299, "Correct amplitude - start");
            assert_equal(scale_10000(amp_env[1].amplitude()), 6299, "Correct amplitude - end");
        }
        assert_equal(scale_1000(notes[0].get_envelope(envelope_type::pitch)[0].amplitude()),
                440000, "Note 0 pitch correct");
        assert_equal(scale_1000(notes[1].get_envelope(envelope_type::pitch)[0].amplitude()),
                220000, "Note 0 pitch correct");
        assert_equal(scale_1000(notes[2].get_envelope(envelope_type::pitch)[0].amplitude()),
                261626, "Note 0 pitch correct");
        assert_equal(scale_1(notes[0].get_envelope(envelope_type::amplitude)[0].position()),
                0, "Note 0 start");
        assert_equal(scale_1(notes[0].get_envelope(envelope_type::amplitude)[1].position()),
                500, "Note 0 end");
        assert_equal(scale_1(notes[1].get_envelope(envelope_type::amplitude)[0].position()),
                1999, "Note 1 start");
        assert_equal(scale_1(notes[1].get_envelope(envelope_type::amplitude)[1].position()),
                2499, "Note 1 end");
        assert_equal(scale_1(notes[2].get_envelope(envelope_type::amplitude)[0].position()),
                3998, "Note 2 start");
        assert_equal(scale_1(notes[2].get_envelope(envelope_type::amplitude)[1].position()),
                4997, "Note 2 end");
        assert_equal(scale_1(notes[3].get_envelope(envelope_type::amplitude)[0].position()),
                7996, "Note 3 start");
        assert_equal(scale_1(notes[3].get_envelope(envelope_type::amplitude)[1].position()),
                8329, "Note 3 end");
    }

    void notes::test_midi_notes_3(const std::string& data_dir)
    {
        SF_MARK_STACK;
        auto inp = join_path({ data_dir , "Test-Notes-3.mid" });
        midi_file_reader reader{inp};
        assert_equal(reader.track_count(), 2, "Correct number of tracks");
        auto track0 = reader.track(0);
        auto track1 = reader.track(1);
        auto merged = merge_midi_tracks({track0, track1});
        track_notes notes{merged, 7744, equal_temperament{}};
        assert_equal(notes.size(), 2, "Two notes created");
        assert_equal(scale_1(notes[0].get_envelope(envelope_type::amplitude)[0].position()),
                0, "Note 0 start");
        assert_equal(scale_1(notes[0].get_envelope(envelope_type::amplitude)[1].position()),
                3215, "Note 0 end");
        assert_equal(scale_1(notes[1].get_envelope(envelope_type::amplitude)[0].position()),
                5480, "Note 1 start");
        assert_equal(scale_1(notes[1].get_envelope(envelope_type::amplitude)[1].position()),
                6046, "Note 0 end");
    }
}
