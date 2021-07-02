#include "cactus.h"

namespace sonic_field
{
    void cactus_creatures(double pitch, double pan_left, std::string name)
    {
        SF_SCOPE("cactus_creatures");

        auto env_mixa = mix( mixer_type::MULTIPLY );
        generate_noise(10000)
            >> distort_power(10000)
            >> env_mixa; 
        generate_linear({ {0, 0}, {100, 1}, {10000-100, 1}, {10000, 0} })
            >> env_mixa;

        std::vector<std::pair<double, double>> pitches{};
        double vol = 1.0;
        while(pitch < 20000)
        {
            pitches.push_back({pitch, vol});
            pitch *= 2;
            vol /= 2;
        }
        //auto low_crackle = cracle >>
        //    filter_rbj(filter_type::LOWPASS, 1000, 1, 1);
        auto source = filter_bank(env_mixa, 0.5, 20, 4, pitches);

        auto env_mixb = mix( mixer_type::MULTIPLY );
        source
            >> filter_rbj(filter_type::LOWPASS, 2000, 0.2, 1)
            >> cut(0, 0, 10000, 0)
            >> env_mixb;

        generate_linear({ {0, 0}, {100, 1}, {10000-100, 1}, {10000, 0} })
            >> env_mixb;

        auto dry = env_mixb 
            >> cut(0, 0, 10000, 10000)
            >> store();

        auto shimmer = mreverberate(
                "revl",
                "revr",
                5000.0,    // Damping frequ
                0.8,       // Density
                10000.0,   // Bandwidth frequ
                0.9,       // Decay 0-1
                50.0,      // Preday ms
                2.5,       // Size
                1.0,       // Gain
                1.0,       // Mix
                1.0        // Early mix
            );

        copy(dry)
            >> filter_rbj(filter_type::HIGHPASS, 2000, 1.0, 1)
            >> filter_rbj(filter_type::HIGHPASS, 2000, 1.0, 1)
            >> distort_power(1.2)
            >> control_gain(0.1, 0.05)
            >> amplify(pan_left)
            >> shimmer;

        copy(dry)
            >> filter_rbj(filter_type::HIGHPASS, 2000, 1.0, 1)
            >> filter_rbj(filter_type::HIGHPASS, 2000, 1.0, 1)
            >> distort_power(1.2)
            >> control_gain(0.1, 0.05)
            >> amplify(1.0 -pan_left)
            >> shimmer;

        auto reverb = mreverberate(
                "revl_low",
                "revr_low",
                2500.0,    // Damping frequ
                0.8,       // Density
                5000.0,    // Bandwidth frequ
                0.5,       // Decay 0-1
                120.0,     // Preday ms
                5.0,       // Size
                1.0,       // Gain
                0.5,       // Mix
                0.2        // Early mix
            );

        copy(dry)
            >> amplify(pan_left)
            >> reverb;

        dry
            >> amplify(1.0 -pan_left)
            >> reverb;

        auto out_mixerl = mix( mixer_type::ADD );
        auto out_mixerr = mix( mixer_type::ADD );

        read("revl")
            >> amplify(0.1)
            >> out_mixerl;

        read("revl_low")
            >> amplify(0.9)
            >> out_mixerl;

        read("revr")
            >> amplify(0.1)
            >> out_mixerr;

        read("revr_low")
            >> amplify(0.9)
            >> out_mixerr;
        
        out_mixerl
            >> write("cactus_l" + name);

        out_mixerr
            >> write("cactus_r" + name);
    }

    void cactus_loop()
    {
        SF_SCOPE("cactus_creatures_loop");
        random_doubles delay_to_next{};
        random_doubles pan_left{};
        random_doubles pitch{};
        uint64_t start = 0;
        auto mxl = mix(mixer_type::OVERLAY);
        generate_silence(400000)
            >> mxl;
        auto mxr = mix(mixer_type::OVERLAY);
        generate_silence(400000)
            >> mxr;
        while(start < 300000)
        {
            double p = uint64_t(std::abs((pitch() * 16)) + 0.51) * 8 + 64;
            double n = std::abs(pan_left());
            if (n < 0.5)
                n *= 2;
            std::string name = std::to_string(start);
            cactus_creatures(p, n, name);
            read("cactus_l" + name)
                >> cut(start, 0, 20000, 0)
                >> mxl;
            read("cactus_r" + name)
                >> cut(start, 0, 20000, 0)
                >> mxr;
            start += std::abs(delay_to_next()) * 10000 + 10000;
        }

        mxl
            >> write("creatures_l");
        mxr
            >> write("creatures_r");

        signal_to_wav("creatures_l");
        signal_to_wav("creatures_r");

    }

    void cactus_decay()
    {
        SF_SCOPE("cactus_decay");

        // Input 30 seconds, 30 seconds echo;
        //uint64_t length{3000000};
        uint64_t length{5*60000 + 2763};

        // Read wav and save to a high performance signal file.
        read_wav("decay_in")
            >> cut(0, 0, length, 0) 
            >> write("decay_in");

        // Read the signal and add a comb filter with off_set millis.
        auto read_decay = [&](uint64_t off_set) {
            auto mx = mix(mixer_type::OVERLAY);
            read("decay_in")
                >> cut(0, 0, length, 30000)
                >> mx; 
            read("decay_in")
                >> cut(off_set, 0, length, 30000 - off_set)
                >> mx; 
            return mx
                >> distort_power(0.95)
                >> amplify(0.5);
        };


        // Process the combed signal and pass through two different echos which will be
        // subtly differnt for left and right giving stereo separation.
        // Also use slightly different comb filter settings left and right so the peaks on sound
        // feel like they are moving around as the shepard tones decend through the comb peaks.
        // Use a bit of filtering to cut out HF noise and boost the base.
        read_decay(9)
            >> echo(
                    475,   // delay
                    0.50,  // feedback
                    0.75,  // mix
                    0.0,   // saturate
                    0.05,  // wow
                    0.00)  // flutter
            >> echo(
                    250,   // delay
                    0.75,  // feedback
                    0.5,   // mix
                    0.1,   // saturate
                    0.05,  // wow
                    0.02)  // flutter
            >>
                filter_rbj(filter_type::LOWPASS, 1024, 0.5, 1)
            >>
                filter_rbj(filter_type::PEAK, 32, 1, 1)
            >> write("decay_out_l");

        read_decay(11)
            >> echo(
                    485,   // delay
                    0.50,  // feedback
                    0.75,  // mix
                    0.0,   // saturate
                    0.05,  // wow
                    0.02)  // flutter
            >> echo(
                    252,   // delay
                    0.5,   // feedback
                    0.5,   // mix
                    0.1,   // saturate
                    0.06,  // wow
                    0.02)  // flutter
            >>
                filter_rbj(filter_type::LOWPASS, 1024, 0.5, 1)
            >>
                filter_rbj(filter_type::PEAK, 32, 1, 1)
            >> write("decay_out_r");

        // Dump out the result.
        signal_to_wav("decay_out_l");
        signal_to_wav("decay_out_r");
    }

    void cactus()
    {
        //cactus_decay();
        cactus_loop();
    }
}
