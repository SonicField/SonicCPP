#include "fractured_mind.h"

namespace sonic_field
{
    // Note that the return length is length + 20000
    // instance, strength, pitch, length, pan_start, pan_end
    std::pair<std::string, std::string> fractured_mind_swoosh(
            uint64_t instance,
            double strength,
            double pitch,
            uint64_t length,
            double pan_start,
            double pan_end)
    {
        SF_SCOPE("fractured_mind_swoosh");
        uint64_t echo_length{length+10000};
        uint64_t total_length{echo_length+10000};
        if (strength < 0.1 || strength > 1.0)
            SF_THROW(std::invalid_argument{"Strength out of range: " + std::to_string(strength)});
        auto make_sound = [=](){
            SF_MARK_STACK;
            auto env_mixer = mix( mixer_type::MULTIPLY );
            generate_linear({{ 0, 0.0 }, { length>>1, 1.0 }, { length, 0.0 }})
                >> env_mixer;
            generate_white_to_tone(length, pitch, strength)
                >> env_mixer;
            return env_mixer
                >> cut(0,0,length, echo_length)
                >> echo(250, 0.85, 0.5, 0.1, 0.02, 0.05);
        };
        auto reverb = mreverberate(
                "revl",
                "revr",
                5000.0,    // Damping frequ
                0.5,       // Density
                10000.0,   // Bandwidth frequ
                0.5,       // Decay 0-1
                500.0,     // Preday ms
                9.0,       // Size
                1.0,       // Gain
                0.5,       // Mix
                1.0        // Early mix
            );
        auto lr_signals = pan_lr(make_sound(), pan_start, pan_end, total_length);
        lr_signals.first >> reverb;
        lr_signals.second >> reverb;


        auto rl = read("revl");
        auto rr = read("revr");
        auto haased = haas(rl, rr, 50, 0.05, total_length);
        auto name_l = "swoosh_l_" + std::to_string(instance);
        auto name_r = "swoosh_r_" + std::to_string(instance);
        haased.first
            >> write(name_l);
        haased.second
            >> write(name_r);
        return {name_l, name_r};
    }

    // instance, delay, length, resonance_pitch, bass, form, pan_start, pan_end
    // instance: number for file name
    // length: length before reverb
    // resonce_patch: ladder filter pitch e.g. 500.
    // bass: bass pitch (e.g. 4, 8 etc)
    // form: formant pitch (e.g. 2000)
    // pan_start: lr pan start position
    // pan_send: lr pan end position
    std::pair<std::string, std::string> fractured_mind_animal(
            uint64_t instance,
            uint64_t length,
            double resonance_pitch,
            double bass,
            double form,
            double pan_start,
            double pan_end)
    {
        SF_SCOPE("fractured_mind_animal");
        std::cout << "ANIMAL:\n"
                  << "    instance=" << instance << "\n"
                  << "    length=" << length << "\n"
                  << "    resonance_pitch=" << resonance_pitch << "\n"
                  << "    bass=" << bass << "\n"
                  << "    form=" << form << "\n"
                  << "    pan_start=" << pan_start << "\n"
                  << "    pan_end=" << pan_end << std::endl;
        auto make_animal = [&]
        {
            auto voice = generate_rich_base(length, bass)
                >> filter_rbj(filter_type::PEAK, form, 4.0, 50) 
                >> filter_rbj(filter_type::PEAK, form*0.3 , 4.0, 50);
            auto ladder = ladder_filter();
            voice 
                >> ladder;
            generate_linear({
                    {0, 0.75},
                    {length * 0.25, 0.5}, {length * 0.75, 0.9}, {length, 0.8} })
                >> ladder;
            generate_linear({
                    {0, resonance_pitch * 0.1},
                    {length * 0.25, resonance_pitch * 1.0},
                    {length * 0.5, resonance_pitch * 1.1},
                    {length, resonance_pitch * 0.25} })
                >> ladder;
            auto taper_mixer = mix(mixer_type::MULTIPLY);
            ladder
                >> echo(125, 0.5, 0.5, 0.0, 0.01, 0.15)
                >> taper_mixer;
            generate_linear({ {0, 0}, {length*0.2, 1.0}, {length*0.4, 1.}, {length, 0.0} })
                >> taper_mixer;
            return taper_mixer;
        };
        auto panned = pan_lr(make_animal(), pan_start, pan_end, length);
        auto rev_name_l = temp_file_name(); 
        auto rev_name_r = temp_file_name(); 
        auto reverb = mreverberate(
                rev_name_l,
                rev_name_r,
                2000.0,    // Damping frequ
                0.2,       // Density
                5000.0,    // Bandwidth frequ
                0.5,       // Decay 0-1
                50.0 ,     // Preday ms
                5.0,       // Size
                1.0,       // Gain
                0.3,       // Mix
                0.5        // Early mix
            );
        uint64_t tail = 5000;
        panned.first
            >> cut(0, 0, length, tail)
            >> reverb;
        panned.second
            >> cut(0, 0, length, tail)
            >> reverb;
        length += tail;

        auto name_l = "animal_l_" + std::to_string(instance);
        auto name_r = "animal_r_" + std::to_string(instance);
        auto rl = read(rev_name_l);
        auto rr = read(rev_name_r);
        auto haased = haas(rl, rr, 30, 0.1, length);
        haased.first
            >> write(name_l);
        haased.second
            >> write(name_r);
        return {name_l, name_r};
    }

    class fractured_walker
    {
        // start at 12 for horror - 1 for pure swoosh
        constrained_random_walk m_numerator{1, 24, 3, 1};
        constrained_random_walk m_denomiator{1, 5, 1, 1};
        double m_old_val{0};

    public:

        double operator()()
        {
           // Ensure the pitch always moves.
           double num = int64_t(m_numerator());
           double dnm = int64_t(m_denomiator());
           double val = num/dnm;
           while (m_old_val == val)
           {
               num = int64_t(m_numerator());
               dnm = int64_t(m_denomiator());
               val = num/dnm;
           }
           m_old_val = val;
           return val;
        }
    };

    void fractured_mind_animal_loop()
    {
        //find_17scale(apitch)
        // instance, delay, length, resonance_pitch, bass, form, pan_start, pan_end
        SF_SCOPE("fractured_mind_animal_loop");
        random_doubles   delay_to_next{};
        random_doubles   length{};
        random_doubles   resonance_pitch;
        fractured_walker bass;
        random_doubles   form;
        random_doubles   pan_mode{};
        random_doubles   pan_value{};
        random_doubles   amp{};

        auto mxl = mix(mixer_type::OVERLAY);
        uint64_t count{200};
        generate_silence(16000 + 10000 * count)
            >> mxl;
        auto mxr = mix(mixer_type::OVERLAY);
        generate_silence(16000 + 10000 * count)
            >> mxr;
        {
            SF_MARK_STACK;
            uint64_t position{2000};
            for (uint64_t note{0}; note < count; ++note)
            {
                double pan_start;
                double pan_end;
                double pan_mode_value{abs(pan_mode()) * 4};
                if (pan_mode_value < 1.0)
                {
                    pan_start = 1.0;
                    pan_end = 0.0;
                }
                else if (pan_mode_value < 2.0)
                {
                    pan_start = 0.0;
                    pan_end = 1.0;
                }
                else if (pan_mode_value < 3.0)
                {
                    pan_start = abs(pan_value());
                    pan_end = abs(pan_value());
                }
                else
                {
                    pan_start = pan_end = abs(pan_value());
                }
                // instance, delay, length, resonance_pitch, bass, form, pan_start, pan_end
                auto this_length = length()*5000 + 8000;
                auto names = fractured_mind_animal(
                        note,
                        this_length,
                        (abs(resonance_pitch())  + 1) * 300,
                        find_17scale(fmax(1.0, bass()*0.5)),
                        (abs(form()) + 6) * 200 + 1200,
                        pan_start,
                        pan_end
                );
                auto an_amp = (abs(amp()) * 0.95) + 0.05;
                read(names.first)
                    >> amplify(an_amp)
                    >> cut(position, 0, this_length + 5000 , 0)
                    >> mxl;
                read(names.second)
                    >> amplify(an_amp)
                    >> cut(position, 0, this_length + 5000 , 0)
                    >> mxr;
                position += 6000 + uint64_t(2000 * delay_to_next());
            }
        }

        {
            SF_MARK_STACK;
            mxl
                >> write("animal_l_final");
            mxr
                >> write("animal_r_final");
        }
    }

    void fractured_mind_swoosh_loop(double base, uint64_t gap)
    {
        //find_17scale(apitch)
        SF_SCOPE("fractured_mind_swoosh_loop");
        random_doubles   delay_to_next{};
        random_doubles   length{};
        random_doubles   strength{};
        fractured_walker pitch{};
        random_doubles   pan_mode{};
        random_doubles   pan_value{};
        random_doubles   amp{};

        auto mxl = mix(mixer_type::OVERLAY);
        uint64_t count{50};
        generate_silence(16000 + (gap * 2) * count)
            >> mxl;
        auto mxr = mix(mixer_type::OVERLAY);
        generate_silence(16000 + (gap * 2) * count)
            >> mxr;
        {
            SF_MARK_STACK;
            uint64_t position{2000};
            for (uint64_t note{0}; note < count; ++note)
            {
                double pan_start;
                double pan_end;
                double pan_mode_value{abs(pan_mode()) * 4};
                if (pan_mode_value < 1.0)
                {
                    pan_start = 1.0;
                    pan_end = 0.0;
                }
                else if (pan_mode_value < 2.0)
                {
                    pan_start = 0.0;
                    pan_end = 1.0;
                }
                else if (pan_mode_value < 3.0)
                {
                    pan_start = abs(pan_value());
                    pan_end = abs(pan_value());
                }
                else
                {
                    pan_start = pan_end = abs(pan_value());
                }
                auto this_length = length()*8000 + 8000;
                // instance, strength, pitch, length, pan_start, pan_end
                auto names = fractured_mind_swoosh(
                        note,
                        (abs(strength())*0.5)+0.5,
                        find_17scale(pitch()*base),
                        this_length,
                        pan_start,
                        pan_end
                );
                auto an_amp = (abs(amp()) * 0.95) + 0.05;
                read(names.first)
                    >> amplify(an_amp)
                    >> cut(position, 0, this_length + 20000 , 0)
                    >> mxl;
                read(names.second)
                    >> amplify(an_amp)
                    >> cut(position, 0, this_length + 20000 , 0)
                    >> mxr;
                position += gap + uint64_t(gap * delay_to_next());
            }
        }

        {
            SF_MARK_STACK;
            mxl
                >> write("swoosh_l_final_" + std::to_string(uint64_t(base)));
            mxr
                >> write("swoosh_r_final_" + std::to_string(uint64_t(base)));
        }
    }

    void fractured_mind()
    {
        SF_SCOPE("fractured_mind");
        fractured_mind_swoosh_loop(128, 24000);
        signal_to_wav("swoosh_l_final_128");
        signal_to_wav("swoosh_r_final_128");
        fractured_mind_swoosh_loop(192, 24000);
        signal_to_wav("swoosh_l_final_192");
        signal_to_wav("swoosh_r_final_192");
        fractured_mind_swoosh_loop(256, 24000);
        signal_to_wav("swoosh_l_final_256");
        signal_to_wav("swoosh_r_final_256");
    }

    void fractured_mind_test()
    {
        SF_SCOPE("fractured_mind_main");
        auto sig_store = generate_linear({{ 0, 0.0 }, { 1000, 0.0 }}) >> store();
        sig_store >> run();
    }
} // sonic_field
