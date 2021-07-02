#include "filter_demo.h"

namespace sonic_field
{
    void simple_sweep()
    {
        SF_SCOPE("simple sweep");
        generate_sweep(2000, 10000, 30000)
            >> write("sweep");
        signal_to_wav("sweep");
    }

    void low_pass_sweep()
    {
        SF_SCOPE("low pass sweep");
        std::vector<double> qs{ {0.01, 0.1, 0.5, 1.0, 5, 10, 100} };
        for (auto& q : qs)
        {
            auto fname = "low_pass_sweep_" + std::to_string(q);
            generate_sweep(2000, 10000, 30000)
                >> filter_rbj(filter_type::LOWPASS, 3000, q, 0)
                >> write(fname);
            signal_to_wav(fname);
        }
    }

    void peak_sweep()
    {
        SF_SCOPE("peak sweep");
        std::vector<double> qs{ {0.01, 0.1, 0.5, 1.0, 5, 10, 100} };
        for (auto& q : qs)
        {
            auto fname = "peak_sweep_" + std::to_string(q);
            generate_sweep(2000, 10000, 30000)
                >> filter_rbj(filter_type::PEAK, 3000, q, 10)
                >> write(fname);
            signal_to_wav(fname);
        }
    }

    void peak_white()
    {
        SF_SCOPE("peak white");
        std::vector<double> qs{ {0.01, 0.1, 0.5, 1.0, 5, 10, 100} };
        for (auto& q : qs)
        {
            auto fname = "peak_white_" + std::to_string(q);
            generate_noise(2000)
                >> filter_rbj(filter_type::PEAK, 3000, q, 10)
                >> write(fname);
            signal_to_wav(fname);
        }
    }

    void notch_sweep()
    {
        SF_SCOPE("notch sweep");
        std::vector<double> qs{ {0.01, 0.1, 0.5, 1.0, 5, 10, 100} };
        for (auto& q : qs)
        {
            auto fname = "notch_sweep_" + std::to_string(q);
            generate_sweep(2000, 10000, 30000)
                >> filter_rbj(filter_type::PEAK, 3000, q, -10)
                >> write(fname);
            signal_to_wav(fname);
        }
    }

    void resonator_sweep()
    {
        SF_SCOPE("resonator sweep");
        auto fname = "resonator_sweep";
        generate_sweep(2000, 10000, 30000)
            >> repeat(4, { filter_rbj(filter_type::PEAK, 3000, 0.1, 20) })
            >> write(fname);
        signal_to_wav(fname);
    }

    void resonator_white()
    {
        SF_SCOPE("resonator white");
        auto fname = "resonator_white";
        generate_noise(2000)
            >> repeat(4, { filter_rbj(filter_type::PEAK, 3000, 0.1, 20) })
            >> write(fname);
        signal_to_wav(fname);
    }

    void comb_demo()
    {
        SF_SCOPE("combe");
        auto fname = "combe";
        auto input = generate_sweep(64, 512, 2000)
            >> store();
        auto mx = mix(mixer_type::OVERLAY);
        copy(input)
            >> mx;
        input
            >> cut(10, 0, 2000-10, 0)
            >> mx;
        mx
            >> write(fname);
        signal_to_wav(fname);
    }

    void resonator_white_low()
    {
        SF_SCOPE("resonator white_low");
        auto fname = "resonator_white_low";
        generate_noise(10000)
            >> repeat(4, { filter_rbj(filter_type::PEAK, 128, 0.1, 20) })
            >> distort_power(0.9)
            >> write(fname);
        signal_to_wav(fname);
    }

    void ladder_q()
    {
        SF_SCOPE("ladder q");
        uint64_t length{10000};
        double pitch{256};
        auto ladder1 = filter_shaped_rbj(filter_type::PEAK); 
        generate_noise(length)
            >> seed(pitch, 0.2, 0)
            >> ladder1;
        generate_linear({ {0, pitch}, {length, pitch} })
            >> ladder1;
        generate_linear({ {0, 0.1}, {length, 0.1} })
            >> ladder1;
        generate_linear({ {0, 20}, {length, 20.0} })
            >> ladder1;

        auto ladder2 = filter_shaped_rbj(filter_type::PEAK); 
        ladder1
            >> damp_gain(0.01, 0.01, 0.01)
            >> ladder2;
        generate_linear({ {0, pitch}, {length, pitch} })
            >> ladder2;
        generate_linear({ {0, 0.1}, {length, 0.1} })
            >> ladder2;
        generate_linear({ {0, 20}, {length, 20.0} })
            >> ladder2;

        auto ladder3 = filter_shaped_rbj(filter_type::PEAK); 
        ladder2
            >> damp_gain(0.01, 0.01, 0.01)
            >> ladder3;
        generate_linear({ {0, pitch}, {length, pitch} })
            >> ladder3;
        generate_linear({ {0, 0.1}, {length, 0.1} })
            >> ladder3;
        generate_linear({ {0, 20}, {length, 20.0} })
            >> ladder3;

        auto adsr_mix = mix(mixer_type::MULTIPLY);

        generate_linear({ {0, 0}, {length/20, 1.0}, {length/10, 0.75}, {length/4, 0.75}, {length, 0} })
            >> adsr_mix;
        ladder2
            >> adsr_mix;

        adsr_mix
            >> write("ladder_q");

        signal_to_wav("ladder_q");
    }

    void filter_demo()
    {
        //simple_sweep();
        //low_pass_sweep();
        //peak_sweep();
        //peak_white();
        //resonator_sweep();
        //resonator_white();
        //resonator_white_low();
        //notch_sweep();
        //comb_demo(); 
        ladder_q();
    }
}
