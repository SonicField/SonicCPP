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

        std::ifstream midi_file_reader::open_file() const
        {
            std::ifstream fstrm{m_file_name, std::ios_base::in | std::ios_base::binary };
            if (!fstrm)
                 SF_THROW(std::invalid_argument{"Fille not found: " + m_file_name});
            return fstrm;
        }

        void midi_file_reader::read_events()
        {
            SF_MARK_STACK;
            auto fstrm = open_file();
            struct file_close
            {
                decltype(fstrm)& m_f;
                file_close(decltype(fstrm)& f): m_f{f}{}
                ~file_close(){ m_f.close(); }
            };
            file_close closer{fstrm};

        }

        midi_file_reader::midi_file_reader(std::string file_name): m_file_name{std::move(file_name)}
        {
            SF_MARK_STACK;
            read_events();
        }

        double temperament::pitch(size_t midi_note) const
        {
            const auto cent = std::pow(2.0, (1.0/1200.0));
            const auto octave = std::floor(static_cast<double>(midi_note)/12.0);
            const auto note = static_cast<size_t>(std::round(static_cast<double>(midi_note)-octave*12.0));

            auto pitch = m_base*std::pow(2.0, octave);
            if (m_offset)
                pitch *= std::pow(cent, ((m_cents[note]+note*100.0)-m_cents[0]));
            else
                pitch *= std::pow(cent, m_cents[note]);
            return pitch;
        }

        // So there is a righ way to do this (iterate over both and merge in one step taking advantage of the fact
        // the are both sorted initially) and the easy 'merge and sort'.  I went for the easy.
        midi_track_events merge_midi_tracks(std::vector<midi_track_events> trackes)
        {
            midi_track_events merged{};
            for(const auto& t: trackes)
            {
                merged.insert(merged.end(), t.begin(), t.end());
            }
            std::sort(merged.begin(), merged.end(), [](const auto& a, const auto& b)
            {
                // Note on events should happen after other events for the effect of the other event to occur on the note
                // event. I am not sure in this is explicit in the midi standard but it sort of makes implicit sense to me
                // and in the case that there is a tempo track which always comes first in the file - it makes strong sense.
                if (a->m_offset == b->m_offset)
                {
                    return a->m_type == midi::event_type::note_on;
                };
                return a->m_offset < b->m_offset;
            });
            return merged;
        }


    }
}
