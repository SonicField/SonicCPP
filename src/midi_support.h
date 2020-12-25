#include "sonic_field.h"

namespace sonic_field
{
    namespace midi
    {
        constexpr const char* TYPE_MThd = "MThd";
        constexpr const char* TYPE_MTrk = "MTrk";
        enum chunk_type {
            header_type,
            track_type
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
            std::vector<uint8_t> m_data;

            // If not correctly initialized this method will just raise and exception.
            uint8_t type()
            {
                return m_data.at(0);
            }
        };

        std::ostream& operator << (std::ostream& out, const chunk& c);
        std::ostream& operator << (std::ostream& out, const header& h);
        std::ostream& operator << (std::ostream& out, const event& e);

        chunk read_chunk(std::istream& input);
        chunk_type type_of_chunk(const chunk&);

        header read_header(std::istream& input);
        bool is_smtpe(const header& h);
        int8_t smtpe_type(const header& h);

        void read_midi_file(const std::string&);
    }
}