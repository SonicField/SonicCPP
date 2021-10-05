#include "notes.h"
namespace sonic_field
{
    namespace notes
    {
        note::note(std::unordered_map<envelope_type, envelope> envs):
            m_envelopes{std::move(envs)}
        {
            SF_MARK_STACK;
            // Check the consistency of all the envelopes.
            must_have_envelope(envelope_type::amplitude);
            must_have_envelope(envelope_type::pitch);
            for(const auto& e: m_envelopes)
            {
                auto sz = e.second.size();
                if (sz < 2)
                    SF_THROW(
                        std::invalid_argument{
                            "Envelope has too few elements. Needs 2 as a minimum got " + std::to_string(sz)});
            }
            auto start = m_envelopes[envelope_type::amplitude].front().position();
            auto end = m_envelopes[envelope_type::amplitude].back().position();
            for(const auto& e: m_envelopes)
            {
                if (e.second.front().position() != start)
                    SF_THROW(std::invalid_argument{"Envelope starts not aligned"});
                if (e.second.back().position() != end)
                    SF_THROW(std::invalid_argument{"Envelope ends not aligned"});
            }
        }

        uint64_t note::start() const
        {
            return m_envelopes.find(envelope_type::amplitude)->second.front().position();
        }

        uint64_t note::end() const
        {
            return m_envelopes.find(envelope_type::amplitude)->second.back().position();
        }

        bool note::has_envelope(envelope_type t) const
        {
            return m_envelopes.contains(t);
        }

        void note::must_have_envelope(envelope_type t) const
        {
            if (!has_envelope(t))
                 SF_THROW(std::invalid_argument{"Envelope '" + envelope_type_to_str(t) + "' not present"});
        }

        envelope note::get_envelope(envelope_type t) const
        {
            auto found = m_envelopes.find(t);
            if (found == m_envelopes.end())
                 SF_THROW(std::invalid_argument{"Envelope '" + envelope_type_to_str(t) + "' not present"});
            return found->second;
        }
    }
}
