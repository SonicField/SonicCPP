#include "notes.h"
#import <map>

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

            // Simply read the entire file into tracks of events.
            // First read the midi header.
            auto header = midi::read_header(fstrm);
            if (header.m_format != 1)
                SF_THROW(std::invalid_argument{"For now only format 1 supported. Got: " +
                    std::to_string(header.m_format)});
            for (int i{0}; i<header.m_ntrks; ++i)
            {
                m_events.push_back({});
                auto tc = midi::read_chunk(fstrm);
                if (type_of_chunk(tc) != midi::chunk_type::track)
                    SF_THROW(std::invalid_argument{"Expected tack chunk but did not get that."});

                midi::event_code prev_code{0};
                // Note we convert to absolute offset.
                uint64_t offset{0};
                while(true)
                {
                    auto eventx = midi::parse_event(fstrm, prev_code);
                    offset += eventx->m_offset;
                    eventx->m_offset = offset;
                    std::cerr << "Midi track event: " << *(eventx) << std::endl;
                    prev_code = eventx->m_code;
                    m_events.back().push_back(eventx);
                    if (eventx->m_type == midi::event_type::end_of_track)
                    {
                        break;
                    }
                }
            }
        }

        size_t midi_file_reader::track_count() const
        {
            return m_events.size();
        }

        midi_file_reader::midi_file_reader(std::string file_name): m_file_name{std::move(file_name)}
        {
            SF_MARK_STACK;
            read_events();
        }

        midi_track_events midi_file_reader::track(size_t n) const
        {
            return m_events.at(n);
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
                // Note on events should happen after other events for the effect of the other event to impact the note.
                // I am not sure in this is explicit in the midi standard but it sort of makes implicit sense to me
                // and in the case that there is a tempo track which always comes first in the file - it makes strong sense.
                if (a->m_offset == b->m_offset)
                {
                    // Therefore if b is note_on, a is 'less than' (i.e. comes before) b.
                    // If they are both note_on, who cares?
                    return b->m_type == midi::event_type::note_on;
                };
                return a->m_offset < b->m_offset;
            });
            // Strip duplicate end of track events.
            // The last even has to be end_of_track because if the way the reader works.
            auto last_event = merged.back();
            midi_track_events ret{};
            std::copy_if(merged.begin(), merged.end(), std::back_inserter(ret), [](const auto& e)
            {
                return e->m_type != midi::event_type::end_of_track;
            });
            ret.push_back(last_event);
            return ret;
        }

        // Utitlies to make dynamic casting to event subtypes easier.
        template<typename T>
        const T* to_event(const midi::event_ptr& e)
        {
            return dynamic_cast<const T*>(e.get());
        } 
        // Pointer to materialized function - not ideal but looks elegant.
        auto to_event_tempo = to_event<midi::event_tempo>;

        // This is factored out just to make track_notes::track_notes more human readable.
        inline auto compute_times(const auto& events, auto total_time_ms)
        {
            // Compute the temp scaling factor.
            // Tempo scale current_tempo/initial_tempo which is updated for each tempo event.
            // Then the different between the previous event and the current event is scaled
            // by tempo scale and written into a vector of offsets.
            // Then the whole vector is rescaled so the last element is total_time_ms.
            // This vector is then the timing use for all note and envelope points.
            std::vector<double> scaled_times{};

            // This could all be done in one elegant pass - but what is the point in making the
            // process harder to understand when midi is a trivial part of the overhead of synthesis?
            // Grab the initial tempo.
            double initial_tempo{};
            for(const auto& e: events)
            {
                if (e->m_type == midi::event_type::tempo)
                {
                    // Found the first tempo - is it at zero?
                    if (e->m_offset != 0)
                        SF_THROW(std::invalid_argument{"First tempo event not at zero"});
                    initial_tempo = to_event_tempo(e)->m_us_per_quater;
                    break;
                }
            }
            if (initial_tempo == 0)
                SF_THROW(std::invalid_argument{"Either no tempo was found or it was zero."});

            uint64_t previous_offset{0};
            double current_tempo{1};
            // Compute scaled time vector...
            for(const auto& e: events)
            {
                auto offset = previous_offset + current_tempo * (e->m_offset - previous_offset);
                scaled_times.push_back(offset);
                if (e->m_type == midi::event_type::tempo)
                {
                    current_tempo =  initial_tempo/to_event_tempo(e)->m_us_per_quater;
                }
                previous_offset = offset;
            }

            // We know there must have been at least one event or the initial_tempo would not have been
            // set and so this code will have already thrown.
            auto scale_factor = total_time_ms / scaled_times.back();
            for(auto& t: scaled_times)
            {
                t *= scale_factor;
            }
            // scaled_times is now the event times in milliseconds.
            return scaled_times;
        }

        track_notes::track_notes(midi_track_events events, uint64_t total_time_ms, temperament tempr)
        {
            using env_pack = std::unordered_map<envelope_type, envelope>;
            using channel = uint32_t;
            using note_id = uint32_t;

            // Use ordered maps to avoid worrying about hashing on pairs. The perf difference is not worth
            // worrying about here.

            // Notes active on a channel.
            std::map<std::pair<channel, note_id>, env_pack> current_notes{};

            // Key pressure values if any have been set.
            std::map<std::pair<channel, note_id>, double> polyphonic_key_pressure{};

            // Controller values if they have been set.
            std::map<std::pair<channel, envelope_type>, double> active_contollers{};

            // Get the tempo corrected times in ms.
            auto times = compute_times(events, total_time_ms);

            // Massive ugly switch on event_type is a bit yuck but good enough for this.

        }
    }
}
