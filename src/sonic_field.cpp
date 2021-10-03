#include "sonic_field.h"
#include <math.h>
#include <atomic>

namespace sonic_field
{
    thread_local std::vector<std::vector<signal>> scopes{};

    scope::scope()
    {
        SF_NO_TRACK;
        scopes.emplace_back(std::vector<signal>{});
    }
    scope::~scope()
    {
        for (auto& x : scopes.back())
        {
            //std::cerr << "Clearing: " << x.name() << std::endl;
            x.clear();
        }
        SF_NO_TRACK;
        scopes.pop_back();
    }

    signal& add_to_scope(signal sig)
    {
        SF_NO_TRACK;
        SF_MESG_STACK("Add to scope");
        if (!scopes.size()) SF_THROW(std::logic_error{ "No current scope - have you defined a scope?" });
        scopes.back().push_back(sig);
        return scopes.back().back();
    }

    std::string WORK_SPACE{};
    void set_work_space(const std::string& dir)
    {
        WORK_SPACE = dir;
        std::cerr << "Setting Work Space To: '" << dir << "'" << std::endl;
    }
    std::string OUTPUT_SPACE{};
    void set_output_space(const std::string& dir)
    {
        OUTPUT_SPACE = dir;
        std::cerr << "Setting Output Space To: '" << dir << "'" << std::endl;
    }

    const std::string& work_space()
    {
        if (WORK_SPACE == "") throw std::logic_error("Work space not set");
        return WORK_SPACE;
    }

    const std::string& output_space()
    {
        if (OUTPUT_SPACE == "") throw std::logic_error("Output space not set");
        return OUTPUT_SPACE;
    }

    std::string temp_file_name()
    {
        static std::atomic<uint64_t> counter{0};
        return "_temp_" + std::to_string(++counter);
    }

    void delete_sig_file(const std::string& name)
    {
        auto fname = work_space() + name + ".sig";
        if (std::remove(fname.c_str()))
           SF_THROW(std::runtime_error{"Failed to remove file: " + fname});
    }

    // TODO: I we end up only using one delete the other.
    #ifdef _WIN32
        constexpr auto PATH_SEP_C = '\\';
    #else
        constexpr auto PATH_SEP_C = '/';
    #endif

    std::string join_path(const std::vector<std::string>& parts)
    {
        auto len = parts.size();
        if (!len) return "";

        std::string ret{};
        // Compute final size.
        decltype(len) tlen{ 0 };
        for (const auto& p : parts) tlen += p.size();
        // Including separators (which we assume are single character).
        tlen += parts.size() - 1;
        ret.reserve(tlen);

        // Now fill the reserved space.
        ret += parts[0];
        decltype(len) idx{ 1 };
        while (idx < len)
        {
            ret += PATH_SEP_C;
            ret += parts[idx];
            ++idx;
        }
        return ret;
    }

    signal_reader::signal_reader(const std::string& name, clean_level clean):
        m_name{ work_space() + name + ".sig" },
        m_filter{ filter_type::LOWPASS, MAX_FREQUENCY, 1.0, 0.0 },
        m_clean_level{ clean },
        m_position{ 0 }
    {
        signal_file_header header{};
        m_in.open(m_name, std::ifstream::ate | std::ifstream::binary);
        if (!m_in)
            SF_THROW(std::out_of_range{ "could not open file " + m_name});

        m_len = (uint64_t(m_in.tellg()) - sizeof(header)) / sizeof(float);
        m_in.seekg(0);
        m_in.read(reinterpret_cast<char*>(&header), sizeof(header));
        if (!m_in) SF_THROW(std::out_of_range{ "signal file corrupt" });
        m_scale = -header.peak_negative > header.peak_positive ?
            -1.0 / header.peak_negative : 1.0 / header.peak_positive;
        if (m_len % WIRE_BLOCK_SIZE != 0)
            SF_THROW(std::out_of_range{ "signal file not integer number of blocks corrupt: " + m_name});
        switch (clean)
        {
        case clean_level::NONE:
            break;
        case clean_level::MILD:
            {
                // Stablise the filter to the first imput value.
                float samp{ 0 };
                m_in.read(reinterpret_cast<char*>(&samp), sizeof(float));
                m_in.seekg(sizeof(header));
                double dsamp = double(samp);
                // Definitely don't need this many loops - but what is a reasonable value?
                for (uint64_t idx{ 0 }; idx < BLOCK_SIZE; ++idx)
                {
                    m_filter.filter(dsamp);
                }
            }
            break;
        case clean_level::NORMAL:
            break;
        default:
            SF_THROW(std::invalid_argument{ "Unknown clean level: " + std::to_string(uint64_t(clean)) });
        }
        std::cerr << "Reading Signal:  name: " << m_name << " dc: " << header.dc_offset << " peak neg: "
                    << header.peak_negative << " peak pos: " << header.peak_positive
                    << " len: " << m_len << std::endl;
    }

    double* signal_reader::next()
    {
        SF_MESG_STACK("signal_reader::next");
        if (!m_len)
        {
            m_in.close();
            return nullptr;
        }
        double* ret = new_block(false);
        float buf[WIRE_BLOCK_SIZE];
        m_in.read(reinterpret_cast<char*>(&buf), sizeof(buf));
        if (!m_in)
            SF_THROW(std::out_of_range{ "signal file corrupt" });
        auto jdx = 0;
        switch (m_clean_level)
        {
        case clean_level::NONE:
            for (uint64_t idx{ 0 }; idx < WIRE_BLOCK_SIZE; ++idx)
            {
                // Very fast upscale which will probably be good enough most of the time.
                auto v = double(buf[idx]) * m_scale;
                ret[jdx++] = v;
                ret[jdx++] = v;
            }
            break;
        case clean_level::MILD:
        case clean_level::NORMAL:
            for (uint64_t idx{ 0 }; idx < WIRE_BLOCK_SIZE; ++idx)
            {
                // Use the filter to remove harmonics created by naive upscaling.
                auto v = double(buf[idx]) * m_scale;
                ret[jdx++] = m_filter.filter(v);
                ret[jdx++] = m_filter.filter(v);
            }
            break;
        default:
            SF_THROW(std::logic_error{ "This code should never be reached" });
        }

        if (m_clean_level == clean_level::NORMAL)
        {
            if (m_len < 11 * WIRE_BLOCK_SIZE)
            {
                double size = 10 * WIRE_BLOCK_SIZE;
                double scale = 1.0 - ((size - double(m_len))/ size);
                constexpr double step = 1.0 / (10 * BLOCK_SIZE);
                for (uint64_t idx{ 0 }; idx < BLOCK_SIZE; ++idx)
                {
                    ret[idx] *= scale;
                    scale -= step;
                }
            }
            else if (m_position < 10 * WIRE_BLOCK_SIZE)
            {
                double size = 10 * WIRE_BLOCK_SIZE;
                constexpr double step = 1.0 / (10 * BLOCK_SIZE);
                double scale = 1.0 - ((size - double(m_position)) / size);
                for (uint64_t idx{ 0 }; idx < BLOCK_SIZE; ++idx)
                {
                    ret[idx] *= scale;
                    scale += step;
                }
            }
        }
        m_len -= WIRE_BLOCK_SIZE;
        m_position += WIRE_BLOCK_SIZE;
        return ret;
    }

    const char* signal_reader::name()
    {
        return "reader";
    }

    const char* storer::name()
    {
        return "storer";
    }

    void storer::inject(signal& in)
    {
        SF_MARK_STACK;
        signal_mono_base::inject(in);
        while (auto block = in.next())
        {
            m_store.emplace_back(block);
        }
    }

    double* storer::next()
    {
        if (m_position < m_store.size())
        {
            auto ret = m_store.at(m_position);
            ++m_position;
            return ret;
        }
        else
            return nullptr;
    }

    signal_base* storer::copy()
    {
        SF_MARK_STACK;
        if (m_position != 0)
            SF_THROW(std::logic_error{"Trying to copy a used store"});
        storer* ret = new storer();
        for(auto b: m_store)
        {
            if (b == empty_block())
            {
                ret->m_store.push_back(b);
            }
            else
            {
                auto nb = new_block();
                memcpy(nb, b, BLOCK_SIZE*sizeof(double));
                ret->m_store.emplace_back(nb);
            }
        }
        return ret;
    }

    const char* leveler::name()
    {
        return "leveler";
    }

    void leveler::inject(signal& in)
    {
        SF_MARK_STACK;
        signal_mono_base::inject(in);
        while (auto block = in.next())
        {
            m_store.emplace_back(block);
            for(uint64_t i{0}; i < BLOCK_SIZE; ++i)
                m_scale = std::fmax(std::abs(block[i]), m_scale);
        }
        m_scale = 1.0 / m_scale;
    }

    double* leveler::next()
    {
        if (m_position < m_store.size())
        {
            auto ret = m_store.at(m_position);
            for(uint64_t i{0}; i < BLOCK_SIZE; ++i)
                ret[i] *= m_scale;
            ++m_position;
            return ret;
        }
        else
            return nullptr;
    }

    signal_base* leveler::copy()
    {
        SF_MARK_STACK;
        if (m_position != 0)
            SF_THROW(std::logic_error{"Trying to copy a used leveler"});
        leveler* ret = new leveler();
        for(auto b: m_store)
        {
            if (b == empty_block())
            {
                ret->m_store.push_back(b);
            }
            else
            {
                auto nb = new_block();
                memcpy(nb, b, BLOCK_SIZE*sizeof(double));
                ret->m_store.emplace_back(nb);
            }
        }
        ret->m_scale = m_scale;
        return ret;
    }

    signal_writer::signal_writer(const std::string& name, bool is_runner) :
        m_name{ work_space() + name + ".sig" },
        m_header{ 0,0,0 },
        m_runner{is_runner}
        {}

    void signal_writer::inject(signal& in)
    {
        SF_MESG_STACK("signal_writer::inject");
        signal_mono_base::inject(in);
        {
            SF_NO_TRACK; // Leaks bytes the first time it is called.
            m_out = std::ofstream{ m_name,std::ios_base::binary };
            if (!m_out)
                SF_THROW(std::logic_error{
                        std::string{ "In " } +name() + ": output stream is invalid: " +
                        strerror(errno)});
        }
        m_out.write(reinterpret_cast<char*>(&m_header), sizeof(m_header));
        if (m_runner)
        {
            while(auto block = next())
            {
                free_block(block);
            };
        }
    }

    double* signal_writer::next()
    {
        SF_MESG_STACK("signal_writer::next");
        return process_no_skip([&](double* block) {
            if (!m_out)
                SF_THROW(std::logic_error{
                        std::string{ "In " } +name() + ": output stream is invalid: " +
                        strerror(errno)});
            if (m_out.tellp() == sizeof(m_header))
            {
                // Prefeed the decimator with the first value.
                for(uint64_t idx{ 0 }; idx < 8; ++idx)
                    m_decimate.decimate(block[0], block[0]);
            }
            if (block)
            {
                float buff[WIRE_BLOCK_SIZE];
                for(uint64_t idx{ 0 }; idx < BLOCK_SIZE; ++idx)
                {
                    auto v = m_decimate.decimate(block[idx], block[idx + 1]);
                    ++idx;
                    m_header.dc_offset += v;
                    if (v < m_header.peak_negative)
                        m_header.peak_negative = v;
                    else if (v > m_header.peak_positive)
                        m_header.peak_positive = v;
                    buff[idx>>1] = v;
                }
                m_out.write(reinterpret_cast<char*>(buff), sizeof(buff));
            }
            else
            {
                m_header.dc_offset /= (uint64_t(m_out.tellp()) - sizeof(m_header)) / sizeof(float);
                m_out.seekp(0);
                m_out.write(reinterpret_cast<char*>(&m_header), sizeof(m_header));
                std::cerr << "Writing Signal:  name: " << m_name << " dc: " << m_header.dc_offset << " peak neg: "
                    << m_header.peak_negative << " peak pos: " << m_header.peak_positive << std::endl;
                m_out.close();
            }
            return block;
            }, input().next());
    }

    const char* signal_writer::name()
    {
        return "writer";
    }

    void runner::inject(signal& in)
    {
        signal_mono_base::inject(in);
        while (auto block = in.next())
        {
            process([](double* data) {free_block(data); return nullptr; }, block);
        }
    }

    const char* runner::name()
    {
        return "runner";
    }

    double* runner::next()
    {
        SF_MARK_STACK;
        SF_THROW(std::logic_error{ "Cannot call next on a runner" });
    }

    noise_generator::noise_generator(uint64_t len)
    {
        timespec ts;
        timespec_get(&ts, TIME_UTC);
        m_state = ts.tv_nsec ^ uint32_t(ts.tv_sec);
        m_len = len;
    }

    signal_base* noise_generator::copy()
    {
        SF_MARK_STACK;
        return new noise_generator(m_len);
    }

    /* Algorithm "xor" from p. 4 of Marsaglia, "Xorshift RNGs" */
    double noise_generator::next_rand()
    {
        m_state ^= m_state << 13;
        m_state ^= m_state >> 17;
        m_state ^= m_state << 5;
        // Normalize
        double ret = std::numeric_limits<int32_t>::max();
        ret -= m_state;
        ret /= std::numeric_limits<int32_t>::min();
        return ret;
    }

    double* noise_generator::next()
    {
        SF_MESG_STACK("noise_generator::next");
        if (!m_len)
        {
            return nullptr;
        }
        --m_len;
        auto ret = new_block(false);
        for (uint64_t idx{ 0 }; idx < BLOCK_SIZE; ++idx)
        {
            ret[idx] = next_rand();
        }
        return ret;
    }

    const char* noise_generator::name()
    {
        return "noise_generator";
    }

    random_doubles::random_doubles()
    {
        timespec ts;
        timespec_get(&ts, TIME_UTC);
        m_state = rand() ^ ts.tv_nsec ^ uint32_t(ts.tv_sec);
    }

    double random_doubles::operator()()
    {
        m_state ^= m_state << 13;
        m_state ^= m_state >> 17;
        m_state ^= m_state << 5;
        // Normalize
        double ret = std::numeric_limits<int32_t>::max();
        ret -= m_state;
        ret /= std::numeric_limits<int32_t>::min();
        return ret;
    }

    silence_generator::silence_generator(uint64_t len)
    {
        m_len = len;
    }

    signal_base* silence_generator::copy()
    {
        SF_MARK_STACK;
        return new silence_generator(m_len);
    }

    double* silence_generator::next()
    {
        if (!m_len--) return nullptr;
        return empty_block();
    }

    const char* silence_generator::name()
    {
        return "silence_generator";
    }

    linear_generator::linear_generator(const envelope& in):
        m_points{ in },
        m_position{ 0 },
        m_point{ 0 }
    {
        if (m_points.size() < 2) SF_THROW(std::invalid_argument{ "Must be at least two points for a linear generator" });
        if (m_points[0].position() != 0) SF_THROW(std::invalid_argument{ "Linear generator first point must be at zero" });
        uint64_t p0{ 0 };
        for (auto& pa : m_points)
        {
            if (p0 && pa.position() <= p0) SF_THROW(std::invalid_argument{ "Linear generator points must each be later than the previous" });
            p0 = pa.position();
        }
    }

    signal_base* linear_generator::copy()
    {
        SF_MARK_STACK;
        return new linear_generator(m_points);
    }

    double* linear_generator::next()
    {
        SF_MESG_STACK("linear_generator::next");
        if (m_point + 1 == m_points.size()) return nullptr;
        if (m_point + 1 > m_points.size()) SF_THROW(std::logic_error{ "Impossible m_point value in linear_generator" });

        double* data = new_block(false);
        auto frst_pos = m_points[m_point];
        auto scnd_pos = m_points[m_point + 1];
        auto frst_at = frst_pos.position() * BLOCK_SIZE;
        auto scnd_at = scnd_pos.position() * BLOCK_SIZE;
        auto len = scnd_at - frst_at;
        for (uint64_t idx{ 0 }; idx < BLOCK_SIZE; ++idx)
        {
            auto offset = m_position - frst_at;
            double rto = double(offset) / double(len);
            data[idx] = frst_pos.amplitude() * (1.0 - rto) + scnd_pos.amplitude() * rto;
            ++m_position;
        }
        // This can only happen at the end of a block because the minimum envelope point spacing is 1ms
        // which is the size of a block.
        if (m_position == scnd_at) ++m_point;
        if (m_position > scnd_at) SF_THROW(std::logic_error{ "Impossible m_position value in linear_generator" });
        return data;
    }

    const char* linear_generator::name()
    {
        return "linear_generator";
    }

    gain_controller::gain_controller(double attack, double release) :
        m_scale{ 1 },
        m_attack{ 1.0 + attack / BLOCK_SIZE },
        m_release{ 1.0 + release / BLOCK_SIZE },
        m_arg_attack{ attack },
        m_arg_release{ release }
    {}

    gain_controller::gain_controller(double scale, double attack, double release) :
        m_scale{ scale },
        m_attack{ 1.0 + attack / BLOCK_SIZE },
        m_release{ 1.0 + release / BLOCK_SIZE },
        m_arg_attack{ attack },
        m_arg_release{ release }
    {}

    signal_base* gain_controller::copy()
    {
        SF_MARK_STACK;
        return new gain_controller{ m_scale, m_arg_attack, m_arg_release };
    }

    double* gain_controller::next()
    {
        SF_MARK_STACK;
        return process_no_skip([&](double* block) {
            if (block)
            {
                for (uint64_t idx{ 0 }; idx < BLOCK_SIZE; ++idx)
                {
                    auto v = block[idx] / m_scale;
                    auto m = std::abs(v);
                    if (m > 0.5)
                    {
                        m_scale *= m_attack;
                    }
                    else
                    {
                        m_scale /= m_release;
                    }
                    if (v < -1.0)
                    {
                        v = -1.0;
                    }
                    else if (v > 1.0)
                    {
                        v = 1.0;
                    }
                    block[idx] = v;
                }
            }
            return block;
            }, input().next());
    }

    const char* gain_controller::name()
    {
        return "gain_controller";
    }

    repeater::repeater(uint64_t count, std::vector<signal>& chain) : m_chain{}
    {
        m_chain = chain;
        while (count > 1)
        {
            --count;
            for (auto& sig : chain)
            {
                auto nsig = sig.copy();
                add_to_scope(nsig);
                m_chain.emplace_back(nsig);
            }
        }
        signal* prev = nullptr;
        for (auto& sig : m_chain)
        {
            if (prev)
            {
                (*prev) >> sig;
            }
            prev = &sig;
        }
    }

    void repeater::inject(signal& in)
    {
        signal_mono_base::inject(in);
        in >> m_chain.front();
    }

    double* repeater::next()
    {
        SF_MESG_STACK("repeater::next");
        return m_chain.back().next();
    }

    const char* repeater::name()
    {
        return "repeater";
    }

    signal_base* repeater::copy()
    {
        SF_MARK_STACK;
        std::vector<signal> new_chain{};
        return new repeater{ 1, new_chain };
    }

    mixer::mixer(mixer_type mode) : m_mode{ mode } {}

    double* mixer::mix_with()
    {
        SF_MARK_STACK;
        auto cnt = input_count();
        if (cnt == 0)
            SF_THROW(std::logic_error{ "Cannot use a mixer with no inputs" });
        auto into = input().next();
        if (!into)
        {
            for (decltype(cnt)idx{ 1 }; idx < cnt; ++idx)
            {
                if (input(idx).next()) SF_THROW(std::logic_error{ "Not all mixing inputs same length" });
            }
            return nullptr;
        }
        if (into == empty_block())into = new_block();
        for (decltype(cnt)idx{ 1 }; idx < cnt; ++idx)
        {
            auto from = input(idx).next();
            if (from == empty_block()) continue;
            if (!from)
            {
                switch (m_mode)
                {
                case mixer_type::OVERLAY:
                    from = new_block();
                    break;
                default:
                    SF_THROW(std::logic_error{ "Not all mixing inputs same length" });
                }
            }
            switch (m_mode)
            {
            case mixer_type::ADD:
            case mixer_type::OVERLAY:
                for (uint64_t jdx{ 0 }; jdx < BLOCK_SIZE; ++jdx)
                    into[jdx] += from[jdx];
                break;
            case mixer_type::MULTIPLY:
                for (uint64_t jdx{ 0 }; jdx < BLOCK_SIZE; ++jdx)
                    into[jdx] *= from[jdx];
                break;
            default:
                SF_THROW(std::invalid_argument{ "Invalid mixer type: " + std::to_string(uint64_t(m_mode)) });
            }
            free_block(from);
        }
        return into;
    }

    double* mixer::mix_append()
    {
        SF_MARK_STACK;
        auto into = input().next();
        if (!into)
        {
            m_inputs.erase(m_inputs.begin());
            if (!m_inputs.size()) return nullptr;
            into = input().next();
        }
        return into;
    }

    double* mixer::next()
    {
        SF_MESG_STACK("mixer::next");
        switch (m_mode)
        {
        case mixer_type::ADD:
        case mixer_type::MULTIPLY:
        case mixer_type::OVERLAY:
            return mix_with();
        case mixer_type::APPEND:
            return mix_append();
        default:
            SF_THROW(std::invalid_argument{ "Invalid mixer type: " + std::to_string(uint64_t(m_mode)) });
        }
    }

    const char* mixer::name()
    {
        return "mixer";
    }

    seeder::seeder(double pitch, double amplitude, double phase) :
        m_pitch{ pitch },
        m_amplitude{ amplitude },
        m_phase{ phase },
        m_position{ uint64_t(SAMPLES_PER_SECOND * phase) }
    {}

    double* seeder::next()
    {
        SF_MARK_STACK;
        return process_no_skip([&](double* block) {
            if (block)
            {
                double rate = 2 * PI * m_pitch / SAMPLES_PER_SECOND;
                for (uint64_t idx{ 0 }; idx < BLOCK_SIZE; ++idx)
                {
                    block[idx] += fast_cos(m_position++ * rate) * m_amplitude;
                }
            }
            return block;
            }, input().next());
    }

    const char* seeder::name()
    {
        return "seeder";
    }

    signal_base* seeder::copy()
    {
        SF_MARK_STACK;
        return new seeder{ m_pitch, m_amplitude, m_phase };
    };

    power::power(double factor) :
        m_factor{ factor }
    {}

    double* power::next()
    {
        SF_MARK_STACK;
        return process([&](double* block) {
            if (block)
            {
                for (uint64_t idx{ 0 }; idx < BLOCK_SIZE; ++idx)
                {
                    auto v = block[idx];
                    if (v < 0.0)
                        block[idx] = -std::pow(-v, m_factor);
                    else
                        block[idx] = std::pow(v, m_factor);
                }
            }
            return block;
            }, input().next());
    }

    const char* power::name()
    {
        return "power";
    }

    signal_base* power::copy()
    {
        SF_MARK_STACK;
        return new power{ m_factor };
    };

    saturater::saturater(double factor) :
        m_factor{ factor }
    {}

    double* saturater::next()
    {
        SF_MARK_STACK;
        return process([&](double* block) {
            if (block)
            {
                for (uint64_t idx{ 0 }; idx < BLOCK_SIZE; ++idx)
                {
                    auto v = block[idx];
                    if (v < 0.0)
                        block[idx] = v / (m_factor - v);
                    else
                        block[idx] = v / (v + m_factor);
                }
            }
            return block;
            }, input().next());
    }

    const char* saturater::name()
    {
        return "saturate";
    }

    signal_base* saturater::copy()
    {
        SF_MARK_STACK;
        return new saturater{ m_factor };
    };

    amplifier::amplifier(double factor) :
        m_factor{ factor }
    {}

    double* amplifier::next()
    {
        SF_MARK_STACK;
        return process([&](double* block) {
            if (block)
            {
                for (uint64_t idx{ 0 }; idx < BLOCK_SIZE; ++idx)
                {
                    block[idx] *= m_factor;
                }
            }
            return block;
            }, input().next());
    }

    const char* amplifier::name()
    {
        return "amplifier";
    }

    signal_base* amplifier::copy()
    {
        SF_MARK_STACK;
        return new amplifier{ m_factor };
    };

    wrapper::wrapper(signal front, signal back) : m_front{ front }, m_back{ back }{}

    void wrapper::inject(signal& in)
    {
        in >> m_front;
    }

    double* wrapper::next()
    {
        SF_MARK_STACK;
        return m_back.next();
    }

    const char* wrapper::name()
    {
        return "wrapper";
    }

    signal_base* wrapper::copy()
    {
        SF_MARK_STACK;
        return new wrapper{add_to_scope(m_front.copy()), add_to_scope(m_back.copy())};
    }

    cutter::cutter(uint64_t pad_before, uint64_t from, uint64_t to, uint64_t pad_after):
        m_pad_before{pad_before},
        m_from{ from },
        m_to{ to },
        m_pad_after{ pad_after },
        m_position{ 0 },
        m_done{false}
    {}

    double* cutter::next()
    {
        SF_MESG_STACK("cutter::next");
        if (m_pad_before)
        {
            --m_pad_before;
            return empty_block();
        }
        while (m_position < m_from)
        {
            auto block = input().next();
            if (block != empty_block()) free_block(block);
            ++m_position;
        }
        if (!m_done && m_position >= m_to)
        {
            while (auto block = input().next())
            {
                if (block != empty_block()) free_block(block);
            }
            m_done = true;
        }
        if (m_done && m_pad_after)
        {
            --m_pad_after;
            return empty_block();
        }
        if (m_done) return nullptr;

        auto block = input().next();
        ++m_position;
        if (!block)
        {
            block = empty_block();
            m_done = true;
        }
        return block;
    }

    const char* cutter::name()
    {
        return "cutter";
    }

    signal_base* cutter::copy()
    {
        SF_MARK_STACK;
        return new cutter{ m_pad_before, m_from, m_to, m_pad_after };
    }

    sweeper::sweeper(double start_frequency, double end_frequency, uint64_t length):
        m_start_frequency{ start_frequency },
        m_end_frequency{ end_frequency },
        m_length{ length },
        m_position{ 0 }
    {}

    double* sweeper::next()
    {
        uint64_t length = m_length * BLOCK_SIZE;
        if (m_position > length) return nullptr;
        double* data = new_block();
        double corrected_end = m_start_frequency + (m_end_frequency - m_start_frequency) / 2.0;
        for (uint64_t idx{ 0 }; idx < BLOCK_SIZE; ++idx)
        {
            double ratio = double(length - m_position) / double(length);
            double f = m_start_frequency * ratio + corrected_end * (1.0-ratio);
            data[idx] = sin(f * m_position * ANGLE_RATE);
            ++m_position;
        }
        return data;
    }

    const char* sweeper::name()
    {
        return "sweeper";
    }

    signal_base* sweeper::copy()
    {
        return new sweeper{ m_start_frequency, m_end_frequency, m_length };
    };

    shepard::shepard(double start_frequency, double end_frequency, uint64_t cycle_length, uint64_t length):
        m_start_frequency{ start_frequency },
        m_end_frequency{ end_frequency },
        m_length{ length },
        m_cycle_length{ cycle_length }
    {
        for(double p{start_frequency}; p < start_frequency * 64; p=p*2)
        {
            m_pitches.push_back(p);
        }
        double nsamples = m_cycle_length * SAMPLES_PER_SECOND * 1000;
        m_step = pow(2, 1.0/nsamples);
        if (m_start_frequency > m_end_frequency)
            m_step = 1.0 / m_step;
    }

    double* shepard::next()
    {
        if (m_length == 0)
            return nullptr;
        auto data = new_block();
        auto start = m_length * BLOCK_SIZE;
        for(uint64_t i{0}; i<BLOCK_SIZE; ++i)
        {
            double datum{0};
            double harmonic_multiplier{1};
            for(uint64_t j{0}; j<m_pitches.size(); ++j)
            {
                auto p = m_pitches[j];
                auto val = p * (start - j) * ANGLE_RATE;
                auto p_start = m_start_frequency * harmonic_multiplier;
                auto p_end = m_end_frequency * harmonic_multiplier;
                auto p_diff = std::abs(p_start * p_end);
                auto p_pos = std::fmin(std::abs((p_start - p)), std::abs(p_end - p));
                auto p_ratio = (p_pos / p_diff) * 2;
                // Follow a porabola from 0 to 1 and back.
                auto p_vol = p_ratio * p_ratio;
                val *= p_vol;
                datum += val;
                if (p_start > p_end)
                {
                    if (p <= p_end)
                        p = p_start;
                    else
                        p *= m_step;
                }
                else
                {
                    if (p >= p_end)
                        p = p_start;
                    else
                        p *= m_step;
                }
                m_pitches[j] = p;
            }
        }
        --m_length;
        return data;
    }

    const char* shepard::name()
    {
        return "shepard";
    }

    signal_base* shepard::copy()
    {
        return new shepard{ m_start_frequency, m_end_frequency, m_cycle_length, m_length };
    };
} // sonic_field
