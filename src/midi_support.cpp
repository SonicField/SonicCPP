#include "midi_support.h"
#include <fstream>
#include <ios>

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
            out << std::dec;
            out << "MIDI_EVENT{m_m_offset: " << e.m_offset;
            out << ", m_data{";
            out << std::hex;
            auto cma = false;
            for (auto d : e.m_data)
            {
                if (cma)
                    out << ", ";
                else
                    cma = true;
                out << d;
            }
            out << "}}";
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
            safe_read(input, &element, 1);
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
            if (tp == hd) return header_type;
            if (tp == hr) return track_type;
            SF_THROW(std::invalid_argument{ "Unknown chunk type: " + std::string{c.m_type, 4} });
        }

        header read_header(std::istream& input)
        {
            SF_MARK_STACK;
            header ret{};
            ret.m_chunk = read_chunk(input);
            if (type_of_chunk(ret.m_chunk) != header_type)
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
    }
}
