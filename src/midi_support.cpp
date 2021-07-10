#include "midi_support.h"
#include <fstream>
#include <ios>
#include <type_traits>
#include <unordered_map>

namespace sonic_field
{
    namespace midi
    {
        template<typename T, typename... P>
        inline void _em(T& t, P&&... p)
        {
            t.emplace(std::forward<decltype(p)>(p)...);
        }

        class stream_state_save
        {
            std::ostream& m_stream;
            std::ios_base::fmtflags m_flags;
        public:
            stream_state_save(std::ostream& o) :
                m_stream{ o }, m_flags{ o.flags() }
            {}
            ~stream_state_save()
            {
                m_stream.flags(m_flags);
            }
        };

        std::ostream& operator << (std::ostream& out, const chunk_type& ct)
        {
            SF_MARK_STACK;
            switch(ct)
            {
                case chunk_type::header:
                    out << "CHUNK_TYPE_HEADER";
                    break;
                case chunk_type::track:
                    out << "CHUNK_TYPE_TRACK";
                    break;
                default:
                    // The type system means this should never happen - but I guess you never know, because
                    // of code rot.
                    SF_THROW(std::invalid_argument{"Unknown chunk type: " +
                        std::to_string(int(ct))});
            }
            return out;
        }

        std::ostream& operator << (std::ostream& out, const chunk& c)
        {
            SF_MARK_STACK;
            stream_state_save s{ out };
            //std::ios_base::fmtflags f{out.flags()};
            out << std::hex;
            out << "MIDI_CHUNK{m_type: " << std::string{ c.m_type, 4 };
            out << std::dec;
            out << ", m_size: " << c.m_size << "}";
            return out;
        }

        std::ostream& operator << (std::ostream& out, const header& h)
        {
            SF_MARK_STACK;
            stream_state_save s{ out };
            out << "MIDI_HEADER{m_chunk: " << h.m_chunk;
            out << std::hex;
            out << ", m_format: " << h.m_format;
            out << std::dec;
            out << ", m_ntrks: " << h.m_ntrks;
            out << std::hex;
            out << ", m_division: " << h.m_division << "}";
            return out;
        }

        std::ostream& operator << (std::ostream& out, const event& e)
        {
            SF_MARK_STACK;
            stream_state_save s{ out };
            out << "MIDI_EVENT@" + std::to_string(e.m_offset) + "{" << e.to_string() << "}";
            return out;
        }

        inline void safe_read(std::istream& input, char* into, uint64_t len)
        {
            input.read(into, len);
            if (!input) SF_THROW(std::invalid_argument("End Of File whilst reading midi"));
        }

        uint32_t read_uint32(std::istream& input) 
        {
            SF_MARK_STACK;
            uint32_t ret{ 0 };
            char element;
            safe_read(input, &element, 1);
            for (int idx{ 0 }; idx < 3; ++idx)
            {
                ret <<= 8;
                safe_read(input, &element, 1);
                ret |= element & 0xFF;
            }
            return ret;
        }

        uint16_t read_uint16(std::istream& input)
        {
            SF_MARK_STACK;
            uint16_t ret{ 0 };
            char element;
            safe_read(input, &element, 1);
            ret = element << 8;
            safe_read(input, &element, 1);
            ret |= element;
            return ret;
        }

        uint8_t read_uint8(std::istream& input)
        {
            SF_MARK_STACK;
            char ret{ 0 };
            safe_read(input, &ret, 1);
            return uint8_t(ret);
        }

        uint32_t read_vlq(std::istream& input)
        {
            SF_MARK_STACK;
            uint32_t ret{ 0 };
            char element;
            safe_read(input, &element, 1);
            while (element & 0x80)
            {
                ret |= element & 0x7F;
                ret <<= 7;
                safe_read(input, &element, 1);
            }
            ret |= element & 0x7F;
            return ret;
        }

        chunk read_chunk(std::istream& input)
        {
            SF_MARK_STACK;
            chunk ret{};
            safe_read(input, ret.m_type, 4);
            ret.m_size = read_uint32(input);
            std::cerr << "Midi read chunk: " << ret << std::endl;
            return ret;
        }

        chunk_type type_of_chunk(const chunk& c)
        {
            SF_MARK_STACK;
            static auto hd = *reinterpret_cast<const uint32_t*>(TYPE_MThd);
            static auto hr = *reinterpret_cast<const uint32_t*>(TYPE_MTrk);
            auto tp = *reinterpret_cast<const uint32_t*>(c.m_type);
            if (tp == hd) return chunk_type::header;
            if (tp == hr) return chunk_type::track;
            SF_THROW(std::invalid_argument{ "Unknown chunk type: " + std::string{c.m_type, 4} });
        }

        header read_header(std::istream& input)
        {
            SF_MARK_STACK;
            header ret{};
            ret.m_chunk = read_chunk(input);
            if (type_of_chunk(ret.m_chunk) != chunk_type::header)
                SF_THROW(std::invalid_argument{ "Expected header chunk got: " + std::string{ret.m_chunk.m_type, 4} });
            if (ret.m_chunk.m_size != 6)
                SF_THROW(std::out_of_range{ "Expected header size to be 6 was: " + std::to_string(ret.m_chunk.m_size) });
            ret.m_format = read_uint16(input);
            if (ret.m_format > 2)
                SF_THROW(std::invalid_argument{ "Unrecognized format: " + std::to_string(ret.m_format) });
            ret.m_ntrks = read_uint16(input);
            if (ret.m_format == 0 && ret.m_ntrks != 1)
                SF_THROW(std::out_of_range{ "Format 0 expects 1 track, asked for: " + std::to_string(ret.m_ntrks) });
            ret.m_division = read_uint16(input);
            std::cerr << "Midi read header: " << ret << std::endl;
            return ret;
        }

        bool is_smtpe(const header& h)
        {
            return h.m_division & 0x8000;
        }

        int8_t smtpe_type(const header& h)
        {
            return (h.m_division >> 8) & 0x7f;
        }

        void read_midi_file(const std::string& name)
        {
            SF_MARK_STACK;
            std::cerr << "Reading midi: " << name << std::endl;
            std::ifstream file{ name, std::ios_base::in | std::ios_base::binary };
            read_header(file);
        }

        event_tempo::event_tempo(uint32_t offset, uint32_t ms_per_quater):
            event{offset, event_type::tempo},
            m_ms_per_quater{ms_per_quater}
        {}

        std::string event_tempo::to_string() const
        {
            return "tempo=" + std::to_string(m_ms_per_quater);
        }

        event_key_signature::event_key_signature(uint32_t offset, int8_t flats_sharps, uint8_t major_minor):
            event{offset, event_type::key_signature},
            m_flats_sharps{flats_sharps},
            m_major_minor{major_minor}
        {}

        std::string event_key_signature::to_string() const
        {
            return "key_signature=" + std::to_string(m_flats_sharps) + "/" + std::to_string(m_major_minor);
        }

        event_time_signature::event_time_signature(
                    uint32_t offset,
                    uint8_t numerator,
                    uint8_t denominator,
                    uint8_t clocks_per_tick,
                    uint8_t thirty_two_in_quater):
            event{offset, event_type::key_signature},
            m_numerator{numerator},
            m_denominator{denominator},
            m_clocks_per_tick{clocks_per_tick},
            m_thirty_two_in_quater{thirty_two_in_quater}
        {}

        std::string event_time_signature::to_string() const
        {
            return "time_signature=" + std::to_string(m_numerator) + "/" + std::to_string(m_denominator) +\
                "," + std::to_string(m_clocks_per_tick) + "," + std::to_string(m_thirty_two_in_quater);
        }

        event_ptr meta_parser::operator()(std::istream& input) const
        {
            SF_MARK_STACK;
            static auto code_map = []
            {
                using k_t = meta_code;
                using v_t = std::unique_ptr<event_parser>;

                std::unordered_map<k_t, v_t> m{};
                _em(m, meta_code::tempo, v_t{new tempo_parser{}});
                _em(m, meta_code::key_signature, v_t{new key_signature_parser{}});
                _em(m, meta_code::copyright, v_t{new copyright_parser{}});
                _em(m, meta_code::track_name, v_t{new track_name_parser{}});
                _em(m, meta_code::instrument_name, v_t{new instrument_name_parser{}});
                _em(m, meta_code::marker, v_t{new marker_parser{}});
                _em(m, meta_code::cue_point, v_t{new cue_point_parser{}});
                _em(m, meta_code::time_signature, v_t{new time_signature_parser{}});
                return m;
            }();

            auto code = read_uint8(input);
            auto found = code_map.find(meta_code{code});
            // If we have not found a parser then we can 'ignore' the event and create an unknown.
            // This will be a text_event and have a text field but that will have been rationalized into
            // printable ASCII (with . replacing unprintable) so it will still be safe to use.
            if (found == code_map.end())
            {
                static event_parser* unknown = new meta_unknown_parser{};
                return unknown->operator()(input);
            }
            // We have a specific parser for this meta so parse it.
            return found->second->operator()(input);
        }

        event_ptr tempo_parser::operator()(std::istream& input) const
        {
            SF_MARK_STACK;
            auto check_value = read_uint8(input);
            if (check_value != 0x03)
            {
                SF_THROW(std::invalid_argument{
                        "set tempo second byte expected 0x03 got: " + std::to_string(check_value)});
            }
            uint32_t ms_per_quater = 0;
            // The next three bytes are the value big endian.
            ms_per_quater += read_uint8(input);
            ms_per_quater <<= 8;
            ms_per_quater += read_uint8(input);
            ms_per_quater <<= 8;
            ms_per_quater += read_uint8(input);
            return event_ptr{new event_tempo{0, ms_per_quater}};
        }

        event_ptr key_signature_parser::operator()(std::istream& input) const
        {
            SF_MARK_STACK;
            auto check_value = read_uint8(input);
            if (check_value != 0x02)
            {
                SF_THROW(std::invalid_argument{"set key signature second byte expected 0x02 got: " +
                        std::to_string(check_value)});
            }
            int8_t sharps_flats = static_cast<int8_t>(read_uint8(input));
            uint8_t major_minor = read_uint8(input);
            return event_ptr{new event_key_signature{0, sharps_flats, major_minor}};
        }

        event_ptr time_signature_parser::operator()(std::istream& input) const
        {
            SF_MARK_STACK;
            auto check_value = read_uint8(input);
            if (check_value != 0x04)
            {
                SF_THROW(std::invalid_argument{"set time signature second byte expected 0x04 got: " +
                        std::to_string(check_value)});
            }
            return event_ptr{new event_time_signature{0, read_uint8(input), read_uint8(input), read_uint8(input),
                read_uint8(input)}};
        }

        std::string parse_text_field(std::istream& input)
        {
            auto len = read_vlq(input);
            // The compiler finds an ambiguity on initialization if we use the standard {}
            // syntax here - falling back to () fixes this but it is confusing to say the least.
            std::string text(len, '\0');
            safe_read(input, text.data(), len);
            // Ensure the text is ASCII by forcing anything out of text range to be '.'.
            for(size_t i{0}; i<text.size(); ++i)
            {
                if (text[i] < ' ' || text[i] > 127)
                    text[i] = '.';
            }
            return text;
        }

        event_ptr parse_event(std::istream& input)
        {
            SF_MARK_STACK;
            // Read the offset as a vlq.
            auto offset = read_vlq(input);

            // Map a full event code to an event parser. Should the parser not be found in this map
            // then we try the message map.  Note that we could use a switch statement for dispatch
            // here; I did it this way because it appealed to me - a switch is probably a better option
            // but - this code is for fun :)
            static auto full_map = []
            {
                using k_t = event_code_full;
                using v_t = std::unique_ptr<event_parser>;
                std::unordered_map<k_t, v_t > m{};
                _em(m, k_t::meta_event, v_t{new meta_parser{}});
                return m;
            }();

            // Map channel events using the top 4 bits for the key then we get the channel from the bottom
            // four bits.
            static auto channel_map = []
            {
                using k_t = event_code_msg;
                using v_t = std::unique_ptr<channel_msg_event_parser>;
                std::unordered_map<k_t, v_t> m{};
                _em(m, k_t::note_on, v_t{new note_on_event_parser{}});
                return m;
            }();

            auto code = read_uint8(input);
            auto found = full_map.find(event_code_full{code});
            if (found == full_map.end())
            {
                SF_THROW(std::logic_error{"Only full event codes implemented; found: " + std::to_string(code)});
            }
  
            // Create the event with a zero offset then set the offset on return.
            auto ret_event = found->second->operator()(input);
            ret_event->m_offset = offset;
            return ret_event;
        }
    }
}
