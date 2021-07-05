#include "sonic_field.h"

namespace sonic_field
{
    namespace midi
    {
        constexpr const char* TYPE_MThd = "MThd";
        constexpr const char* TYPE_MTrk = "MTrk";
        enum struct chunk_type : uint32_t
        {
            header,
            track
        };

        enum struct event_type : uint32_t
        {
            set_tempo
        };

        enum struct event_code_full : uint8_t
        {
            sys_exclusive         = 0b11110000,
            song_position_pointer = 0b11110001,
            song_select           = 0b11110011,
            tune_request          = 0b11110110,
            end_of_exclusive      = 0b11110111,
            timing_clock          = 0b11111000,
            start                 = 0b11111010,
            cont                  = 0b11111011, // sorteded as continue is a reserved word.
            stop                  = 0b11111100,
            active_sensing        = 0b11111110,
            meta_event            = 0b11111111  // Also is reset but not in files.
        };

        enum struct meta_code : uint8_t
        {
            copy_right      = 0x02,
            track_name      = 0x03,
            instrument_name = 0x04,
            lyric           = 0x05,
            marker          = 0x06,
            cue_point       = 0x07,
            channel_prefix  = 0x20,
            end_of_track    = 0x2F,
            set_tempo       = 0x51,
            smpte_offset    = 0x54,
            time_signature  = 0x58,
            key_signature   = 0x59,
            sequencer_only  = 0x7F
        };

        // No encapsulation, just use raw data fields for simplicity.
        // The higher level interface will then encapsulate music objects
        // derived from processing these envents.
        struct chunk
        {
            char m_type[4];
            uint32_t m_size;

        };

        struct header
        {
            chunk m_chunk;
            uint16_t m_format;
            uint16_t m_ntrks;
            uint16_t m_division;
        };

        struct event
        {
            uint32_t m_offset;
            event_type m_type;
            event(uint32_t offset, event_type ty): m_offset{offset}, m_type{ty}{}
            virtual std::string to_string() const = 0;
            virtual ~event(){}
        };

        typedef std::shared_ptr<event> event_ptr;

        struct event_set_tempo: event
        {
            uint32_t m_ms_per_quater;
            event_set_tempo(uint32_t offset, uint32_t ms_per_quater);
            std::string to_string() const override;
        };

        // Event parsing functor.
        struct event_parser
        {
            virtual event_ptr operator()(std::istream& input) const = 0;
        };

        struct meta_parser: event_parser
        {
            virtual event_ptr operator()(std::istream& input) const override;
        };

        struct set_tempo_parser: event_parser
        {
            event_ptr operator()(std::istream& input) const override;
        };

        std::ostream& operator << (std::ostream& out, const chunk_type& ct);
        std::ostream& operator << (std::ostream& out, const chunk& c);
        std::ostream& operator << (std::ostream& out, const header& h);
        std::ostream& operator << (std::ostream& out, const event& e);

        chunk read_chunk(std::istream& input);
        chunk_type type_of_chunk(const chunk&);

        header read_header(std::istream& input);
        bool is_smtpe(const header& h);
        int8_t smtpe_type(const header& h);
        void read_midi_file(const std::string&);
        // event is polymorphic and which one you get can be figured out from the
        // m_type member. This is why we pass them by shared_ptr. Nothing in this library
        // is performance centric so shared_ptr is fine.
        event_ptr parse_event(std::istream& input);

        // TODO - delete - expose for testing only.
        uint8_t read_uint8(std::istream& input);
        uint32_t read_vlq(std::istream& input);

    }
}
