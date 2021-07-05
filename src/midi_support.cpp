#include "midi_support.h"
#include <fstream>
#include <ios>
#include <type_traits>
#include <unordered_map>

namespace sonic_field
{
    namespace midi
    {
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
            out << "MIDI_EVENT{" << e.to_string() << "}";
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

        event_set_tempo::event_set_tempo(uint32_t offset, uint32_t ms_per_quater):
            event{offset, event_type::set_tempo},
            m_ms_per_quater{ms_per_quater}
        {}

        std::string event_set_tempo::to_string() const
        {
            return "set_tempo=" + std::to_string(m_ms_per_quater);
        }

        event_ptr meta_parser::operator()(std::istream& input) const
        {
            SF_MARK_STACK;
            static std::unordered_map<meta_code, event_parser*> code_map
            {
                {meta_code::set_tempo, new set_tempo_parser{}}
            };
            auto code = read_uint8(input);
            auto found = code_map.find(meta_code{code});
            if (found == code_map.end())
            {
                SF_THROW(std::invalid_argument{"meta code " + std::to_string(code) + " not found"});
            }
            return (*found->second)(input);
        }

        event_ptr set_tempo_parser::operator()(std::istream& input) const
        {
            SF_MARK_STACK;
            SF_THROW(std::logic_error{"Not implemented"});
        }

        event_ptr parse_event(std::istream& input)
        {
            SF_MARK_STACK;
            // Read the offset as a vlq.
            auto offset = read_vlq(input);

            // Map a full even code to an event parser. Should the parser not be found in this map
            // then we try the message map.  Note that we could use a switch statement for dispatch
            // here; I did it this way because it appealed to me - a switch is probably a better option
            // but - this code is for fun :)
            static std::unordered_map<event_code_full, event_parser* > full_map
            {
                {event_code_full::meta_event, new meta_parser{}}
            };
            auto code = read_uint8(input);
            auto found = full_map.find(event_code_full{code});
            if (found == full_map.end())
            {
                SF_THROW(std::logic_error{"Only full event codes implemented; found: " + std::to_string(code)});
            }
  
            // Create the event with a zero offset then set the offset on return.
            auto ret_event = (*(found->second))(input);
            ret_event->m_offset = offset;
            return ret_event;
        }
    }
}
