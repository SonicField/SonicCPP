#pragma once
#include <sstream>
#include <array>
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
            meta_unknown,
            note_off,
            note_on,
            key_pressure,
            control,
            program,
            channel_pressure,
            pitch,
            sys_exclusive,
            song_position_pointer,
            song_select,
            tune_request,
            end_of_exclusive,
            timing_clock,
            start,
            cont,
            stop,
            active_sensing,
            end_of_track,
            invalid
        };

        inline std::string event_type_to_string(event_type t)
        {
            switch(t)
            {
                case event_type::tempo:                 return "tempo";
                case event_type::key_signature:         return "key_signature";
                case event_type::copyright:             return "copyright";
                case event_type::track_name:            return "track_name";
                case event_type::instrument_name:       return "instrument_name";
                case event_type::lyric:                 return "lyric";
                case event_type::marker:                return "marker";
                case event_type::cue_point:             return "cue_point";
                case event_type::meta_unknown:          return "meta_unknown";
                case event_type::note_off:              return "note_off";
                case event_type::note_on:               return "note_on";
                case event_type::key_pressure:          return "key_pressure";
                case event_type::control:               return "control";
                case event_type::program:               return "program";
                case event_type::channel_pressure:      return "channel_pressure";
                case event_type::pitch:                 return "pitch";
                case event_type::sys_exclusive:         return "sys_exclusive";
                case event_type::song_position_pointer: return "song_position_pointer";
                case event_type::song_select:           return "song_select";
                case event_type::tune_request:          return "tune_requests";
                case event_type::end_of_exclusive:      return "end_of_exclusive";
                case event_type::timing_clock:          return "timing_clock";
                case event_type::start:                 return "start";
                case event_type::cont:                  return "cont";
                case event_type::stop:                  return "stop";
                case event_type::active_sensing:        return "active_sensing";
                case event_type::end_of_track:          return "end_of_track";
                case event_type::invalid:               return "invalid";
                default: SF_THROW(std::invalid_argument{
                                 "Unknown msg type: " +
                                 std::to_string(static_cast<std::underlying_type_t<event_type>>(t))});
            }
        }

        inline std::string event_type_to_string(uint8_t t)
        {
            return event_type_to_string(static_cast<event_type>(t));
        }


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
            note_off         = 0b10000000, // 2
            note_on          = 0b10010000, // 2
            key_pressure     = 0b10100000, // 2
            control          = 0b10110000, // 2
            program          = 0b11000000, // 1
            channel_pressure = 0b11010000, // 1
            pitch            = 0b11100000, // 2
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

        // If the passed type is something which is makes sense to show as hex this will
        // otherwise just return the string representation via the default stringstring representation.
        inline auto to_hex(auto&& x)
        {
            std::stringstream ss{};
            if constexpr (std::is_integral<decltype(x)>::value)
                ss << "0x" << std::hex << x;
            // We tread char and uchar as integer values here as uint8_t and int8_t are uchar and char
            // respectively.
            else if constexpr (std::is_same<typename std::remove_cvref<decltype(x)>::type, uint8_t>::value)
                ss << "0x" << std::hex << int(x);
            else
                ss << x;
            return ss.str();
        }

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

        using event_code = uint8_t;
        struct event
        {
            // Time offset in midi ticks.
            uint32_t m_offset;
            // Midi 8 bit code which is used to figuring out continuation events in parsing.
            event_code m_code;
            // Internal type of the event
            event_type m_type;

            event(uint32_t offset, event_type ty): m_offset{offset}, m_type{ty}{}
            virtual std::string to_string() const = 0;
            virtual ~event(){}
        };

        using event_ptr = std::shared_ptr<event>;

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

        struct event_end_of_track: event
        {
            using event::event;
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

        template<event_type C, size_t N>
        struct msg_event: event
        {
            static constexpr auto size = N;
            std::array<uint8_t, N> m_data;
            msg_event(uint32_t offset, std::array<uint8_t, N>&& data):
                event{offset, C},
                m_data{std::forward<decltype(data)>(data)}
            {}

            virtual std::string to_string() const override
            {
                std::stringstream ret{};
                ret << "message_" << event_type_to_string(m_type) << ": ";
                bool first{true};
                for(const auto v: m_data)
                {
                    static_assert(std::is_same<decltype(v), const uint8_t>::value);
                    if (first)
                        first = false;
                    else
                        ret << "/";
                    ret  << to_hex(v);
                }
                return ret.str();
            }
        };

        template<event_type C, size_t N>
        struct channel_msg_event: msg_event<C, N>
        {
            uint8_t m_channel;
            channel_msg_event(uint32_t offset, std::array<uint8_t, N>&& data, uint8_t channel):
                msg_event<C, N>{offset, std::forward<decltype(data)>(data)},
                m_channel{channel}
            {}

            virtual std::string to_string() const override
            {
                return msg_event<C, N>::to_string() + "#" + std::to_string(m_channel);
            }
        };

        struct note_off_event: channel_msg_event<event_type::note_off, 2>
        {
            using channel_msg_event::channel_msg_event;
        };
        struct note_on_event: channel_msg_event<event_type::note_on, 2>
        {
            using channel_msg_event::channel_msg_event;
        };
        struct key_pressure_event: channel_msg_event<event_type::key_pressure, 2>
        {
            using channel_msg_event::channel_msg_event;
        };
        struct control_event: channel_msg_event<event_type::control, 2>
        {
            using channel_msg_event::channel_msg_event;
        };
        struct program_event: channel_msg_event<event_type::program, 1>
        {
            using channel_msg_event::channel_msg_event;
        };
        struct channel_pressure_event: channel_msg_event<event_type::channel_pressure, 1>
        {
            using channel_msg_event::channel_msg_event;
        };
        struct pitch_pressure_event: channel_msg_event<event_type::pitch, 1>
        {
            using channel_msg_event::channel_msg_event;
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
            event_ptr operator()(std::istream& input) const override
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

        struct end_of_track_parser: event_parser
        {
            event_ptr operator()(std::istream& input) const override;
        };

        // Event parsing functor.
        struct channel_msg_event_parser
        {
            virtual event_ptr operator()(std::istream& input, uint8_t channle) const = 0;
            // Virtual destructor to ensure the dispatch machinery is deleted correctly.
            virtual ~channel_msg_event_parser(){} 
        };

        uint8_t read_uint8(std::istream& input);
        template<typename E>
        struct channel_msg_event_parser_imp: channel_msg_event_parser
        {
            event_ptr operator()(std::istream& input, uint8_t channel) const override
            {
                SF_MARK_STACK;
                std::array<uint8_t, E::size> data{};
                for(size_t i{0}; i < E::size; ++i)
                {
                    data[i] = read_uint8(input);
                }
                return event_ptr{new E{0, std::move(data), channel}};
            }
        };

        struct note_on_event_parser: channel_msg_event_parser_imp<note_on_event>{};
        struct note_off_event_parser: channel_msg_event_parser_imp<note_off_event>{};
        struct key_pressure_event_parser: channel_msg_event_parser_imp<key_pressure_event>{};
        struct control_event_parser: channel_msg_event_parser_imp<control_event>{};
        struct program_event_parser: channel_msg_event_parser_imp<program_event>{};
        struct channel_pressure_event_parser: channel_msg_event_parser_imp<channel_pressure_event>{};
        struct pitch_pressure_event_parser: channel_msg_event_parser_imp<pitch_pressure_event>{};

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
        event_ptr parse_event(std::istream& input, event_code prev_code=0);

        uint32_t read_vlq(std::istream& input);
    }
}
