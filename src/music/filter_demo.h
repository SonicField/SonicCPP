#pragma once
#include "../sonic_field.h" 

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

    void reasonator_sweep()
    {
        SF_SCOPE("reasonator sweep");
        auto fname = "reasonator_sweep";
        generate_sweep(2000, 10000, 30000)
            >> repeat(4, { filter_rbj(filter_type::PEAK, 3000, 0.1, 20) })
            >> write(fname);
        signal_to_wav(fname);
    }

    void filter_demo()
    {
        //simple_sweep();
        //low_pass_sweep();
        //peak_sweep();
        //reasonator_sweep();
        notch_sweep();
    }
}