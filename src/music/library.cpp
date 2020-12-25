#include "library.h" 

namespace sonic_field
{
    signal generate_rich_base(uint64_t length, double pitch)
    {
        return generate_noise(length)
            >> seed(pitch * 2.0, 0.02, 0.25)
            >> repeat(3, { filter_rbj(filter_type::PEAK, pitch, 0.1, 20) })
            >> control_gain(0.1, 0.005)
            >> filter_rbj(filter_type::LOWPASS, pitch, 2, 0.0)
            >> distort_power(1.25)
            >> distort_saturate(0.5)
            >> control_gain(0.1, 0.005);

    }

    signal generate_windy_base(uint64_t length, double pitch)
    {
        return generate_noise(length)
            >> seed(pitch, 0.01, 0.75)
            >> repeat(2, { filter_rbj(filter_type::PEAK, pitch / 2.0, 0.2, 20) })
            >> control_gain(0.1, 0.005)
            >> filter_rbj(filter_type::LOWPASS, pitch / 2.0, 2, 0.0)
            >> distort_power(1.25)
            >> control_gain(0.1, 0.005);
    }
}