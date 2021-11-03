#pragma once
#include "sonic_field.h"
#include "midi_support.h"

namespace sonic_field
{
    namespace notes
    {
        enum struct envelope_type : uint32_t
        {
            modulation,
            breath,
            foot,
            portamento_time,
            amplitude,
            balance,
            pan,
            expression,
            general_purpose_1,
            general_purpose_2,
            general_purpose_3,
            general_purpose_4,
            pitch,
            pressure,
            other_registered,
            effect_reverb,
            effect_tremolo,
            effect_chorus,
            effect_detune,
            effect_phaser,
            sound_variation,
            sound_timbre,
            sound_release,
            sound_attack,
            sound_brightness,
            sound_decay,
            sound_vibrato_rate,
            sound_vibrato_depth,
            sound_vibrato_delay,
            damper,
            sustain,
            portamento,
            soft,
            legato
        };

        inline std::string envelope_type_to_str(envelope_type t)
        {
            switch(t)
            {
                case envelope_type::modulation: return "modulation";
                case envelope_type::breath: return "breath";
                case envelope_type::foot: return "foot";
                case envelope_type::portamento_time: return "portamento_time";
                case envelope_type::amplitude: return "amplitude";
                case envelope_type::balance: return "balance";
                case envelope_type::pan: return "pan";
                case envelope_type::expression: return "expression";
                case envelope_type::general_purpose_1: return "general_purpose_1";
                case envelope_type::general_purpose_2: return "general_purpose_2";
                case envelope_type::general_purpose_3: return "general_purpose_3";
                case envelope_type::general_purpose_4: return "general_purpose_4";
                case envelope_type::pitch: return "pitch";
                case envelope_type::pressure: return "pressure";
                case envelope_type::other_registered: return "other_registered";
                case envelope_type::effect_reverb: return "effect_reverb";
                case envelope_type::effect_tremolo: return "effect_tremolo";
                case envelope_type::effect_chorus: return "effect_chorus";
                case envelope_type::effect_detune: return "effect_detune";
                case envelope_type::effect_phaser: return "effect_phaser";
                case envelope_type::sound_variation: return "sound_variation";
                case envelope_type::sound_timbre: return "sound_timbre";
                case envelope_type::sound_release: return "sound_release";
                case envelope_type::sound_attack: return "sound_attack";
                case envelope_type::sound_brightness: return "sound_brightness";
                case envelope_type::sound_decay: return "sound_decay";
                case envelope_type::sound_vibrato_rate: return "sound_vibrato_rate";
                case envelope_type::sound_vibrato_depth: return "sound_vibrato_depth";
                case envelope_type::sound_vibrato_delay: return "sound_vibrato_delay";
                case envelope_type::damper: return "damper";
                case envelope_type::sustain: return "sustain";
                case envelope_type::portamento: return "portamento";
                case envelope_type::soft: return "soft";
                case envelope_type::legato: return "legato";
                default:
                    return "unknown: " + std::to_string(static_cast<std::underlying_type_t<envelope_type>>(t));
            }
        }

        // A note is a constrained set of envelopes to represent a sound.
        // It must have a pitch and amplitude envelope, all others are optional.
        // All envelopes must start and end at the same position but do not need
        // to have the same number of elements.
        // See envelope_type.
        class note
        {
            uint32_t m_channel;
            std::unordered_map<envelope_type, envelope> m_envelopes;
        public:
            explicit note(std::unordered_map<envelope_type, envelope> envs);

            uint64_t start() const;

            uint64_t end() const;

            bool has_envelope(envelope_type t) const;

            void must_have_envelope(envelope_type t) const;

            envelope get_envelope(envelope_type t) const;
            
            uint32_t channel() const
            {
                return m_channel;
            }
        };

        using voice = std::vector<note>;
        using composition = std::vector<voice>;
        using midi_track_events = std::vector<midi::event_ptr>;
        using midi_tracks_events = std::vector<midi_track_events>;

        // Reads a file into a midi_tracks_events object.
        // Note that the resulting m_offset members of the events are
        // absolute not relative to the previous event in the track as they
        // are in the actual midi file.  This is done so the events are stand-alone
        // and can be merged together into different track_events containers whilst
        // retaining their timing information.
        //
        // Support:
        // Format 0 -> yes
        // Format 1 -> yes
        // Format 2 -> not sure!!!
        //
        // For format 1 it is necessary to always have track 0 to merge with any
        // other track to be able to generate notes from tempo.
        class midi_file_reader
        {
            const std::string m_file_name;
            midi_tracks_events m_events;
            void read_events();
            std::ifstream open_file() const;

        public:
            explicit midi_file_reader(std::string file_name);
            std::vector<size_t> tracks_with_notes() const;
            midi_track_events track(size_t) const;
            size_t track_count() const;
            void dump_track() const;

        };

        constexpr double modern_base  = 8.1757989156;
        constexpr double baroque_base = modern_base * 415.0/440.0;

        class temperament
        {
        protected:
            const double m_base;
            const std::array<double, 12> m_cents;

            const bool m_offset;
            explicit temperament(double base, decltype(m_cents)&& cents, bool offset):
                m_base{base},              // Tuning of A4
                m_cents{std::move(cents)}, // Either the absolute cents offset from the equal temperament or absolute cents from C
                m_offset{offset}           // Are the cents offset or absolute?
            {}
        public:
            // Get the frequency of a key given a list of cents offsets within each octave
            // note this only works for midi keys so does not support true enharmonisity
            double pitch(size_t midi_note) const;
        };

        // This is by far my favourite temperament for Bach!
        class bach_lehman_temperament: public temperament
        {
        public:
            explicit bach_lehman_temperament():
                temperament
                {
                    baroque_base,
                    // C   C#     D       Eb      E       F       F#      G       G#      A       A#      B
                    {5.9  ,3.9   ,2      ,3.9   ,-2      ,7.8    ,2      ,3.9    ,3.9    ,0      ,3.9    ,0},
                    true
                }
            {}
        };

        // What nearly all 'modern' music is written for.
        class equal_temperament: public temperament
        {
        public:
            explicit equal_temperament():
                temperament
                {
                    modern_base,
                    // C   C#     D       Eb      E       F       F#      G       G#      A       A#      B
                    {  0  ,0     ,0      ,0     , 0      ,0      ,0      ,0      ,0      ,0      ,0      ,0},
                    true
                }
            {}
        };

        // Great 'go to' temperament which works well for baroque music.
        class werckmeisterIII_temperament: public temperament
        {
        public:
            explicit werckmeisterIII_temperament():
                temperament
                {
                    baroque_base,
                    // C   C#     D       Eb      E       F       F#      G       G#      A       A#      B
                    {0,  90.225,192.18, 294.135,390.225,498.045,588.27, 696.09, 792.18, 888.27, 996.09, 1092.18},
                    false
                }
            {}
        };

        // TODO: Compute the cents for just intonation - see python 2.7 frequency computation:
        //     '''
        //     One transposition for just intonation onto the non enharmonic chromatic scale.
        //     This is only likely to work in scales close to C. It will go horribly out of
        //     tune unless great care is taken.
        //     '''
        //     key=float(key)
        //     ratios = (
        //         (1,1),    #C
        //         (16,15),  #C+
        //         (9,8),    #D
        //         (6,5),    #D+
        //         (5,4),    #E
        //         (4,3),    #F
        //         (10,7),   #F+
        //         (3,2),    #G
        //         (32,21),  #G+
        //         (5,3),    #A
        //         (9,5),    #A+
        //         (15,8)    #B
        //    )
        //    octave=math.floor(key/12.0)
        //    pitch=base*2.0**octave
        //    note=int(key-octave*12)
        //    ratio=ratios[note]
        //    ratio=float(ratio[0])/float(ratio[1])
        //    pitch*=ratio
        //    return pitch

        // Merge multiple tracks of events into a single one.
        midi_track_events merge_midi_tracks(std::vector<midi_track_events> trackes);

        // A vector of notes from a set of track events.
        class track_notes: public std::vector<note>
        {
            track_notes() = default;

        public:
            // Takes events including tempo events and a total_time which is how long the track should be.
            // total time is used to work around the complex midi time system which should be implemented
            // but just is not worth it.
            // Use this constructor if tempo events are in the track.
            explicit track_notes(
                midi_track_events events,
                uint64_t total_time_ms,
                temperament tempr);

            // Takes events and a tempo track and a total_time which is how long the track should be.
            // total time is used to work around the complex midi time system which should be implemented
            // but just is not worth it.
            // Use this constructor if there is a tempo track.
            explicit track_notes(
                midi_track_events events,
                midi_track_events tempo,
                uint64_t total_time_ms,
                temperament tempr):
                track_notes{merge_midi_tracks({events, tempo}), total_time_ms, tempr}
            {}

            // Get only the notes for a given channel.
            track_notes for_channel(auto channel)
            {
                track_notes ret{};
                std::copy_if(begin(), end(), std::back_inserter(ret), [channel](const auto& n)
                    {
                        return n.channel() == channel;
                    });
                return ret;
            }
        };

    }
}
