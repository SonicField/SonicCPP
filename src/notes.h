#include "sonic_field.h"

namespace sonic_field
{
    namespace notes
    {
        enum struct envelope_type : uint32_t
        {
            amplitude,
            pitch,
            pan
        };

        std::string envelope_type_to_str(envelope_type t)
        {
            switch(t)
            {
                case envelope_type::amplitude:
                    return "amplitude";
                case envelope_type::pitch:
                    return "pitch";
                default:
                    return "unknown: " + std::to_string(uint32_t(t));
            }
        }

        // A note is a constrained set of envelopes to represent a sound.
        // It must have a pitch and amplitude envelope, all others are optional.
        // All envelopes must start and end at the same position but do not need
        // to have the same number of elements.
        // See envelope_type.
        class note
        {
            std::unordered_map<envelope_type, envelope> m_envelopes;
        public:
            note(std::unordered_map<envelope_type, envelope> envs);

            uint64_t start() const;

            uint64_t end() const;

            bool has_envelope(envelope_type t) const;

            void must_have_envelope(envelope_type t) const;

            envelope get_envelope(envelope_type t) const;
                
        };

        using voice = std::vector<note>;
        using composition = std::vector<voice>;
    }
}
