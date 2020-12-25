#include "sonic_field.h"

namespace sonic_field
{

    double rbj_filter::filter(double in0)
    {
        // filter
        double yn = b0a0 * in0 + b1a0 * in1 + b2a0 * in2 - a1a0 * ou1 - a2a0 * ou2;

        // push in/out buffers
        in2 = in1;
        in1 = in0;
        ou2 = ou1;
        ou1 = yn;

        // return output
        return yn;
    }

    double* rbj_filter::next()
    {
        SF_MESG_STACK("rbj_filter::next");
        return process_no_skip([&](double* block) {
            if (block)
            {
                for (uint64_t idx{ 0 }; idx < BLOCK_SIZE; ++idx)
                {
                    block[idx] = filter(block[idx]);
                };
            }
            return block;
            }, input().next());
    }

    const char* rbj_filter::name()
    {
        return "rbj_filter";
    }

    rbj_filter::rbj_filter(filter_type type, double frequency, double q, double db_gain)
    {
        // reset filter coeffs
        b0a0 = b1a0 = b2a0 = a1a0 = a2a0 = 0.0;

        // reset in/out history
        ou1 = ou2 = in1 = in2 = 0.0f;

        bool q_is_bandwidth;
        double sample_rate = SAMPLES_PER_SECOND;
        switch (type)
        {
        case filter_type::ALLPASS:
        case filter_type::HIGHPASS:
        case filter_type::LOWPASS:
        case filter_type::LOWSHELF:
        case filter_type::HIGHSHELF:
            q_is_bandwidth = false;
            break;
        default:
            q_is_bandwidth = true;
            break;
        }
        // System.out.println("Q Is Bandwidth " + q_is_bandwidth);
        // temp pi

        // temp coef vars
        double alpha, a0 = 0, a1 = 0, a2 = 0, b0 = 0, b1 = 0, b2 = 0;

        // peaking, lowshelf and hishelf
        if (type == filter_type::PEAK || type == filter_type::HIGHSHELF || type == filter_type::LOWSHELF)
        {
            double A = pow(10.0, (db_gain / 40.0));
            double omega = 2.0 * PI * frequency / sample_rate;
            double tsin = sin(omega);
            double tcos = cos(omega);
            if (type == filter_type::PEAK) alpha = tsin * sinh(log(2.0) / 2.0 * q * omega / tsin);
            else
                alpha = tsin / 2.0 * sqrt((A + 1 / A) * (1 / q - 1) + 2);

            double beta = sqrt(A) / q;

            // peaking
            if (type == filter_type::PEAK)
            {
                b0 = (1.0 + alpha * A);
                b1 = (-2.0 * tcos);
                b2 = (1.0 - alpha * A);
                a0 = (1.0 + alpha / A);
                a1 = (-2.0 * tcos);
                a2 = (1.0 - alpha / A);
            }

            // lowshelf
            if (type == filter_type::LOWSHELF)
            {
                b0 = (A * ((A + 1.0) - (A - 1.0) * tcos + beta * tsin));
                b1 = (2.0 * A * ((A - 1.0) - (A + 1.0) * tcos));
                b2 = (A * ((A + 1.0) - (A - 1.0) * tcos - beta * tsin));
                a0 = ((A + 1.0) + (A - 1.0) * tcos + beta * tsin);
                a1 = (-2.0 * ((A - 1.0) + (A + 1.0) * tcos));
                a2 = ((A + 1.0) + (A - 1.0) * tcos - beta * tsin);
            }

            // hishelf
            if (type == filter_type::HIGHSHELF)
            {
                b0 = (A * ((A + 1.0) + (A - 1.0) * tcos + beta * tsin));
                b1 = (-2.0 * A * ((A - 1.0) + (A + 1.0) * tcos));
                b2 = (A * ((A + 1.0) + (A - 1.0) * tcos - beta * tsin));
                a0 = ((A + 1.0) - (A - 1.0) * tcos + beta * tsin);
                a1 = (2.0 * ((A - 1.0) - (A + 1.0) * tcos));
                a2 = ((A + 1.0) - (A - 1.0) * tcos - beta * tsin);
            }
        }
        else
        {
            // other filters
            double omega = 2.0 * PI * frequency / sample_rate;
            double tsin = sin(omega);
            double tcos = cos(omega);

            if (q_is_bandwidth) alpha = tsin * sinh(log(2.0) / 2.0 * q * omega / tsin);
            else
                alpha = tsin / (2.0 * q);

            // lowpass
            if (type == filter_type::LOWPASS)
            {
                b0 = (1.0 - tcos) / 2.0;
                b1 = 1.0 - tcos;
                b2 = (1.0 - tcos) / 2.0;
                a0 = 1.0 + alpha;
                a1 = -2.0 * tcos;
                a2 = 1.0 - alpha;
            }

            // hipass
            if (type == filter_type::HIGHPASS)
            {
                b0 = (1.0 + tcos) / 2.0;
                b1 = -(1.0 + tcos);
                b2 = (1.0 + tcos) / 2.0;
                a0 = 1.0 + alpha;
                a1 = -2.0 * tcos;
                a2 = 1.0 - alpha;
            }

            // bandpass csg
            if (type == filter_type::BANDPASS_SKIRT)
            {
                b0 = tsin / 2.0;
                b1 = 0.0;
                b2 = -tsin / 2;
                a0 = 1.0 + alpha;
                a1 = -2.0 * tcos;
                a2 = 1.0 - alpha;
            }

            // bandpass czpg
            if (type == filter_type::BANDPASS_PEAK)
            {
                b0 = alpha;
                b1 = 0.0;
                b2 = -alpha;
                a0 = 1.0 + alpha;
                a1 = -2.0 * tcos;
                a2 = 1.0 - alpha;
            }

            // notch
            if (type == filter_type::NOTCH)
            {
                b0 = 1.0;
                b1 = -2.0 * tcos;
                b2 = 1.0;
                a0 = 1.0 + alpha;
                a1 = -2.0 * tcos;
                a2 = 1.0 - alpha;
            }

            // allpass
            if (type == filter_type::ALLPASS)
            {
                b0 = 1.0 - alpha;
                b1 = -2.0 * tcos;
                b2 = 1.0 + alpha;
                a0 = 1.0 + alpha;
                a1 = -2.0 * tcos;
                a2 = 1.0 - alpha;
            }
        }

        // set filter coeffs
        b0a0 = (b0 / a0);
        b1a0 = (b1 / a0);
        b2a0 = (b2 / a0);
        a1a0 = (a1 / a0);
        a2a0 = (a2 / a0);
    }

    rbj_filter::rbj_filter(double b0a0, double  b1a0, double  b2a0, double  a1a0, double a2a0):
        b0a0{ b0a0 },
        b1a0{ b1a0 },
        b2a0{ b2a0 },
        a1a0{ a1a0 },
        a2a0{ a2a0 },
        ou1{ 0 },
        ou2{ 0 },
        in1{ 0 },
        in2{ 0 }
    {}

    signal_base* rbj_filter::copy()
    {
        return new rbj_filter{ b0a0, b1a0, b2a0, a1a0, a2a0 };
    }


    decimator::decimator()
    {
        h0 = (8192 / 16384.0);
        h1 = (5042 / 16384.0);
        h3 = (-1277 / 16384.0);
        h5 = (429 / 16384.0);
        h7 = (-116 / 16384.0);
        h9 = (18 / 16384.0);
        R1 = R2 = R3 = R4 = R5 = R6 = R7 = R8 = R9 = 0.0;
    }

    double decimator::decimate(double d,double e)
    {
        double h9x0 = h9 * d;
        double h7x0 = h7 * d;
        double h5x0 = h5 * d;
        double h3x0 = h3 * d;
        double h1x0 = h1 * d;
        double R10 = R9 + h9x0;
        R9 = R8 + h7x0;
        R8 = R7 + h5x0;
        R7 = R6 + h3x0;
        R6 = R5 + h1x0;
        R5 = R4 + h1x0 + h0 * e;
        R4 = R3 + h3x0;
        R3 = R2 + h5x0;
        R2 = R1 + h7x0;
        R1 = h9x0;
        return R10;
    }

    // I no longer can work out where this came from originally.
    // The concept of shaping it is my own, the rest is other people's work.
    // I belive this will be OK to release under GLP, if the original was less stringent then
    // feel free to copy that and use it instead. This was translated by me from C to Java then
    // to C++.
    // PS I think it might have come from http://www.musicdsp.org/showone.php?id=24 wich is now
    // defunct.
    class shaped_ladder
    {
        class inner_filter
        {
            double cutoff;
            double res;
            double fs;
            double y1, y2, y3, y4;
            double oldx;
            double oldy1, oldy2, oldy3;
            double x;
            double r;
            double p;
            double k;
        public:
            inner_filter()
            {
                fs = SAMPLES_PER_SECOND;
                init();
            }

            void init()
            {
                // initialize values
                y1 = y2 = y3 = y4 = oldx = oldy1 = oldy2 = oldy3 = 0;
                calc();
            }

            void calc()
            {
                double f = (cutoff + cutoff) / fs; // [0 - 1]
                p = f * (1.8f - 0.8f * f);
                k = p + p - 1.f;

                double t = (1.f - p) * 1.386249f;
                double t2 = 12.f + t * t;
                r = res * (t2 + 6.f * t) / (t2 - 6.f * t);
            }

            double process(double input)
            {
                // process input
                x = input - r * y4;

                // Four cascaded onepole filters (bilinear transform)
                y1 = x * p + oldx * p - k * y1;
                y2 = y1 * p + oldy1 * p - k * y2;
                y3 = y2 * p + oldy2 * p - k * y3;
                y4 = y3 * p + oldy3 * p - k * y4;

                // Clipper band limited sigmoid
                y4 -= (y4 * y4 * y4) / 6.f;

                oldx = x;
                oldy1 = y1;
                oldy2 = y2;
                oldy3 = y3;
                return y4;
            }

            void set_cutoff(double c)
            {
                // Only recalculate when the change is bigger than
                // one cent
                double cc = c / cutoff;
                if (cc < 1.0) cc = 1.0 / cc;
                if (cc > 1.0005777895)
                {
                    // System.out.println("Recomputing c " + c);
                    cutoff = c;
                    calc();
                }
            }

            void set_res(double r1)
            {
                // Only recalculate when the change is bigger than
                // one cent
                double rr = r1 / cutoff;
                if (rr < 1.0) rr = 1.0 / r1;
                if (rr > 1.0005777895)
                {
                    res = r1;
                    calc();
                }
            }
        };

        inner_filter filter{};

    public:

        double* process(double* input, double* resonance, double* cutoff)
        {
            SF_MARK_STACK;
            double* out = new_block(false);
            for (uint64_t idx = 0; idx < BLOCK_SIZE; ++idx)
            {
                filter.set_res(resonance[idx]);
                filter.set_cutoff(cutoff[idx]);
                out[idx] = filter.process(input[idx]);
            }
            free_block(input);
            free_block(resonance);
            free_block(cutoff);
            return out;
        }
    };

    ladder_filter_driver::ladder_filter_driver()
    {
        m_ladder = new shaped_ladder{};
    }

    ladder_filter_driver::~ladder_filter_driver()
    {
        delete m_ladder;
    }

    double* ladder_filter_driver::next()
    {
        if (input_count() != 3)
            SF_THROW(std::invalid_argument{ "Ladder filter requires three inputs (signal, resonance, cutoff)" });
        double* signal = input(0).next();
        double* resonance = input(1).next();
        double* cutoff = input(2).next();
        if (signal == nullptr && resonance == nullptr && cutoff == nullptr)
            return nullptr;
        if (signal == nullptr || resonance == nullptr || cutoff == nullptr)
            SF_THROW(std::invalid_argument{ "Inputs to ladder filter do not have the same length" });
        return m_ladder->process(signal, resonance, cutoff);
    }

    const char* ladder_filter_driver::name()
    {
        return "ladder_filter_driver";
    }

    signal_base* ladder_filter_driver::copy()
    {
        return new ladder_filter_driver{};
    }

} // sonic_field