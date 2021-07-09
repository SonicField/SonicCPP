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
            tempo,
            key_signature,
            copyright,
            track_name,
            instrument_name,
            lyric,
            marker,
            cue_point,
            meta_unknown
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

        enum struct event_code_msg : uint8_t
        {
            note_off         = 0b1000,
            note_on          = 0b1001,
            key_pressure     = 0b1010,
            control          = 0b1011,
            program          = 0b1100,
            channel_pressure = 0b1101,
            pitch            = 0b1110
        };

        enum struct meta_code : uint8_t
        {
            copyright       = 0x02,
            track_name      = 0x03,
            instrument_name = 0x04,
            lyric           = 0x05,
            marker          = 0x06,
            cue_point       = 0x07,
            channel_prefix  = 0x20,
            end_of_track    = 0x2F,
            tempo           = 0x51,
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

        struct event_tempo: event
        {
            uint32_t m_ms_per_quater;
            event_tempo(uint32_t offset, uint32_t ms_per_quater);
            std::string to_string() const override;
        };

        struct event_key_signature: event
        {
            int8_t m_flats_sharps;
            uint8_t m_major_minor;
            event_key_signature(uint32_t offset, int8_t flats_sharps, uint8_t m_major_minor);
            std::string to_string() const override;
        };

        struct event_time_signature: event
        {
            uint8_t m_numerator;
            uint8_t m_denominator;
            uint8_t m_clocks_per_tick;
            uint8_t m_thirty_two_in_quater;
            event_time_signature(
                    uint32_t offset,
                    uint8_t numerator,
                    uint8_t denominator,
                    uint8_t clocks_per_tick,
                    uint8_t thirty_two_in_quater);
            std::string to_string() const override;
        };

        template<event_type C>
        struct text_event: event
        {
            std::string m_text;
            text_event(uint32_t offset, const std::string& text):
                event{offset, C},
                m_text(text)
            {}

            virtual std::string to_string() const override
            {
                return name() + "='" + m_text + "'";
            }

        protected:
            virtual std::string name() const = 0;
        };

        struct event_copyright: text_event<event_type::copyright>
        {
            using text_event::text_event;
        protected:
            std::string name() const override {return "copyright";}
        };

        struct event_lyric: text_event<event_type::lyric>
        {
            using text_event::text_event;
        protected:
            std::string name() const override {return "lyric";}
        };

        struct event_track_name: text_event<event_type::track_name>
        {
            using text_event::text_event;
        protected:
            std::string name() const override {return "track_name";}
        };

        struct event_instrument_name: text_event<event_type::instrument_name>
        {
            using text_event::text_event;
        protected:
            std::string name() const override {return "instrument_name";}
        };

        struct event_cue_point: text_event<event_type::cue_point>
        {
            using text_event::text_event;
        protected:
            std::string name() const override {return "cue_point";}
        };

        struct event_marker: text_event<event_type::marker>
        {
            using text_event::text_event;
        protected:
            std::string name() const override {return "marker";}
        };

        struct event_meta_unknown: text_event<event_type::meta_unknown>
        {
            using text_event::text_event;
        protected:
            std::string name() const override {return "meta_unknown";}
        };

        template<event_type C>
        struct msg_zero_event: event
        {
            msg_zero_event(uint32_t offset):
                event{offset, C}
            {}

            virtual std::string to_string() const override
            {
                return name();
            }

        protected:
            virtual std::string name() const = 0;
        };

        template<event_type C>
        struct msg_one_event: event
        {
            uint8_t m_one;
            msg_one_event(uint32_t offset, uint8_t one):
                event{offset, C},
                m_one{one}
            {}

            virtual std::string to_string() const override
            {
                return name() + "=" + std::to_string(m_one);
            }

        protected:
            virtual std::string name() const = 0;
        };

        template<event_type C>
        struct msg_two_event: event
        {
            uint8_t m_one;
            uint8_t m_two;
            msg_two_event(uint32_t offset, uint8_t one, uint8_t two):
                event{offset, C},
                m_one{one},
                m_two{two}
            {}

            virtual std::string to_string() const override
            {
                return name() + "=" + std::to_string(m_one) + "," + std::to_string(m_two);
            }

        protected:
            virtual std::string name() const = 0;
        };

        // Event parsing functor.
        struct event_parser
        {
            virtual event_ptr operator()(std::istream& input) const = 0;
            // Virtual destructor to ensure the dispatch machinery is deleted correctly.
            virtual ~event_parser(){} 
        };

        struct meta_parser: event_parser
        {
            virtual event_ptr operator()(std::istream& input) const override;
        };

        struct tempo_parser: event_parser
        {
            event_ptr operator()(std::istream& input) const override;
        };

        struct key_signature_parser: event_parser
        {
            event_ptr operator()(std::istream& input) const override;
        };

        struct time_signature_parser: event_parser
        {
            event_ptr operator()(std::istream& input) const override;
        };

        std::string parse_text_field(std::istream& input);
        template<typename E>
        struct text_event_parser: event_parser
        {
            event_ptr operator()(std::istream& input) const
            {
                SF_MARK_STACK;
                return event_ptr{new E{0, parse_text_field(input)}};
            }
        };

        struct copyright_parser: text_event_parser<event_copyright>{};
        struct track_name_parser: text_event_parser<event_track_name>{};
        struct instrument_name_parser: text_event_parser<event_instrument_name>{};
        struct lyric_parser: text_event_parser<event_lyric>{};
        struct marker_parser: text_event_parser<event_marker>{};
        struct cue_point_parser: text_event_parser<event_cue_point>{};
        struct meta_unknown_parser: text_event_parser<event_meta_unknown>{};

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
