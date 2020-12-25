#include "sonic_field.h"

namespace sonic_field
{

    fft::fft(uint64_t n, bool isForward) : m_n{ n }, m_m{ uint64_t(log(n) / log(2)) }
    {

        if (m_n != (1ull << m_m)) SF_THROW(std::logic_error{ "fft must be power of 2" });

        m_cos = new double[m_n >> 1];
        m_sin = new double[m_n >> 1];
        double dir = isForward ? -2 * PI : 2 * PI;

        for (uint64_t i = 0; i < m_n >> 1; ++i)
        {
            m_cos[i] = cos(dir * i / m_n);
            m_sin[i] = sin(dir * i / m_n);
        }

    }

    fft::~fft()
    {
        delete[] m_sin;
        delete[] m_cos;
    }

    void fft::compute(double* x, double* y)
    {
        uint64_t i, j, k, n1, n2, a;
        double c, s, t1, t2;

        // Bit-reverse
        j = 0;
        n2 = m_n >> 1;
        for (uint64_t i = 1; i < m_n - 1; ++i)
        {
            n1 = n2;
            while (j >= n1)
            {
                j = j - n1;
                n1 >>= 1;
            }
            j = j + n1;

            if (i < j)
            {
                t1 = x[i];
                x[i] = x[j];
                x[j] = t1;
                t1 = y[i];
                y[i] = y[j];
                y[j] = t1;
            }
        }

        n1 = 0;
        n2 = 1;

        for (i = 0; i < m_m; ++i)
        {
            n1 = n2;
            n2 <<= 1;
            a = 0;

            for (j = 0; j < n1; j++)
            {
                c = m_cos[a];
                s = m_sin[a];
                a += 1ll << (m_m - i - 1);
                for (k = j; k < m_n; k += n2)
                {
                    uint64_t kn1 = k + n1;
                    t1 = c * x[kn1] - s * y[kn1];
                    t2 = s * x[kn1] + c * y[kn1];
                    x[kn1] = x[k] - t1;
                    y[kn1] = y[k] - t2;
                    x[k] = x[k] + t1;
                    y[k] = y[k] + t2;
                }
            }
        }
    }
    } // sonic_field

    //    Copyright (c) 2010 Martin Eastwood
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
    //  You should have received a copy of the GNU General Public License
    //  along with this MVerb.  If not, see <http://www.gnu.org/licenses/>.

    namespace mverb
    {
    //forward declaration
    template<typename T, uint64_t max_length> class all_pass_filter;
    template<typename T, uint64_t max_length> class static_all_pass_four_tap;
    template<typename T, uint64_t max_length> class static_delayline;
    template<typename T, uint64_t max_length> class static_delay_line_four_tap;
    template<typename T, uint64_t max_length> class static_delay_line_eight_tap;
    template<typename T, uint64_t OverSampleCount> class StateVariable;

    constexpr double SAMPLE_RATE = double(sonic_field::SAMPLES_PER_SECOND);

    template<typename T>
    class MVerb
    {
    private:
        all_pass_filter<T, 320000> all_pass[4];
        static_all_pass_four_tap<T, 320000> all_passFourTap[4];
        StateVariable<T, 4> bandwidthFilter[2];
        StateVariable<T, 4> damping[2];
        static_delayline<T, 320000> predelay;
        static_delay_line_four_tap<T, 320000> staticDelayLine[4];
        static_delay_line_eight_tap<T, 320000> earlyReflectionsDelayLine[2];
        T SampleRate, DampingFreq, Density1, Density2, BandwidthFreq, PreDelayTime, Decay, Gain, Mix, EarlyMix, Size;
        T MixSmooth, EarlyLateSmooth, BandwidthSmooth, DampingSmooth, PredelaySmooth, SizeSmooth, DensitySmooth, DecaySmooth;
        T PreviousLeftTank, PreviousRightTank;
        uint64_t ControlRate, ControlRateCounter;

    public:
        enum
        {
            DAMPINGFREQ = 0,
            DENSITY,
            BANDWIDTHFREQ,
            DECAY,
            PREDELAY,
            SIZE,
            GAIN,
            MIX,
            EARLYMIX,
            NUM_PARAMS
        };

        MVerb() {
            DampingFreq = 0.9;
            BandwidthFreq = 0.9;
            SampleRate = SAMPLE_RATE;
            Decay = 0.5;
            Gain = 1.;
            Mix = 1.;
            Size = 1.;
            EarlyMix = 1.;
            PreviousLeftTank = 0.;
            PreviousRightTank = 0.;
            PreDelayTime = 100 * (SampleRate / 1000);
            MixSmooth = EarlyLateSmooth = BandwidthSmooth = DampingSmooth = PredelaySmooth = SizeSmooth = DecaySmooth = DensitySmooth = 0.;
            ControlRate = uint64_t(SampleRate / 1000.);
            ControlRateCounter = 0;
            reset();
        }

        ~MVerb() {}

        void process(T** inputs, T** outputs, uint64_t sampleFrames) {
            SF_MARK_STACK;
            T OneOverSampleFrames = 1. / sampleFrames;
            T MixDelta = (Mix - MixSmooth) * OneOverSampleFrames;
            T EarlyLateDelta = (EarlyMix - EarlyLateSmooth) * OneOverSampleFrames;
            T BandwidthDelta = ((BandwidthFreq + 100.) - BandwidthSmooth) * OneOverSampleFrames;
            T DampingDelta = ((DampingFreq + 100.) - DampingSmooth) * OneOverSampleFrames;
            T PredelayDelta = ((PreDelayTime * 200 * (SampleRate / 1000)) - PredelaySmooth) * OneOverSampleFrames;
            T SizeDelta = (Size - SizeSmooth) * OneOverSampleFrames;
            T DecayDelta = (((0.7995f * Decay) + 0.005) - DecaySmooth) * OneOverSampleFrames;
            T DensityDelta = (((0.7995f * Density1) + 0.005) - DensitySmooth) * OneOverSampleFrames;
            for (uint64_t i = 0; i < sampleFrames; ++i) {
                T left = inputs[0][i];
                T right = inputs[1][i];
                MixSmooth += MixDelta;
                EarlyLateSmooth += EarlyLateDelta;
                BandwidthSmooth += BandwidthDelta;
                DampingSmooth += DampingDelta;
                PredelaySmooth += PredelayDelta;
                SizeSmooth += SizeDelta;
                DecaySmooth += DecayDelta;
                DensitySmooth += DensityDelta;
                if (ControlRateCounter >= ControlRate) {
                    ControlRateCounter = 0;
                    bandwidthFilter[0].Frequency(BandwidthSmooth);
                    bandwidthFilter[1].Frequency(BandwidthSmooth);
                    damping[0].Frequency(DampingSmooth);
                    damping[1].Frequency(DampingSmooth);
                }
                ++ControlRateCounter;
                predelay.set_length(uint64_t(PredelaySmooth));
                Density2 = DecaySmooth + 0.15;
                if (Density2 > 0.5)
                    Density2 = 0.5;
                if (Density2 < 0.25)
                    Density2 = 0.25;
                all_passFourTap[1].set_feedback(Density2);
                all_passFourTap[3].set_feedback(Density2);
                all_passFourTap[0].set_feedback(Density1);
                all_passFourTap[2].set_feedback(Density1);
                T bandwidthLeft = bandwidthFilter[0](left);
                T bandwidthRight = bandwidthFilter[1](right);
                T earlyReflectionsL = earlyReflectionsDelayLine[0](bandwidthLeft * 0.5 + bandwidthRight * 0.3)
                    + earlyReflectionsDelayLine[0].get_index(2) * 0.6
                    + earlyReflectionsDelayLine[0].get_index(3) * 0.4
                    + earlyReflectionsDelayLine[0].get_index(4) * 0.3
                    + earlyReflectionsDelayLine[0].get_index(5) * 0.3
                    + earlyReflectionsDelayLine[0].get_index(6) * 0.1
                    + earlyReflectionsDelayLine[0].get_index(7) * 0.1
                    + (bandwidthLeft * 0.4 + bandwidthRight * 0.2) * 0.5;
                T earlyReflectionsR = earlyReflectionsDelayLine[1](bandwidthLeft * 0.3 + bandwidthRight * 0.5)
                    + earlyReflectionsDelayLine[1].get_index(2) * 0.6
                    + earlyReflectionsDelayLine[1].get_index(3) * 0.4
                    + earlyReflectionsDelayLine[1].get_index(4) * 0.3
                    + earlyReflectionsDelayLine[1].get_index(5) * 0.3
                    + earlyReflectionsDelayLine[1].get_index(6) * 0.1
                    + earlyReflectionsDelayLine[1].get_index(7) * 0.1
                    + (bandwidthLeft * 0.2 + bandwidthRight * 0.4) * 0.5;
                T predelayMonoInput = predelay((bandwidthRight + bandwidthLeft) * 0.5f);
                //std::cout << "MONTY " << predelayMonoInput << std::endl;
                T smearedInput = predelayMonoInput;
                for (uint64_t j = 0; j < 4; j++)
                    smearedInput = all_pass[j](smearedInput);
                //std::cout << "WILLOW " << smearedInput << std::endl;
                T leftTank = all_passFourTap[0](smearedInput + PreviousRightTank);
                leftTank = staticDelayLine[0](leftTank);
                leftTank = damping[0](leftTank);
                leftTank = all_passFourTap[1](leftTank);
                leftTank = staticDelayLine[1](leftTank);
                T rightTank = all_passFourTap[2](smearedInput + PreviousLeftTank);
                rightTank = staticDelayLine[2](rightTank);
                rightTank = damping[1](rightTank);
                rightTank = all_passFourTap[3](rightTank);
                rightTank = staticDelayLine[3](rightTank);
                PreviousLeftTank = leftTank * DecaySmooth;
                PreviousRightTank = rightTank * DecaySmooth;
                T accumulatorL = (0.6 * staticDelayLine[2].get_index(1))
                    + (0.6 * staticDelayLine[2].get_index(2))
                    - (0.6 * all_passFourTap[3].get_index(1))
                    + (0.6 * staticDelayLine[3].get_index(1))
                    - (0.6 * staticDelayLine[0].get_index(1))
                    - (0.6 * all_passFourTap[1].get_index(1))
                    - (0.6 * staticDelayLine[1].get_index(1));
                T accumulatorR = (0.6 * staticDelayLine[0].get_index(2))
                    + (0.6 * staticDelayLine[0].get_index(3))
                    - (0.6 * all_passFourTap[1].get_index(2))
                    + (0.6 * staticDelayLine[1].get_index(2))
                    - (0.6 * staticDelayLine[2].get_index(3))
                    - (0.6 * all_passFourTap[3].get_index(2))
                    - (0.6 * staticDelayLine[3].get_index(2));
                accumulatorL = ((accumulatorL * EarlyMix) + ((1 - EarlyMix) * earlyReflectionsL));
                accumulatorR = ((accumulatorR * EarlyMix) + ((1 - EarlyMix) * earlyReflectionsR));
                left = (left + MixSmooth * (accumulatorL - left)) * Gain;
                right = (right + MixSmooth * (accumulatorR - right)) * Gain;
                if (!(std::isfinite(left) && std::isfinite(right)))
                {
                    SF_THROW(std::overflow_error{"Overflow or NaN in reverberator. left: " + std::to_string(left) + " right: " + std::to_string(right)});
                }
                //std::cout << "Left: " << left << " MixSmooth: " << MixSmooth << " accumulatorL: " << accumulatorL << std::endl;
                outputs[0][i] = left;
                outputs[1][i] = right;
            }
        }

        void reset() {
            SF_MARK_STACK;
            ControlRateCounter = 0;
            bandwidthFilter[0].SetSampleRate(SampleRate);
            bandwidthFilter[1].SetSampleRate(SampleRate);
            bandwidthFilter[0].Reset();
            bandwidthFilter[1].Reset();
            damping[0].SetSampleRate(SampleRate);
            damping[1].SetSampleRate(SampleRate);
            damping[0].Reset();
            damping[1].Reset();
            predelay.clear();
            predelay.set_length(uint64_t(PreDelayTime));
            all_pass[0].clear();
            all_pass[1].clear();
            all_pass[2].clear();
            all_pass[3].clear();
            all_pass[0].set_length(uint64_t(0.0048 * SampleRate));
            all_pass[1].set_length(uint64_t(0.0036 * SampleRate));
            all_pass[2].set_length(uint64_t(0.0127 * SampleRate));
            all_pass[3].set_length(uint64_t(0.0093 * SampleRate));
            all_pass[0].set_feedback(0.75);
            all_pass[1].set_feedback(0.75);
            all_pass[2].set_feedback(0.625);
            all_pass[3].set_feedback(0.625);
            all_passFourTap[0].clear();
            all_passFourTap[1].clear();
            all_passFourTap[2].clear();
            all_passFourTap[3].clear();
            all_passFourTap[0].set_length(uint64_t(0.020 * SampleRate * Size));
            all_passFourTap[1].set_length(uint64_t(0.060 * SampleRate * Size));
            all_passFourTap[2].set_length(uint64_t(0.030 * SampleRate * Size));
            all_passFourTap[3].set_length(uint64_t(0.089 * SampleRate * Size));
            all_passFourTap[0].set_feedback(Density1);
            all_passFourTap[1].set_feedback(Density2);
            all_passFourTap[2].set_feedback(Density1);
            all_passFourTap[3].set_feedback(Density2);
            all_passFourTap[0].set_index(0, 0, 0, 0);
            all_passFourTap[1].set_index(0, uint64_t(0.006 * SampleRate * Size), uint64_t(0.041 * SampleRate * Size), 0);
            all_passFourTap[2].set_index(0, 0, 0, 0);
            all_passFourTap[3].set_index(0, uint64_t(0.031 * SampleRate * Size), uint64_t(0.011 * SampleRate * Size), 0);
            staticDelayLine[0].clear();
            staticDelayLine[1].clear();
            staticDelayLine[2].clear();
            staticDelayLine[3].clear();
            staticDelayLine[0].set_length(uint64_t(0.15 * SampleRate * Size));
            staticDelayLine[1].set_length(uint64_t(0.12 * SampleRate * Size));
            staticDelayLine[2].set_length(uint64_t(0.14 * SampleRate * Size));
            staticDelayLine[3].set_length(uint64_t(0.11 * SampleRate * Size));
            staticDelayLine[0].set_index(0, uint64_t(0.067 * SampleRate * Size), uint64_t(0.011 * SampleRate * Size), uint64_t(0.121 * SampleRate * Size));
            staticDelayLine[1].set_index(0, uint64_t(0.036 * SampleRate * Size), uint64_t(0.089 * SampleRate * Size), 0);
            staticDelayLine[2].set_index(0, uint64_t(0.0089 * SampleRate * Size), uint64_t(0.099 * SampleRate * Size), 0);
            staticDelayLine[3].set_index(0, uint64_t(0.067 * SampleRate * Size), uint64_t(0.0041 * SampleRate * Size), 0);
            earlyReflectionsDelayLine[0].clear();
            earlyReflectionsDelayLine[1].clear();
            earlyReflectionsDelayLine[0].set_length(uint64_t(0.089 * SampleRate));
            earlyReflectionsDelayLine[0].set_index(0, uint64_t(0.0199 * SampleRate), uint64_t(0.0219 * SampleRate), uint64_t(0.0354 * SampleRate), uint64_t(0.0389 * SampleRate), uint64_t(0.0414 * SampleRate), uint64_t(0.0692 * SampleRate), 0);
            earlyReflectionsDelayLine[1].set_length(uint64_t(0.069 * SampleRate));
            earlyReflectionsDelayLine[1].set_index(0, uint64_t(0.0099 * SampleRate), uint64_t(0.011 * SampleRate), uint64_t(0.0182 * SampleRate), uint64_t(0.0189 * SampleRate), uint64_t(0.0213 * SampleRate), uint64_t(0.0431 * SampleRate), 0);
        }

        void setParameter(uint64_t index, T value) {
            SF_MARK_STACK;
            switch (index) {
            case DAMPINGFREQ:
                DampingFreq = value;
                break;
            case DENSITY:
                Density1 = value;
                break;
            case BANDWIDTHFREQ:
                BandwidthFreq = value;
                break;
            case PREDELAY:
                PreDelayTime = value;
                break;
            case SIZE:
                Size = (0.95 * value) + 0.05;
                all_passFourTap[0].clear();
                all_passFourTap[1].clear();
                all_passFourTap[2].clear();
                all_passFourTap[3].clear();
                all_passFourTap[0].set_length(uint64_t(0.020 * SampleRate * Size));
                all_passFourTap[1].set_length(uint64_t(0.060 * SampleRate * Size));
                all_passFourTap[2].set_length(uint64_t(0.030 * SampleRate * Size));
                all_passFourTap[3].set_length(uint64_t(0.089 * SampleRate * Size));
                all_passFourTap[1].set_index(0, uint64_t(0.006 * SampleRate * Size), uint64_t(0.041 * SampleRate * Size), 0);
                all_passFourTap[3].set_index(0, uint64_t(0.031 * SampleRate * Size), uint64_t(0.011 * SampleRate * Size), 0);
                staticDelayLine[0].clear();
                staticDelayLine[1].clear();
                staticDelayLine[2].clear();
                staticDelayLine[3].clear();
                staticDelayLine[0].set_length(uint64_t(0.15 * SampleRate * Size));
                staticDelayLine[1].set_length(uint64_t(0.12 * SampleRate * Size));
                staticDelayLine[2].set_length(uint64_t(0.14 * SampleRate * Size));
                staticDelayLine[3].set_length(uint64_t(0.11 * SampleRate * Size));
                staticDelayLine[0].set_index(0, uint64_t(0.067 * SampleRate * Size), uint64_t(0.011 * SampleRate * Size), uint64_t(0.121 * SampleRate * Size));
                staticDelayLine[1].set_index(0, uint64_t(0.036 * SampleRate * Size), uint64_t(0.089 * SampleRate * Size), 0);
                staticDelayLine[2].set_index(0, uint64_t(0.0089 * SampleRate * Size), uint64_t(0.099 * SampleRate * Size), 0);
                staticDelayLine[3].set_index(0, uint64_t(0.067 * SampleRate * Size), uint64_t(0.0041 * SampleRate * Size), 0);
                break;
            case DECAY:
                Decay = value;
                break;
            case GAIN:
                Gain = value;
                break;
            case MIX:
                Mix = value;
                break;
            case EARLYMIX:
                EarlyMix = value;
                break;
            }
        }

        void setSampleRate(T sr) {
            SampleRate = sr;
            ControlRate = SampleRate / 1000;
            reset();
        }
    };

    template<typename T, uint64_t max_length>
    class all_pass_filter
    {
    private:
        T m_buffer[max_length];
        uint64_t m_index;
        uint64_t m_length;
        T m_feedback;

    public:
        all_pass_filter()
        {
            set_length(max_length - 1);
            clear();
            m_feedback = 0.5;
        }

        T operator()(T input)
        {
            T output;
            T bufout;
            bufout = m_buffer[m_index];
            T temp = input * - m_feedback;
            output = bufout + temp;
            m_buffer[m_index] = input + ((bufout + temp) * m_feedback);
            if (++m_index >= m_length) m_index = 0;
            return output;

        }

        void set_length(uint64_t length)
        {
            if (length >= max_length) SF_THROW(std::invalid_argument("length of delay too long: " + std::to_string(length * 1000 / SAMPLE_RATE)));
            m_length = length;
        }

        void set_feedback(T feedback)
        {
            m_feedback = feedback;
        }

        void clear()
        {
            memset(m_buffer, 0, sizeof(m_buffer));
            m_index = 0;
        }
    };

    template<typename T, uint64_t max_length>
    class static_all_pass_four_tap
    {
    private:
        T m_buffer[max_length];
        uint64_t m_index1, m_index2, m_index3, m_index4;
        uint64_t m_length;
        T m_feedback;

    public:
        static_all_pass_four_tap()
        {
            set_length(max_length - 1);
            clear();
            m_feedback = 0.5;
        }

        T operator()(T input)
        {
            T output;
            T bufout;

            bufout = m_buffer[m_index1];
            T temp = input * - m_feedback;
            output = bufout + temp;
            m_buffer[m_index1] = input + ((bufout + temp) * m_feedback);

            if (++m_index1 >= m_length)
                m_index1 = 0;
            if (++m_index2 >= m_length)
                m_index2 = 0;
            if (++m_index3 >= m_length)
                m_index3 = 0;
            if (++m_index4 >= m_length)
                m_index4 = 0;

            return output;
        }

        void set_index(uint64_t index1, uint64_t index2, uint64_t index3, uint64_t index4)
        {
            m_index1 = index1;
            m_index2 = index2;
            m_index3 = index3;
            m_index4 = index4;
        }

        T get_index(uint64_t index)
        {
            switch (index)
            {
            case 0:
                return m_buffer[m_index1];
                break;
            case 1:
                return m_buffer[m_index2];
                break;
            case 2:
                return m_buffer[m_index3];
                break;
            case 3:
                return m_buffer[m_index4];
                break;
            default:
                return m_buffer[m_index1];
                break;
            }
        }

        void set_length(uint64_t length)
        {
            if (length >= max_length) SF_THROW(std::invalid_argument("length of delay too long: " + std::to_string(length * 1000 / SAMPLE_RATE)));
            m_length = length;
        }


        void clear()
        {
            memset(m_buffer, 0, sizeof(m_buffer));
            m_index1 = m_index2 = m_index3 = m_index4 = 0;
        }

        void set_feedback(T feedback)
        {
            m_feedback = feedback;
        }
    };

    template<typename T, uint64_t max_length>
    class static_delayline
    {
    private:
        T m_buffer[max_length];
        uint64_t m_index;
        uint64_t m_length;

    public:
        static_delayline()
        {
            set_length(max_length - 1);
            clear();
        }

        T operator()(T input)
        {
            T output = m_buffer[m_index];
            m_buffer[m_index++] = input;
            if (m_index >= m_length)
                m_index = 0;
            return output;

        }

        void set_length(uint64_t length)
        {
            if (length >= max_length) SF_THROW(std::invalid_argument("length of delay too long: " + std::to_string(length * 1000 / SAMPLE_RATE)));
            m_length = length;
        }

        void clear()
        {
            memset(m_buffer, 0, sizeof(m_buffer));
            m_index = 0;
        }
    };

    template<typename T, uint64_t max_length>
    class static_delay_line_four_tap
    {
    private:
        T m_buffer[max_length];
        uint64_t m_index1, m_index2, m_index3, m_index4;
        uint64_t m_length;

    public:
        static_delay_line_four_tap()
        {
            set_length(max_length - 1);
            clear();
        }

        //get ouput and iterate
        T operator()(T input)
        {
            T output = m_buffer[m_index1];
            m_buffer[m_index1++] = input;
            if (m_index1 >= m_length)
                m_index1 = 0;
            if (++m_index2 >= m_length)
                m_index2 = 0;
            if (++m_index3 >= m_length)
                m_index3 = 0;
            if (++m_index4 >= m_length)
                m_index4 = 0;
            return output;

        }

        void set_index(uint64_t index1, uint64_t index2, uint64_t index3, uint64_t index4)
        {
            auto do_set = [&](uint64_t& to, uint64_t& from)
            {
                if (from >= m_length) SF_THROW(std::invalid_argument("offset of index too long: " + std::to_string(from * 1000ll / SAMPLE_RATE)));
                to = from;
            };
            do_set(m_index1, index1);
            do_set(m_index2, index2);
            do_set(m_index3, index3);
            do_set(m_index4, index4);
        }

        T get_index(uint64_t index)
        {
            switch (index)
            {
            case 0:
                return m_buffer[m_index1];
                break;
            case 1:
                return m_buffer[m_index2];
                break;
            case 2:
                return m_buffer[m_index3];
                break;
            case 3:
                return m_buffer[m_index4];
                break;
            default:
                return m_buffer[m_index1];
                break;
            }
        }

        void set_length(uint64_t length)
        {
            if (length >= max_length) SF_THROW(std::invalid_argument("length of four tap delay too long: " + std::to_string(length * 1000ll / SAMPLE_RATE)));
            m_length = length;
        }

        void clear()
        {
            memset(m_buffer, 0, sizeof(m_buffer));
            m_index1 = m_index2 = m_index3 = m_index4 = 0;
        }
    };

    template<typename T, uint64_t max_length>
    class static_delay_line_eight_tap
    {
    private:
        T m_buffer[max_length];
        uint64_t m_index1, m_index2, m_index3, m_index4, m_index5, m_index6, m_index7, m_index8;
        uint64_t m_length;

    public:
        static_delay_line_eight_tap()
        {
            set_length(max_length - 1);
            clear();
        }

        //get ouput and iterate
        T operator()(T input)
        {
            T output = m_buffer[m_index1];
            m_buffer[m_index1++] = input;
            if (m_index1 >= m_length)
                m_index1 = 0;
            if (++m_index2 >= m_length)
                m_index2 = 0;
            if (++m_index3 >= m_length)
                m_index3 = 0;
            if (++m_index4 >= m_length)
                m_index4 = 0;
            if (++m_index5 >= m_length)
                m_index5 = 0;
            if (++m_index6 >= m_length)
                m_index6 = 0;
            if (++m_index7 >= m_length)
                m_index7 = 0;
            if (++m_index8 >= m_length)
                m_index8 = 0;
            return output;

        }

        void set_index(uint64_t index1, uint64_t index2, uint64_t index3, uint64_t index4, uint64_t index5, uint64_t index6, uint64_t index7, uint64_t index8)
        {
            auto do_set = [&](uint64_t& to, uint64_t& from)
            {
                if (from >= m_length) SF_THROW(std::invalid_argument("offset of index too long: " + std::to_string(from * 1000ll / SAMPLE_RATE)));
                to = from;
            };
            do_set(m_index1, index1);
            do_set(m_index2, index2);
            do_set(m_index3, index3);
            do_set(m_index4, index4);
            do_set(m_index5, index1);
            do_set(m_index6, index2);
            do_set(m_index7, index3);
            do_set(m_index8, index4);
        }

        T get_index(uint64_t m_index)
        {
            switch (m_index)
            {
            case 0:
                return m_buffer[m_index1];
                break;
            case 1:
                return m_buffer[m_index2];
                break;
            case 2:
                return m_buffer[m_index3];
                break;
            case 3:
                return m_buffer[m_index4];
                break;
            case 4:
                return m_buffer[m_index5];
                break;
            case 5:
                return m_buffer[m_index6];
                break;
            case 6:
                return m_buffer[m_index7];
                break;
            case 7:
                return m_buffer[m_index8];
                break;
            default:
                return m_buffer[m_index1];
                break;
            }
        }

        void set_length(uint64_t length)
        {
            if (length >= max_length) SF_THROW(std::invalid_argument("length of four tap delay too long: " + std::to_string(length * 1000ll / SAMPLE_RATE)));
            m_length = length;
        }

        void clear()
        {
            memset(m_buffer, 0, sizeof(m_buffer));
            m_index1 = m_index2 = m_index3 = m_index4 = m_index5 = m_index6 = m_index7 = m_index8 = 0;
        }
    };

    template<typename T, uint64_t OverSampleCount>
    class StateVariable
    {
    public:

        enum FilterType
        {
            LOWPASS,
            HIGHPASS,
            BANDPASS,
            NOTCH,
            FilterTypeCount
        };

    private:

        T sampleRate;
        T frequency;
        T q;
        T f;

        T low;
        T high;
        T band;
        T notch;

        T* out;

    public:
        StateVariable()
        {
            SetSampleRate(SAMPLE_RATE);
            Frequency(1000.);
            Resonance(0);
            Type(LOWPASS);
            Reset();
        }

        T operator()(T input)
        {
            for (uint64_t i = 0; i < OverSampleCount; i++)
            {
                low += f * band + 1e-25;
                high = input - low - q * band;
                band += f * high;
                notch = low + high;
            }
            return *out;
        }

        void Reset()
        {
            low = high = band = notch = 0;
        }

        void SetSampleRate(T sampleRate)
        {
            this->sampleRate = sampleRate * OverSampleCount;
            UpdateCoefficient();
        }

        void Frequency(T frequency)
        {
            this->frequency = frequency;
            UpdateCoefficient();
        }

        void Resonance(T resonance)
        {
            this->q = 2 - 2 * resonance;
        }

        void Type(uint64_t type)
        {
            switch (type)
            {
            case LOWPASS:
                out = &low;
                break;

            case HIGHPASS:
                out = &high;
                break;

            case BANDPASS:
                out = &band;
                break;

            case NOTCH:
                out = &notch;
                break;

            default:
                out = &low;
                break;
            }
        }

    private:
        void UpdateCoefficient()
        {
            f = 2. * sinf(sonic_field::PI * frequency / sampleRate);
        }
    };
    } // mverb

    namespace sonic_field
    {

    std::unique_ptr<mreverb> create_mreverb(
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
        std::unique_ptr<mreverb> ret{ new mreverb{} };
        ret->setParameter(mreverb::DAMPINGFREQ, damping_freq);
        ret->setParameter(mreverb::DENSITY, density);
        ret->setParameter(mreverb::BANDWIDTHFREQ, bandwidth_freq);
        ret->setParameter(mreverb::DECAY, decay);
        ret->setParameter(mreverb::PREDELAY, predelay / 1000.0);
        ret->setParameter(mreverb::SIZE, size);
        ret->setParameter(mreverb::GAIN, gain);
        ret->setParameter(mreverb::MIX, mix);
        ret->setParameter(mreverb::EARLYMIX, early_mix);
        ret->reset();
        return ret;
    }
    std::pair<double*, double*> mreverb_process_block(mreverb* verb, double* left, double* right)
    {
        double* lrin[2];
        lrin[0] = left;
        lrin[1] = right;
        double* lrout[2];
        lrout[0] = new_block(false);
        lrout[1] = new_block(false);
        verb->process(lrin, lrout, BLOCK_SIZE);
        free_block(left);
        if (left != right) free_block(right);
        return { lrout[0], lrout[1] };
    }

    std::pair<double*, double*> mreverb_process_block(mreverb* verb, double* block_in)
    {
        return mreverb_process_block(verb, block_in, block_in);
    }

    mreverberator::mreverberator(
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
        double early_mix): m_left{ add_to_scope(new signal_writer{ left }) }, m_right{ add_to_scope(new signal_writer{ right }) }
    {
        m_reverb = create_mreverb(damping_freq, density, bandwidth_freq, decay, predelay, size, gain, mix, early_mix);
    }

    class writer_plug : public signal_mono_base
    {
        double* m_data;

    public:
        writer_plug() : m_data{ nullptr } {};

        virtual double* next() override
        {
            return m_data;
        }

        void set_data(double* data)
        {
            m_data = data;
        }

        virtual const char* name() override
        {
            return "writer_plug";
        }
    };

    void mreverberator::inject(signal& in)
    {
        SF_MARK_STACK;
        signal_base::inject(in);
        auto cnt = input_count();
        if (cnt == 1) return;
        if (cnt > 2) SF_THROW(std::logic_error{ "Reverberators can only have two inputs" });
        writer_plug* left_plug = new writer_plug{};
        writer_plug* right_plug = new writer_plug{};
        m_left.inject({ add_to_scope(left_plug) });
        m_right.inject({ add_to_scope(right_plug) });
        while (true)
        {
            auto get_or_make = [](double* data)
            {
                if (data == empty_block())
                {
                    data = new_block();
                }
                return data;
            };
            auto left = get_or_make(input(0).next());
            auto right = get_or_make(input(1).next());
            if (left == nullptr && right == nullptr)
            {
                left_plug->set_data(nullptr);
                right_plug->set_data(nullptr);
                m_left.next();
                m_right.next();
                return;
            }
            if (left == nullptr || right == nullptr) SF_THROW(std::logic_error{ "Not all mixing inputs same length" });
            auto verbed = mreverb_process_block(m_reverb.get(), left, right);
            left_plug->set_data(verbed.first);
            right_plug->set_data(verbed.second);
            free_block(m_left.next());
            free_block(m_right.next());
        }
    }
    double* mreverberator::next() 
    {
        SF_MARK_STACK;
        SF_THROW(std::logic_error{ "Cannot call next on a rebererator" });
    }

    const char* mreverberator::name()
    {
        return "mreverberator";
    }

    signal_base* mreverberator::copy()
    {
        SF_THROW(std::logic_error{ "Cannot call copy on a rebererator" });
    }

    echo_chamber::echo_chamber(
        uint64_t delay,
        double feedback,
        double mix,
        double saturate,
        double wow,
        double flutter) :
        m_buffer{ new double[delay * BLOCK_SIZE] },
        m_delay{ delay },
        m_feedback{ feedback },
        m_mix{ mix },
        m_saturate{ saturate },
        m_wow{ wow },
        m_flutter{ flutter },
        m_index{ 0 }
    {
        memset(m_buffer, 0, sizeof(double)* delay * BLOCK_SIZE);
    };

    double* echo_chamber::next()
    {
        return process_no_skip([&](double* block) {
            if (block)
            {
                uint64_t length = m_delay * BLOCK_SIZE;
                constexpr double wow_rate = 2 * PI * 1.0 / SAMPLES_PER_SECOND;
                constexpr double flutter_rate = 2 * PI * 40.0 / SAMPLES_PER_SECOND;

                for (uint64_t idx{ 0 }; idx < BLOCK_SIZE; ++idx)
                {
                    // Up to +-10ms wow and +-1 ms flutter.
                    uint64_t wow = int64_t(m_wow * (1.0+fast_cos(m_index * wow_rate)) * double(BLOCK_SIZE) * 10.0);
                    uint64_t flutter = int64_t(m_flutter * (1.0+fast_cos(m_index * flutter_rate)) * double(BLOCK_SIZE));
                    uint64_t at = (wow + flutter + m_index) % length;
                    double evalue = m_buffer[at];
                    double ivalue = block[idx];
                    double value = evalue * m_mix + ivalue * (1.0 - m_mix);
                    block[idx] = value;
                    value = value* m_feedback + ivalue * (1.0 - m_feedback);
                    evalue = value * (1.0 - m_saturate);
                    if (value < 0)
                        evalue -= m_saturate * std::pow(-value, 0.98);
                    else
                        evalue += m_saturate * std::pow(value, 0.98);
                    m_buffer[m_index % length] = evalue;
                    ++m_index;
                }
            }
            return block;
            }, input().next());

    }
    const char* echo_chamber::name()
    {
        return "echo_chamber";
    }

    signal_base* echo_chamber::copy()
    {
        SF_MARK_STACK;
        return new echo_chamber{m_delay, m_feedback, m_mix, m_saturate, m_wow, m_flutter};
    }

    echo_chamber::~echo_chamber()
    {
        if (m_buffer) delete[] m_buffer;
    }

    situator::situator(situator_input_t& taps):
        m_buffer{ nullptr },
        m_length{ 0 },
        m_position{ 0 }
    {
        m_taps = taps;
        for (const auto& tap : m_taps)
        {
            if (tap.first > m_length) m_length = tap.first;
        }
        m_length *= BLOCK_SIZE;
        m_buffer = new double[m_length];
        memset(m_buffer, 0, sizeof(double)* m_length);
    }

    situator::~situator()
    {
        if (m_buffer) delete[] m_buffer;
    }

    double* situator::next()
    {
        return process_no_skip([&](double* block)
        {
            if (block)
            {
                auto out = new_block();
                for (uint64_t idx{ 0 }; idx < BLOCK_SIZE; ++idx)
                {
                    double accumulator{block[idx]};
                    m_buffer[m_position % m_length] = accumulator;
                    for (const auto& tap : m_taps)
                    {
                        int64_t at = int64_t(m_position) - int64_t(tap.first * BLOCK_SIZE);
                        if (at > 0)
                        {
                            accumulator += m_buffer[at % m_length] * tap.second;
                        }
                    }
                    out[idx] = accumulator;
                    ++m_position;
                }
                free_block(block);
                block = out;
            }
            return block;
        }, input().next());
    }

    const char* situator::name()
    {
        return "situator";
    }

    signal_base* situator::copy()
    {
        return new situator{ m_taps };
    }

} // sonic_field

