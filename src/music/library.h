#pragma once
#include "../sonic_field.h"

namespace sonic_field
{
    signal filter_bank(
        signal input,
        double width, 
        double resonance,
        uint64_t repeats,
        std::vector<std::pair<double, double>> pitches
    );
    signal filter_bank(
        const std::string& name,
        double width, 
        double resonance,
        uint64_t repeats,
        std::vector<std::pair<double, double>> pitches
    );
    signal generate_rich_base(uint64_t length, double pitch);
    signal generate_windy_base(uint64_t length, double pitch);
    signal generate_pure_tone(uint64_t length, double pitch, uint64_t cycles);
    signal generate_white_to_tone(uint64_t length, double pitch, double purity);
    std::pair<signal, signal> pan_lr(signal input, double pan_start, double pan_end, uint64_t length);
    std::pair<signal, signal> haas(signal left, signal right, uint64_t delay, double amount, uint64_t length);
    double find_17scale(double target);
    class constrained_random_walk
    {
        double m_min_value;
        double m_max_value;
        double m_step_size;
        double m_state;
        random_doubles m_gen;
    public:
        constrained_random_walk(double min_value, double max_value, double step_size, double start);
        double operator()();
    };
}
