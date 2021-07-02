#include "library.h" 

namespace sonic_field
{
    signal filter_bank(
        signal input, 
        double width, 
        double resonance,
        uint64_t repeats,
        std::vector<std::pair<double, double>> pitches)
    {
        SF_MARK_STACK;
        if (pitches.empty())
            SF_THROW(std::invalid_argument{"Pitch vector empty in filter_bank"});
        auto sig_store = input
            >> store();
        auto output_mixer = mix(mixer_type::ADD);
        for(auto pitch: pitches)
        {
            copy(sig_store)
                >> repeat(repeats, { filter_rbj(filter_type::PEAK, pitch.first, width, resonance) })
                >> amplify(pitch.second)
                >> output_mixer;
        }
        // Drain the store;
        sig_store
            >> run();
        return output_mixer;
    }

    signal filter_bank(
        const std::string& input, 
        double width, 
        double resonance,
        uint64_t repeats,
        std::vector<std::pair<double, double>> pitches)
    {
        SF_MARK_STACK;
        if (pitches.empty())
            SF_THROW(std::invalid_argument{"Pitch vector empty in filter_bank"});
        auto output_mixer = mix(mixer_type::ADD);
        for(auto pitch: pitches)
        {
            read(input)
                >> repeat(repeats, { filter_rbj(filter_type::PEAK, pitch.first, width, resonance) })
                >> amplify(pitch.second)
                >> output_mixer;
        }
        return output_mixer;
    }

    signal generate_rich_base(uint64_t length, double pitch)
    {
        SF_MARK_STACK;
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
        SF_MARK_STACK;
        return generate_noise(length)
            >> seed(pitch, 0.01, 0.75)
            >> repeat(2, { filter_rbj(filter_type::PEAK, pitch / 2.0, 0.2, 20) })
            >> control_gain(0.1, 0.005)
            >> filter_rbj(filter_type::LOWPASS, pitch / 2.0, 2, 0.0)
            >> distort_power(1.25)
            >> control_gain(0.1, 0.005);
    }

    signal generate_pure_tone(uint64_t length, double pitch, uint64_t cycles)
    {
        SF_MARK_STACK;
        return generate_noise(length+1000)
            >> seed(pitch, 0.1, 0.0)
            >> repeat(cycles, { 
               filter_rbj(filter_type::PEAK, pitch, 0.2, 20),
               filter_rbj(filter_type::LOWPASS, pitch, 1, 2),
               filter_rbj(filter_type::HIGHPASS, pitch, 1, 2),
               damp_gain(0.001, 0.01, 0.01)})
            >> control_gain(0.1, 0.005)
            >> cut(0, 1000, length+1000, 0);
    }

    signal generate_white_to_tone(uint64_t length, double pitch, double purity)
    {
        SF_MARK_STACK;
        if (purity < 0.1 || purity > 2.0)
            SF_THROW(std::out_of_range{
                    "Purity out of range, must be between 0.1 and 2.0 was: " + std::to_string(purity)});
        uint64_t attack  = length * 0.25;
        uint64_t decay   = length * 0.5;
        uint64_t release = length * 0.75;
        auto pitch_env = generate_linear({ {0, pitch}, {length, pitch} });
        auto peak_q_env = generate_linear({
                {0,        0.1},
                {attack,  20 * purity},
                {decay ,  10 * purity},
                {release,  2 * purity},
                {length,   0}});
        auto peak_w_env = generate_linear({
                {0,       10.0},
                {attack,  0.2},
                {decay ,  1.0},
                {release, 2.0},
                {length,  10.0}});
        auto pass_q_env = generate_linear({
                {0,       0.1},
                {attack,  2},
                {decay ,  1},
                {release, 0.2},
                {length,  0.1}});
        auto pass_w_env = generate_linear({
                {0,       0.1},
                {attack,  1},
                {decay ,  1},
                {release, 0.2},
                {length,  0.1}});

        auto cycle = [&](signal& input)
        {
            auto peak = filter_shaped_rbj(filter_type::PEAK);
            input >> peak;
            copy(pitch_env) >> peak;
            copy(peak_w_env) >> peak;
            copy(peak_q_env) >> peak;
            peak >> damp_gain(0.001, 0.01, 0.01);
            auto low = filter_shaped_rbj(filter_type::LOWPASS);
            peak >> low;
            copy(pitch_env) >> low;
            copy(pass_w_env) >> low;
            copy(pass_q_env) >> low;
            auto high = filter_shaped_rbj(filter_type::HIGHPASS);
            low >> high;
            copy(pitch_env) >> high;
            copy(pass_w_env) >> high;
            copy(pass_q_env) >> high;
            return high >> damp_gain(0.001, 0.01, 0.01);
        };

        auto input = generate_noise(length) >> seed(pitch, 0.1, 0.0);
        auto step1 = cycle(input);
        auto step2 = cycle(step1);
        auto step3 = cycle(step2);
        return step3 >> control_gain(0.1, 0.005);
    }

    // Pans left to right from a single signal returning the two output signals left and right as a pair.
    std::pair<signal, signal> pan_lr(signal input, double pan_start, double pan_end, uint64_t length)
    {
        SF_MARK_STACK;
        if (pan_start > 1.0 || pan_start < 0.0)
            SF_THROW(std::invalid_argument("pan_start out of range (0.0 - 1.0) was: " + std::to_string(pan_start)));
        if (pan_end > 1.0 || pan_end < 0.0)
            SF_THROW(std::invalid_argument("pan_end out of range (0.0 - 1.0) was: " + std::to_string(pan_end)));
        auto do_pan = [&](signal sig, double s, double e)
        {
            auto pan_mixer = mix(mixer_type::MULTIPLY);
            sig 
                >> cut(0, 0, length, length)
                >> cut(0, 0, length, 0)
                >> pan_mixer;
            generate_linear({ { 0, s}, {length, e } })
                >> pan_mixer;
            return pan_mixer;
        };
        auto sig_store = input >> store();
        return {
            do_pan(copy(sig_store), pan_start, pan_end),
            do_pan(sig_store, 1.0-pan_start, 1.0-pan_end)};
    }

    std::pair<signal, signal> haas(signal left, signal right, uint64_t delay, double amount, uint64_t length)
    {
        SF_MARK_STACK;
        auto left_store = left >> store();
        auto right_store = right >> store();
        auto haas_mixer_l = mix(mixer_type::OVERLAY);
        copy(left_store)
            >> cut(delay, 0, length, 0)
            >> amplify(amount)
            >> haas_mixer_l;
        copy(right_store)
            >> haas_mixer_l;

        auto haas_mixer_r = mix(mixer_type::OVERLAY);
        right_store
            >> cut(delay, 0, length, 0)
            >> amplify(amount)
            >> haas_mixer_r;
        left_store
            >> haas_mixer_r;
        return {haas_mixer_l, haas_mixer_r};
    }

    double find_17scale(double target)
    {
        SF_MARK_STACK;
        static std::vector<double> notes{};
        constexpr double max_pitch = 1000000;
        if (notes.empty())
        {
            double pitch{1};
            double step{pow(2, 1.0/17.0)};
            while(pitch<max_pitch)
            {
                notes.push_back(pitch);
                pitch *= step;
            }
        }
        if (target < 1 || target > notes.back())
            SF_THROW(std::invalid_argument{"target pitch out of range: " + std::to_string(target)});
        double ret{0};
        double diff{max_pitch * 2.0};
        // This could complete early, but this algorithm should be fast enough.
        for(auto test: notes)
        {
            if (abs(target - test) < diff)
            {
                diff = abs(target - test);
                ret = test;
            }
        }
        return ret;
    }

    constrained_random_walk::constrained_random_walk(
            double min_value,
            double max_value,
            double step_size,
            double start):
        m_min_value{min_value}, m_max_value{max_value}, m_step_size{step_size}, m_state{start}
    {
        SF_MARK_STACK;
        if (m_state < m_min_value || m_state > m_max_value)
            SF_THROW(std::invalid_argument{"start out of range. start: " + std::to_string(start)});
        if (m_min_value >= m_max_value)
            SF_THROW(std::invalid_argument{"min_value >= max_value"});
    }

    double constrained_random_walk::operator()()
    {
        auto get_move = [&]
        {
            return m_gen() * m_step_size;
        };
        double state = m_state + get_move();
        while (state < m_min_value || state > m_max_value)
        {
            state = m_state + get_move();
        }
        m_state = state;
        return m_state;
    }

}
