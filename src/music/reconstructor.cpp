#include "reconstructor.h"

namespace sonic_field
{
    void regenerate_highs()
    {
        SF_SCOPE("regenerate_highs");

        double middle_c = 261.63;
        double step{pow(2, 1.0/12.0)};
        std::vector<std::pair<double, double>> pitches{};
        for(double p{middle_c / 4.0}; p < 15000.0; p*=step)
        {
            pitches.push_back({p, 1.0});
        }

        read_wav("moonlight_in")
            >> cut(0, 0, 60000, 0)
            >> write("moonlight");

        auto env_mixer = mix(mixer_type::ADD);
        read("moonlight")
            >> filter_rbj(filter_type::HIGHPASS, 500 , 1.0, 1.0)
            >> write("moonlight_high");
        filter_bank("moonlight_high", 0.001, 4, 8, pitches)
            >> write("moonlight_filter");
        read("moonlight_filter")
            >> amplify(10)
            >> distort_power(0.85)
            >> amplify(0.2)
            >> env_mixer;
        read("moonlight")
            >> env_mixer;
        auto mono = env_mixer
            >> write("mono");

        auto stereo = haas(
                read("mono_filter")
                    >> filter_rbj(filter_type::HIGHPASS, 192, 1.0, 1.0),
                read("mono_filter")
                    >> filter_rbj(filter_type::LOWPASS,  192, 1.0, 1.0)
                    >> filter_rbj(filter_type::PEAK,      64, 0.5, 1.0),
                25, 0.2, 292000); 
        stereo.first
            >> write("moonlight_out_l");
        stereo.second
            >> write("moonlight_out_r");

        signal_to_wav("moonlight_out_l");
        signal_to_wav("moonlight_out_r");
    }
}
