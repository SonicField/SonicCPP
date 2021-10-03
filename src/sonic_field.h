#pragma once

//    Copyright (c) 2020 Alexander J. Turner.
//  This code is distributed under the terms of the GNU General Public License

//  MVerb is free software: you can redistribute it and/or modify
//  it under the terms of the GNU General Public License as published by
//  the Free Software Foundation, either version 3 of the License, or
//  at your option) any later version.
//
//  MVerb is distributed in the hope that it will be useful,
//  but WITHOUT ANY WARRANTY; without even the implied warranty of
//  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//  GNU General Public License for more details.
//
//  You should have received a copy of the GNU General Public License_WIN32
//  along with this MVerb.  If not, see <http://www.gnu.org/licenses/>.

#include <cstdint>
#include <cmath>
#include <string>
#include <functional>
#include <utility>
#include <unordered_map>
#include <unordered_set>
#include <fstream>
#include <iostream>
#include <time.h>
#include <limits>
#include <tuple>

#include "memory_manager.h"

namespace sonic_field
{
    constexpr double PI = 3.1415926535897932384626433832795;
    constexpr double MAX_FREQUENCY = double(SAMPLES_PER_SECOND >> 2);
    constexpr double ANGLE_RATE = 2.0 * PI / SAMPLES_PER_SECOND;


    void set_work_space(const std::string&);
    void set_output_space(const std::string&);
    const std::string& work_space();
    const std::string& output_space();
    std::string join_path(const std::vector<std::string>&);

    std::string temp_file_name();
    void delete_sig_file(const std::string& name);

    template<class C>
    class signal_impl
    {
    public:
        typedef C* R;
        std::vector<C> m_inputs;
        signal_impl() : m_inputs{}{}

        virtual double* next()
        {
            return nullptr;
        }

        template<typename L>
        double* process(const L& lambda, double* data)
        {
            return data == empty_block()? data: lambda(data);
        }

        template<typename L>
        double* process_no_skip(const L& lambda, double* data)
        {
            if (data == empty_block())
            {
                auto blank = new_block();
                return lambda(blank);
            }
            else
            {
                return lambda(data);;
            }
        }

        virtual void inject(C& in)
        {
            m_inputs.push_back(in);
        }

        virtual const char* name()
        {
            return "signal";
        }

        virtual void check_monophonic()
        {
            SF_MARK_STACK;
            if (m_inputs.size() > 1) SF_THROW(std::invalid_argument(std::string{ name() } +" must be monophonic"));
        }

        C& input(size_t idx)
        {
            return m_inputs.at(idx);
        }

        C& input()
        {
            return m_inputs.at(0);
        }

        size_t input_count()
        {
            return m_inputs.size();
        }

        virtual signal_impl* copy()
        {
            SF_MARK_STACK;
            SF_THROW(std::logic_error{ std::string{ "Cannot copy a "} + name() + " - must be overridden"});
        }

        virtual ~signal_impl()
        {
            //std::cerr << "Deleting " << name() << std::endl;
        }
    };

    class signal
    {
        typedef signal_impl<signal> wrapped_type;
        typedef wrapped_type* value_type;

        value_type m_signal;

        void check_dead()
        {
            if (!m_signal) SF_THROW(std::logic_error("Referencing dead signal"));
        }

        void kill()
        {
            m_signal = nullptr;
        }

        bool alive()
        {
            return m_signal != nullptr;
        }

    public:
        signal():m_signal { nullptr }{};
        signal(value_type s): m_signal{ s }{}
        signal(signal& s) : m_signal{ s.m_signal } {}
        signal(const signal& s) : m_signal{ s.m_signal } {}

        signal operator=(const signal& s)
        {
            m_signal = s.m_signal;
            return *this;
        }

        double* next()
        {
            return m_signal->next();
        }

        void clear()
        {
            if (!m_signal) return;
            delete m_signal;
        }

        signal operator >> (signal in)
        {
            //std::cerr << "Injecting: " << name() << " into " << in.name() << std::endl;
            in.inject(*this);
            return in;
        }

        void inject(signal in)
        {
            m_signal->inject(in);
        }

        const char* name()
        {
            if (m_signal)
                return m_signal->name();
            else
                return "<NULL>";
        }

        signal copy()
        {
            return { m_signal->copy() };
        }

        ~signal(){}
    };

    typedef signal_impl<signal> signal_base;

    class signal_mono_base: public signal_base
    {
    public:
        void inject(signal& in) override
        {
            signal_base::inject(in);
            check_monophonic();
        }

    };

    class signal_generator_base : public signal_base
    {
    public:
        void inject(signal& in)override
        {
            SF_THROW(std::logic_error{ "Cannot pass signal into a generator" });
        }
    };

    struct position_and_amplitude : public std::tuple<uint64_t, double>
    {
        using std::tuple<uint64_t, double>::tuple;

        uint64_t position() const
        {
            return std::get<0>(*this);
        }

        double amplitude() const
        {
            return std::get<1>(*this);
        }
    };

    using envelope = std::vector<position_and_amplitude>;

    struct scope
    {
        scope();
        ~scope();
    };

    signal& add_to_scope(signal sig);

    // Utilities
    // =========
    template<class L>
    inline void time_it(const char* msg, const L& what)
    {
        timespec ts0;
        timespec_get(&ts0, TIME_UTC);
        what();
        timespec ts1;
        timespec_get(&ts1, TIME_UTC);
        auto millis = (ts1.tv_nsec - ts0.tv_nsec) / 1000000;
        millis += (ts1.tv_sec - ts0.tv_sec) * 1000;
        auto secs = millis / 1000;
        millis %= 1000;
        std::cerr << "Action " << msg << " took " << secs << "." << millis << "s" << std::endl;
    }

    void signal_to_wav(const std::string&);

    class wav_file_reader;
    class wav_reader : public signal_generator_base
    {
        wav_file_reader* m_reader;

    public:
        wav_reader() = delete;
        explicit wav_reader(const std::string& name);
        virtual double* next() override;
        virtual const char* name() override;
        virtual ~wav_reader() override;
    };

    inline signal read_wav(const std::string& file_name)
    {
        SF_MESG_STACK("read_wav - create wav_reader");
        return add_to_scope({ new wav_reader{file_name} });
    }

    template<typename T>
    inline T fast_cos(T x) noexcept
    {
        constexpr T tp = 1.0 / (2.0 * PI);
        x *= tp;
        x -= T(0.25) + std::floor(x + T(0.25));
        x *= T(16.0) * (std::abs(x) - T(0.5));
        x += T(.225) * x * (std::abs(x) - T(1.));
        return x;
    }

    // Definitions of processors
    // =========================

    #pragma pack(push, 1)
    struct signal_file_header
    {
        float dc_offset;
        float peak_negative;
        float peak_positive;
    };
    #pragma pack(pop)

    class decimator
    {
        double R1, R2, R3, R4, R5, R6, R7, R8, R9;
        double h0, h1, h3, h5, h7, h9;

    public:
        decimator();
        double decimate(double, double);
    };

    class signal_writer : public signal_mono_base
    {
        const std::string m_name;
        std::ofstream m_out;
        decimator m_decimate;
        signal_file_header m_header;
        bool m_runner;

    public:
        signal_writer() = delete;
        explicit signal_writer(const std::string& name, bool is_runner=false);
        virtual void inject(signal& in) override;
        virtual double* next() override;
        virtual const char* name() override;
        void set_runner(signal);
    };

    class runner : public signal_mono_base
    {
    public:
        virtual void inject(signal& in) override;
        virtual const char* name() override;
        virtual double* next() override;
    };

    inline signal run()
    {
        SF_MARK_STACK;
        return add_to_scope({ new runner{} });
    }

    inline signal write(const std::string& file_name)
    {
        SF_MARK_STACK;
        return add_to_scope({ new signal_writer{file_name, true} });
    }

    class noise_generator : public signal_generator_base
    {
        uint32_t m_state;
        uint64_t m_len;
    public:
        noise_generator() = delete;
        explicit noise_generator(uint64_t len);
        double next_rand();
        virtual double* next() override;
        virtual const char* name() override;
        virtual signal_base* copy() override;
    };

    inline signal generate_noise(uint64_t len)
    {
        SF_MESG_STACK("generate_noise - create noise generator");
        return add_to_scope({ new noise_generator{len} });
    }

    class silence_generator : public signal_generator_base
    {
        uint64_t m_len;
    public:
        silence_generator() = delete;
        explicit silence_generator(uint64_t len);
        virtual double* next() override;
        virtual const char* name() override;
        virtual signal_base* copy() override;
    };

    inline signal generate_silence(uint64_t len)
    {
        SF_MESG_STACK("generate_silence - create silence generator");
        return add_to_scope({ new silence_generator{len} });
    }

    class linear_generator :  public signal_generator_base
    {
        envelope m_points;
        uint64_t m_position;
        uint64_t m_point;
    public:
        linear_generator() = delete;
        explicit linear_generator(const envelope&);
        virtual double* next() override;
        virtual const char* name() override;
        virtual signal_base* copy() override;
    };

    inline signal generate_linear(envelope points)
    {
        SF_MARK_STACK;
        return add_to_scope({ new linear_generator{points} });
    }

    class gain_controller : public signal_mono_base
    {
        double m_scale;
        double m_attack;
        double m_release;
        double m_arg_attack;
        double m_arg_release;

    public:
        gain_controller() = delete;
        explicit gain_controller(double attack, double release);
        explicit gain_controller(double scale, double attack, double release);
        virtual double* next() override;
        virtual const char* name() override;
        virtual signal_base* copy() override;
    };

    inline signal control_gain(double attack, double release)
    {
        SF_MARK_STACK;
        return add_to_scope({ new gain_controller{attack, release} });
    }

    inline signal damp_gain(double scale, double attack, double release)
    {
        SF_MARK_STACK;
        return add_to_scope({ new gain_controller{scale, attack, release} });
    }

    enum class filter_type
    {
        LOWPASS,
        HIGHPASS,
        BANDPASS_SKIRT,
        BANDPASS_PEAK,
        NOTCH,
        ALLPASS,
        PEAK,
        LOWSHELF,
        HIGHSHELF
    };
    /*
        * RBJ Filters from C++ version by arguru[AT]smartelectronix[DOT]com based on eq filter cookbook by Robert Bristow-Johnson
        * <rbj@audioimagination.com> This code is believed to be domain and license free after best efforts to establish its
        * licensing.
        */
    class rbj_filter : public signal_mono_base
    {
        // filter coeffs
        double b0a0, b1a0, b2a0, a1a0, a2a0;

        // in/out history
        double ou1, ou2, in1, in2;

    public:

        rbj_filter(filter_type type, double frequency, double q, double db_gain);
        rbj_filter(double b0a0, double  b1a0, double  b2a0, double  a1a0, double a2a0);

        double filter(double in0);
        virtual double* next() override;
        virtual const char* name() override;
        virtual signal_base* copy() override;

        struct memory
        {
            double m_ou1;
            double m_ou2;
            double m_in1;
            double m_in2;
            memory(): m_ou1{0}, m_ou2{0}, m_in1{0}, m_in2{0}
            {}
            memory(double o1, double o2, double i1, double i2):
                m_ou1{o1}, m_ou2{o2}, m_in1{i1}, m_in2{i2}
            {}
        };

        memory store_memory() const;
        void restore_memory(const memory&);

    };

    inline signal filter_rbj(filter_type type, double frequency, double q, double db_gain)
    {
        SF_MARK_STACK;
        return add_to_scope({ new rbj_filter(type, frequency, q, db_gain) });
    }

    class shaped_rbj : public signal_base
    {
        rbj_filter::memory m_memory;
        filter_type m_type;
    public:
        shaped_rbj(filter_type);
        virtual double* next() override;
        virtual const char* name() override;
        virtual signal_base* copy() override;
    };

    inline signal filter_shaped_rbj(filter_type type)
    {
        SF_MARK_STACK;
        return add_to_scope({ new shaped_rbj(type) });
    }

    enum class clean_level
    {
        NORMAL,
        MILD,
        NONE
    };

    class signal_reader : public signal_generator_base
    {
        const std::string m_name;
        std::ifstream m_in;
        uint64_t m_len;
        double m_scale;
        rbj_filter m_filter;
        clean_level m_clean_level;
        uint64_t m_position;

    public:
        signal_reader() = delete;
        explicit signal_reader(const std::string& name, clean_level);
        virtual double* next() override;
        virtual const char* name() override;
    };

    inline signal read(const std::string& file_name, clean_level clean = clean_level::NORMAL)
    {
        SF_MARK_STACK;
        return add_to_scope({ new signal_reader{file_name, clean} });
    }

    class repeater : public signal_mono_base
    {
        std::vector<signal> m_chain;

    public:
        repeater() = delete;
        explicit repeater(uint64_t count, std::vector<signal>& chain);
        virtual void inject(signal&) override;
        virtual double* next() override;
        virtual const char* name() override;
        virtual signal_base* copy() override;
    };

    inline signal repeat(uint64_t count, std::vector<signal> chain)
    {
        SF_MARK_STACK;
        return add_to_scope({ new repeater{count, chain} });
    }

    enum class mixer_type
    {
        MULTIPLY,
        MULTIPLY_AND_ZERO,
        ADD,
        APPEND,
        OVERLAY
    };

    class mixer : public signal_base
    {
        mixer_type m_mode;
        double* mix_with();
        double* mix_append();
    public:
        explicit mixer(mixer_type);
        virtual double* next() override;
        virtual const char* name() override;
        friend signal mix(mixer_type);
    };

    inline signal mix(mixer_type mode)
    {
        SF_MESG_STACK("mix - create mixer");
        return add_to_scope({ new mixer{ mode } });
    }

    class seeder : public signal_mono_base
    {
        double m_pitch;
        double m_amplitude;
        double m_phase;
        uint64_t m_position;

    public:
        seeder() = delete;
        explicit seeder(double pitch, double amplitude, double phase);
        virtual double* next() override;
        virtual const char* name() override;
        virtual signal_base* copy() override;
    };

    inline signal seed(double pitch, double amplitude, double phase)
    {
        SF_MESG_STACK("seed - create seeder");
        return add_to_scope({ new seeder{pitch, amplitude, phase} });
    }

    class power : public signal_mono_base
    {
        double m_factor;

    public:
        power() = delete;
        explicit power(double factor);
        virtual double* next() override;
        virtual const char* name() override;
        virtual signal_base* copy() override;
    };

    inline signal distort_power(double factor)
    {
        SF_MESG_STACK("distort_power - create power distorter");
        return add_to_scope({ new power{factor} });
    }

    class saturater : public signal_mono_base
    {
        double m_factor;

    public:
        saturater() = delete;
        explicit saturater(double factor);
        virtual double* next() override;
        virtual const char* name() override;
        virtual signal_base* copy() override;
    };

    inline signal distort_saturate(double factor)
    {
        SF_MESG_STACK("distort_saturate - create saturate distorter");
        return add_to_scope({ new saturater{factor} });
    }

    class amplifier : public signal_mono_base
    {
        double m_factor;

    public:
        amplifier() = delete;
        explicit amplifier(double factor);
        virtual double* next() override;
        virtual const char* name() override;
        virtual signal_base* copy() override;
    };

    inline signal amplify(double factor)
    {
        SF_MESG_STACK("amplify - create amplifier");
        return add_to_scope({ new amplifier{factor} });
    }

    class wrapper : public signal_mono_base
    {
        signal m_front;
        signal m_back;

    public:
        wrapper() = delete;
        explicit wrapper(signal, signal);
        virtual void inject(signal&) override;
        virtual double* next() override;
        virtual const char* name() override;
        virtual signal_base* copy() override;
    };

    inline signal wrap(signal front, signal back)
    {
        SF_MESG_STACK("wrap - create wrapper");
        return add_to_scope({ new wrapper{front, back} });
    }

    inline signal copy(signal& input)
    {
        SF_MESG_STACK("copy - create copy");
        return add_to_scope(input.copy());
    }

    class fft
    {
        const uint64_t m_n, m_m;

        // Lookup tables. Only need to recompute when size of FFT changes.
        double* m_cos;
        double* m_sin;
    public:
        fft(uint64_t n, bool isForward);
        void compute(double* x, double* y);
        ~fft();
    };

    }

    namespace mverb
    {
    template<typename T> class MVerb;
    }

    namespace sonic_field
    {
    typedef mverb::MVerb<double> mreverb;

    std::unique_ptr<mreverb> create_mreverb(
        double damping_freq,
        double density,
        double bandwidth_freq,
        double decay,
        double predelay,
        double size,
        double gain,
        double mix,
        double early_mix);

    class mreverberator : public signal_base
    {
        std::unique_ptr<mreverb> m_reverb;
        signal m_left;
        signal m_right;

    public:
        mreverberator() = delete;
        explicit mreverberator(
            const std::string& left,
            const std::string& right,
            double damping_freq,
            double density,
            double bandwidth_freq,
            double decay,
            double predelay,
            double size,
            double gain,
            double mix,
            double early_mix);
        virtual double* next() override;
        virtual const char* name() override;
        virtual signal_base* copy() override;
        virtual void inject(signal&) override;
    };

    inline signal mreverberate(
        const std::string& left,
        const std::string& right,
        double damping_freq,
        double density,
        double bandwidth_freq,
        double decay,
        double predelay,
        double size,
        double gain,
        double mix,
        double early_mix)
    {
        SF_MESG_STACK("mreverberate - create mreverberator");
        return add_to_scope({ new mreverberator{left, right, damping_freq, density, bandwidth_freq,
            decay, predelay, size, gain, mix, early_mix} });
    }

    class shaped_ladder;

    class ladder_filter_driver : public signal_base
    {
        shaped_ladder* m_ladder;
    public:
        ladder_filter_driver();
        virtual double* next() override;
        virtual const char* name() override;
        virtual signal_base* copy() override;
        virtual ~ladder_filter_driver();
    };

    inline signal ladder_filter()
    {
        SF_MESG_STACK("ladder_filter - create saturate ladder_filter_driver");
        return add_to_scope({ new ladder_filter_driver{} });
    }

    class echo_chamber : public signal_mono_base
    {
        double* m_buffer;
        uint64_t m_delay;
        double m_feedback;
        double m_mix;
        double m_saturate;
        double m_wow;
        double m_flutter;
        uint64_t m_index;

    public:
        echo_chamber() = delete;
        explicit echo_chamber(
            uint64_t delay,
            double feedback,
            double mix,
            double saturate,
            double wow,
            double flutter
            );
        virtual double* next() override;
        virtual const char* name() override;
        virtual signal_base* copy() override;
        virtual ~echo_chamber();
    };

    inline signal echo(
        uint64_t delay,
        double feedback,
        double mix,
        double saturate,
        double wow,
        double flutter)
    {
        SF_MESG_STACK("echo - create echo_chamber");
        return add_to_scope({ new echo_chamber{delay, feedback, mix, saturate, wow, flutter } });
    }

    class warmer : public signal_mono_base
    {
        double m_cube_amount;
        double m_max_difference;
    public:
        warmer() = delete;
        explicit warmer(
                double cube_amount,
                double max_difference
                );
        virtual double* next() override;
        virtual const char* name() override;
        virtual signal_base* copy() override;
    };

    inline signal warm(
            double cube_amount,
            double max_difference
            )
    {
        SF_MESG_STACK("warm - create warmer");
        return add_to_scope({ new warmer{cube_amount, max_difference} });
    }

    class random_doubles
    {
        uint32_t m_state;
    public:
        random_doubles();
        double operator()();
    };

    class cutter : public signal_mono_base
    {
        uint64_t m_pad_before;
        uint64_t m_from;
        uint64_t m_to;
        uint64_t m_pad_after;
        uint64_t m_position;
        bool m_done;

    public:
        cutter() = delete;
        explicit cutter(uint64_t pad_before, uint64_t from, uint64_t to, uint64_t pad_after);
        virtual double* next() override;
        virtual const char* name() override;
        virtual signal_base* copy() override;
    };

    inline signal cut(uint64_t pad_before, uint64_t from, uint64_t to, uint64_t pad_after)
    {
        SF_MESG_STACK("cut - create cutter");
        return add_to_scope({ new cutter{pad_before, from, to, pad_after} });
    }

    class sweeper : public signal_generator_base
    {
        double m_start_frequency;
        double m_end_frequency;
        uint64_t m_length;
        uint64_t m_position;

    public:
        sweeper() = delete;
        sweeper(double start_frequency, double end_frequency, uint64_t length);
        virtual double* next() override;
        virtual const char* name() override;
        virtual signal_base* copy() override;
    };

    inline signal generate_sweep(double start_frequency, double end_frequency, uint64_t length)
    {
        SF_MESG_STACK("sweeper - create sweeper");
        return add_to_scope({ new sweeper{start_frequency, end_frequency, length} });
    }

    class shepard : public signal_generator_base
    {
        double m_start_frequency;
        double m_end_frequency;
        uint64_t m_length;
        uint64_t m_cycle_length;
        double m_step;
        std::vector<double> m_pitches;

    public:
        shepard() = delete;
        shepard(double start_frequency, double end_frequency, uint64_t cycle_length, uint64_t length);
        virtual double* next() override;
        virtual const char* name() override;
        virtual signal_base* copy() override;
    };

    inline signal generate_shepard(double start_frequency, double end_frequency, uint64_t cycle_length, uint64_t length)
    {
        SF_MESG_STACK("shepard - create shepard");
        return add_to_scope({ new shepard{start_frequency, end_frequency, cycle_length, length} });
    }

    class storer : public signal_mono_base
    {
        std::vector<double*> m_store;
        uint64_t m_position;

    public:
        storer(): m_store{}, m_position{0}{}
        virtual void inject(signal&) override;
        virtual double* next() override;
        virtual const char* name() override;
        virtual signal_base* copy() override;
    };

    inline signal store()
    {
        SF_MESG_STACK("storer - create store");
        return add_to_scope({ new storer{} });
    }

    class leveler : public signal_mono_base
    {
        std::vector<double*> m_store;
        uint64_t m_position;
        double m_scale;

    public:
        leveler(): m_store{}, m_position{0}, m_scale{0}{}
        virtual void inject(signal&) override;
        virtual double* next() override;
        virtual const char* name() override;
        virtual signal_base* copy() override;
    };

    inline signal level_store()
    {
        SF_MESG_STACK("level_store - create leveler");
        return add_to_scope({ new leveler{} });
    }

    class situator : public signal_mono_base
    {
    public:
        typedef std::vector<std::pair<uint64_t, double>> situator_input_t;

    private:
        situator_input_t m_taps;
        double* m_buffer;
        uint64_t m_length;
        uint64_t m_position;

    public:
        situator() = delete;
        situator(situator_input_t&);
        virtual double* next() override;
        virtual const char* name() override;
        virtual signal_base* copy() override;
        virtual ~situator();
    };

    inline signal situate(situator::situator_input_t taps)
    {
        SF_MESG_STACK("situate - create situator");
        return add_to_scope({ new situator{taps} });
    }

    /*
    class subsampler : public signal_mono_base
    {
         decimator m_decimator;

    public:
        subsampler();
        virtual double* next() override;
        virtual const char* name() override;
        virtual signal_base* copy() override;
    };

    inline signal amplify(double factor)
    {
        SF_MESG_STACK("subsample - create subsampler");
        return add_to_scope({ new subsampler{} });
    }

    class supersampler : public signal_mono_base
    {
        rbj_filter m_filter;

    public:
        supersampler();
        virtual double* next() override;
        virtual const char* name() override;
        virtual signal_base* copy() override;
    };

    inline signal supersample(double factor)
    {
        SF_MESG_STACK("supersample - create supersampler");
        return add_to_scope({ new supersampler{} });
    }
    */

} // sonic_field
