#pragma once
#include "library.h"

namespace sonic_field
{

    signal screach_gen(uint64_t length, double pitch, double boost)
    {
        if (length < 500) SF_THROW(std::invalid_argument{ "Screach length must be > 500ms" });

        auto ladder = ladder_filter();
        generate_noise(length)
            >> ladder;
        generate_linear({ {0, 0.75}, {length / 4, 1.0}, {length * 3 / 4, 1.0}, {length, 0.85} })
            >> ladder;
        generate_linear({ {0, pitch * 0.8}, {400, pitch * 1.0}, {length / 2, pitch * 1.1}, {length, pitch * 1.0} })
            >> ladder;

        auto ladder2 = ladder_filter();
        ladder
            >> ladder2;
        generate_linear({ {0, 0.85}, {250, 1.0}, {length * 3 / 4, 1.0}, {length, 0.85} })
            >> ladder2;
        generate_linear({ {0, pitch * 0.8}, {length * 1 / 4, pitch * (1.0+boost)}, {length / 2, pitch * (1.1+boost)}, {length, pitch * 1.0} })
            >> ladder2;

        auto env_mixer = mix( mixer_type::MULTIPLY );
        generate_linear({{ 0, 0.0 }, { 100, 1.0 }, { length / 2, 1.0 }, { length, 0.0 }})
            >> env_mixer;
        ladder2
            >> env_mixer;

        return env_mixer
            >> filter_rbj(filter_type::HIGHPASS, pitch, 1, 0.0)
            >> filter_rbj(filter_type::HIGHPASS, pitch, 1, 0.0);
    }

    void screach_mix(double pan_start, double pan_end, uint64_t length, double pitch, double boost)
    {
        SF_SCOPE("kites_start");
        std::cerr << "Screach "
            << " pad_start:" << pan_start
            << " pad_end:" << pan_end
            << " length:" << length
            << " pitch:" << pitch
            << " boost:" << boost
            << std::endl;

        {
            SF_SCOPE("kites_pan_mix");
            auto pan_mixerl = mix(mixer_type::MULTIPLY);
            screach_gen(length, pitch, boost)
                >> pan_mixerl;
            generate_linear({ { 0, 1.0 - pan_start }, {length, pan_end } })
                >> pan_mixerl;
            pan_mixerl >> write("screachl");

            auto pan_mixerr = mix(mixer_type::MULTIPLY);
            screach_gen(length, pitch, boost)
                >> pan_mixerr;
            generate_linear({ { 0, 1.0 - pan_start }, {length, pan_end } })
                >> pan_mixerr;
            pan_mixerr >> write("screachr");
        }

        {
            SF_SCOPE("kites_verbl");
            auto mxl = mix(mixer_type::APPEND);
            auto mxr = mix(mixer_type::APPEND);
            read("screachl") >> mxl;
            generate_silence(8000) >> mxl;
            read("screachr") >> mxr;
            generate_silence(8000) >> mxr;

            auto reverb = mreverberate(
                "revl",
                "revr",
                5000.0,    // Damping frequ
                0.5,       // Density
                10000.0,   // Bandwidth frequ
                0.5,       // Decay 0-1
                500.0,    // Preday ms
                1.1,       // Size
                1.0,       // Gain
                1.0,       // Mix
                1.0        // Early mix
            );
            mxl >> reverb;
            mxr >> reverb;
        }
        {
            SF_SCOPE("kites_verbr");
            auto mxl = mix(mixer_type::APPEND);
            auto mxr = mix(mixer_type::APPEND);
            read("screachl") >> mxl;
            generate_silence(8000) >> mxl;
            read("screachr") >> mxr;
            generate_silence(8000) >> mxr;

            auto reverb = mreverberate(
                "revlb",
                "revrb",
                5000.0,    // Damping frequ
                0.5,       // Density
                2000.0,    // Bandwidth frequ
                0.5,       // Decay 0-1
                2000.0,    // Preday ms
                1.1,       // Size
                1.0,       // Gain
                1.0,       // Mix
                1.0        // Early mix
            );
            mxl >> reverb;
            mxr >> reverb;
        }

        auto mxol = mix(mixer_type::OVERLAY);
        auto mxor = mix(mixer_type::OVERLAY);
        read("revl")
            >> distort_power(0.90)
            >> amplify(1)
            >> mxol;
        read("revr")
            >> distort_power(0.90)
            >> amplify(1)
            >> mxor;
        read("revlb")
            >> distort_power(0.8)
            >> amplify(0.5)
            >> mxol;
        read("revrb")
            >> distort_power(0.8)
            >> amplify(0.5)
            >> mxor;
        read("screachl") >> distort_power(1.25) >> amplify(1.5) >> mxol;
        read("screachr") >> distort_power(1.25) >> amplify(1.5) >> mxor;

        auto pan_mixerl = mix(mixer_type::MULTIPLY);
        mxol
            >> cut(0, 0, 10000, 0)
            >> pan_mixerl;
        generate_linear({ { 0, 1.0 - pan_start }, { 5000, pan_start }, { 10000, pan_start } })
            >> cut(0, 0, 10000, 0)
            >> pan_mixerl;
        pan_mixerl >> write("screachl_done");

        auto pan_mixerr = mix(mixer_type::MULTIPLY);
        mxor 
            >> cut(0, 0, 10000, 0)
            >> pan_mixerr;
        generate_linear({ { 0, pan_start }, { 5000, 1.0 - pan_start }, { 10000, 1.0 - pan_start } })
            >> cut(0, 0, 10000, 0)
            >> pan_mixerr;
        pan_mixerr >> write("screachr_done");
    }

    void kites()
    {
        random_doubles pan{};
        random_doubles delay_to_next{};
        random_doubles pitch_shift{};
        random_doubles pitch_boost{};
        random_doubles length{};
        random_doubles volume{};

        SF_SCOPE("kites_main_lop");
        uint64_t position{ 0 };
        uint64_t count{ 20 };
        auto mxl = mix(mixer_type::OVERLAY);
        auto mxr = mix(mixer_type::OVERLAY);
        generate_silence(7000 * count + 20000)
            >> mxl;
        generate_silence(7000 * count + 20000)
            >> mxr;
        for (uint64_t note{ 0 }; note < count; ++note)
        {

            // Example of a nice screach.
            //screach_mix(0.2, 0.8, 2500, 128 * 10, 0.25);
            auto pan_start = std::abs(pan());
            auto pan_end = 1.0 - pan_start;
            auto boost = pitch_boost();
            while (std::abs(boost) > 0.5)
                boost = pitch_boost();
            if (boost < 0.0) boost *= 0.5;

            screach_mix(
                std::abs(pan_start),
                std::abs(pan_end),
                uint64_t(2500.0 + length() * 500.0),
                128 * (11+ (2 * pitch_shift())),
                boost
            );

            auto vol = volume();
            while(std::abs(vol) < 0.1)
                vol = volume();
            read("screachl_done")
                >> write("screachl_done" + std::to_string(note));
            read("screachr_done")
                >> write("screachr_done" + std::to_string(note));

            read("screachl_done" + std::to_string(note))
                >> amplify(vol)
                >> cut(position, 0, 10000, 0)
                >> mxl;
            read("screachr_done" + std::to_string(note))
                >> amplify(vol)
                >> cut(position, 0, 10000, 0)
                >> mxr;

            position += 6000 + uint64_t(2000 * delay_to_next());
        }

        mxl
            >> echo(250, 0.85, 0.5, 0.1, 0.02, 0.01)
            >> write("screachl_final");

        mxr
            >> echo(250, 0.85, 0.5, 0.1, 0.02, 0.01)
            >> write("screachr_final");
    }

    void thunder()
    {
        SF_SCOPE("thunder_before_kites");

        auto reverb = mreverberate(
            "revlb",
            "revrb",
            1000.0,    // Damping frequ
            0.5,       // Density
             500.0,    // Bandwidth frequ
            1.0,       // Decay 0-1
            10.0,      // Preday ms
            1.0,       // Size
            1.0,       // Gain
            0.5,       // Mix
            0.25       // Early mix
        );

        auto thunder_mixl = mix(mixer_type::MULTIPLY);
        generate_noise(300)
            >> thunder_mixl;
        generate_linear({ {0,0}, {50,1}, { 100,0.25}, {120, 1}, {200,0}, {210,1}, {300, 0}})
            >> thunder_mixl;
        thunder_mixl
            >> cut(10,0,300,60030)
            >> distort_power(2.0)
            >> filter_rbj(filter_type::LOWPASS, 1000, 2, 1)
            >> distort_power(2.0)
            >> filter_rbj(filter_type::LOWPASS, 600, 1, 4)
            >> control_gain(0.1, 0.005)
            >> distort_saturate(1.0)
            >> filter_rbj(filter_type::LOWPASS, 300, 1, 4)
            >> filter_rbj(filter_type::PEAK, 64, 1, 10)
            >> echo(350, 0.2, 0.5, 0.5, 0.0, 0.1)
            >> echo(5000, 0.85, 0.5, 0., 0.1, 0.0)
            >> echo(11000, 0.70, 0.5, 0., 0.1, 0.0)
            >> distort_power(1.1)
            >> reverb;

        auto thunder_mixr = mix(mixer_type::MULTIPLY);
        generate_noise(300)
            >> thunder_mixr;
        generate_linear({ {0,0}, {60,1}, { 90,0.25}, {130, 1}, {200,0} , {210,1}, {300, 0 }})
            >> thunder_mixr;
        thunder_mixr
            >> cut(40, 0, 300, 60000)
            >> distort_power(2.0)
            >> filter_rbj(filter_type::LOWPASS, 1000, 2, 1)
            >> distort_power(2.0)
            >> filter_rbj(filter_type::LOWPASS, 600, 1, 4)
            >> control_gain(0.1, 0.005)
            >> distort_saturate(1.0)
            >> filter_rbj(filter_type::LOWPASS, 300, 1, 4)
            >> filter_rbj(filter_type::PEAK, 64, 1, 10)
            >> echo(360, 0.2, 0.5, 0.5, 0.0, 0.1)
            >> echo(5600, 0.85, 0.5, 0., 0.1, 0.0)
            >> echo(10000, 0.70, 0.5, 0., 0.1, 0.0)
            >> distort_power(1.1)
            >> reverb;

        read("revlb")
            >> echo(250, 0.85, 0.5, 0.1, 0.02, 0.01)
            >> write("thunderl_final");

        read("revrb")
            >> echo(250, 0.85, 0.5, 0.1, 0.02, 0.01)
            >> write("thunderr_final");
    }

    void rain()
    {
        SF_SCOPE("rain");

        auto mixl = mix(mixer_type::MULTIPLY);
        auto mixr = mix(mixer_type::MULTIPLY);

        generate_noise(30000)
            >> distort_power(100)
            >> filter_rbj(filter_type::LOWPASS, 1000, 1, 1)
            >> echo(250, 0.5, 0.25, 0.1, 0.02, 0.01)
            >> mixl;

        generate_linear({ {0, 0}, {20000, 1}, {30000, 0} })
            >> mixl;
        mixl
            >> write("rainl_final");

        generate_noise(30000)
            >> distort_power(100)
            >> filter_rbj(filter_type::LOWPASS, 1000, 1, 1)
            >> echo(250, 0.5, 0.25, 0.1, 0.02, 0.01)
            >> mixr;


        generate_linear({ {0, 0}, {18000, 1}, {27000, 0.1}, {30000, 0} })
            >> mixr;
        mixr
            >> write("rainr_final");

    }

    void kites_music()
    {
        {
            auto mxl = mix(mixer_type::ADD);
            auto mxr = mix(mixer_type::ADD);

            generate_windy_base(30000, 64.0)
                >> distort_power(1.1)
                >> mxl;
            generate_windy_base(30000, 64.0 * 3 / 2)
                >> distort_power(1.1)
                >> mxl;
            generate_windy_base(30000, 64.0 * 2.0)
                >> distort_power(1.1)
                >> mxl;

            generate_windy_base(30000, 64)
                >> distort_power(1.1)
                >> mxr;
            generate_windy_base(30000, 64.0 * 3 / 2)
                >> distort_power(1.1)
                >> mxr;
            generate_windy_base(30000, 64.0 * 2.0)
                >> distort_power(1.1)
                >> mxr;
            mxl
                >> write("musicl_1");

            mxr
                >> write("musicr_1");
        }

        {
            auto mxl = mix(mixer_type::ADD);
            auto mxr = mix(mixer_type::ADD);
            generate_rich_base(15000, 128.0)
                >> distort_power(1.5)
                >> mxl;
            generate_windy_base(15000, 128.0 * 5/4)
                >> distort_power(1.1)
                >> mxl;
            generate_windy_base(15000, 128.0 * 4 / 3)
                >> distort_power(1.1)
                >> mxl;

            generate_rich_base(15000, 128)
                >> distort_power(1.5)
                >> mxr;
            generate_windy_base(15000, 128.0 * 5 / 3)
                >> distort_power(1.1)
                >> mxr;
            generate_windy_base(15000, 128.0 * 4 / 4)
                >> distort_power(1.1)
                >> mxr;
            mxl
                >> write("musicl_2");

            mxr
                >> write("musicr_2");
        }

        {
            auto mxl = mix(mixer_type::ADD);
            auto mxr = mix(mixer_type::ADD);
            generate_rich_base(15000, 128.0 * 4 / 3)
                >> mxl;
            generate_windy_base(15000, 128.0 * 3 / 2)
                >> mxl;
            generate_windy_base(15000, 128.0 * 7 / 4)
                >> mxl;

            generate_rich_base(15000, 128 * 4 / 3)
                >> mxr;
            generate_windy_base(15000, 128.0 * 3 / 2)
                >> mxr;
            generate_windy_base(15000, 128.0 * 9 / 5)
                >> mxr;
            mxl
                >> write("musicl_3");

            mxr
                >> write("musicr_3");
        }

        {
            auto mxl = mix(mixer_type::ADD);
            auto mxr = mix(mixer_type::ADD);
            generate_rich_base(15000, 128.0 * 4 / 3)
                >> mxl;
            generate_rich_base(15000, 128.0 * 3 / 2)
                >> mxl;
            generate_rich_base(15000, 128.0 * 9 / 5)
                >> mxl;

            generate_rich_base(15000, 128 * 4 / 3)
                >> mxr;
            generate_rich_base(15000, 128.0 * 3 / 2)
                >> mxr;
            generate_rich_base(15000, 128.0 * 7 / 4)
                >> mxr;
            mxl
                >> write("musicl_3");

            mxr
                >> write("musicr_3");
        }

        {
            auto mxl = mix(mixer_type::ADD);
            auto mxr = mix(mixer_type::ADD);
            generate_rich_base(15000, 96.0)
                >> mxl;
            generate_rich_base(15000, 96.0 * 5/3)
                >> mxl;
            generate_rich_base(15000, 96.0 * 3/2)
                >> mxl;

            generate_rich_base(15000, 96.01)
                >> mxr;
            generate_rich_base(15000, 95.99 * 5 / 3)
                >> mxr;
            generate_rich_base(15000, 95.99 * 7 / 4)
                >> mxr;
            mxl
                >> write("musicl_4");

            mxr
                >> write("musicr_4");
        }

        {
            auto mxl = mix(mixer_type::ADD);
            auto mxr = mix(mixer_type::ADD);

            generate_rich_base(15000, 64.0)
                >> mxl;
            generate_rich_base(15000, 64.0 * 3 / 2)
                >> mxl;
            generate_rich_base(15000, 64.0 * 2.0)
                >> mxl;

            generate_rich_base(15000, 64)
                >> mxr;
            generate_rich_base(15000, 64.0 * 3 / 2)
                >> mxr;
            generate_rich_base(15000, 64.0 * 2.0)
                >> mxr;
            mxl
                >> write("musicl_5");

            mxr
                >> write("musicr_5");
        }
        auto mxl = mix(mixer_type::OVERLAY);
        auto mxr = mix(mixer_type::OVERLAY);

        generate_silence(120000)
            >> mxl;
        generate_silence(120000)
            >> mxr;

        read("musicl_1")
            >> amplify(4.0)
            >> mxl;
        read("musicl_2")
            >> amplify(2.0)
            >> cut(30000,0,15000,0)
            >> mxl;
        read("musicl_3")
            >> cut(45000, 0, 15000, 0)
            >> amplify(0.75)
            >> mxl;
        read("musicl_4")
            >> cut(60000, 0, 15000, 0)
            >> mxl;
        read("musicl_5")
            >> cut(75000, 0, 15000, 0)
            >> mxl;
        read("musicl_1")
            >> amplify(2.0)
            >> cut(90000, 0, 30000, 0)
            >> mxl;
        mxl
            >> echo(250, 0.85, 0.5, 0.1, 0.02, 0.01);

        read("musicr_1")
            >> amplify(4.0)
            >> mxr;
        read("musicr_2")
            >> amplify(2.0)
            >> cut(30000, 0, 15000, 0)
            >> mxr;
        read("musicr_3")
            >> cut(45000, 0, 15000, 0)
            >> amplify(0.75)
            >> mxr;
        read("musicr_4")
            >> cut(60000, 0, 15000, 0)
            >> mxr;
        read("musicr_5")
            >> cut(75000, 0, 15000, 0)
            >> mxr;
        read("musicr_1")
            >> amplify(2.0)
            >> cut(90000, 0, 30000, 0)
            >> mxr;
        mxr
            >> echo(250, 0.85, 0.5, 0.1, 0.02, 0.01)
            >> write("musicr_final");
    }

    void kites_over_water()
    {
        SF_SCOPE("kites_over_water");

        kites();
        thunder();
        rain();
        kites_music();
        auto mxl = mix(mixer_type::OVERLAY);
        auto mxr = mix(mixer_type::OVERLAY);

        generate_silence(300000)
            >> mxl;
        read("screachl_final")
            >> cut(55000, 0, 200000, 0)
            >> mxl;
        read("rainl_final")
            >> cut(19000, 0, 30000, 0)
            >> amplify(0.25)
            >> mxl;
        read("musicl_final")
            >> cut(40000, 0, 200000, 0)
            >> amplify(0.5)
            >> mxl;
        read("thunderl_final")
            >> mxl;

        generate_silence(300000)
            >> mxr;
        read("screachr_final")
            >> cut(55000, 0, 200000, 0)
            >> mxr;
        read("rainr_final")
            >> cut(19000, 0, 30000, 0)
            >> amplify(0.25)
            >> mxr;
        read("musicr_final")
            >> cut(40000, 0, 200000, 0)
            >> amplify(0.5)
            >> mxr;
        read("thunderr_final")
            >> mxr;

        mxl >> write("kites_over_water_left");
        mxr >> write("kites_over_water_right");

        signal_to_wav("kites_over_water_left");
        signal_to_wav("kites_over_water_right");

    }

    void strange_kites()
    {
        random_doubles pan{};
        random_doubles delay_to_next{};
        random_doubles pitch_shift{};
        random_doubles pitch_boost{};
        random_doubles length{};
        random_doubles volume{};

        SF_SCOPE("kites_main_lop");
        uint64_t position{ 0 };
        uint64_t count{ 100 };
        auto mxl = mix(mixer_type::OVERLAY);
        auto mxr = mix(mixer_type::OVERLAY);
        generate_silence(10000 * count + 20000)
            >> mxl;
        generate_silence(10000 * count + 20000)
            >> mxr;
        for (uint64_t note{ 0 }; note < count; ++note)
        {

            // Example of a nice screach.
            //screach_mix(0.2, 0.8, 2500, 128 * 10, 0.25);
            auto pan_start = std::abs(pan());
            auto pan_end = 1.0 - pan_start;
            auto boost = pitch_boost();
            while (std::abs(boost) > 0.5)
                boost = pitch_boost();
            if (boost < 0.0) boost *= 0.5;

            screach_mix(
                std::abs(pan_start),
                std::abs(pan_end),
                uint64_t(2500.0 + std::abs(length()) * 5000.0),
                128 * (1 + (12 * std::abs(pitch_shift()))),
                boost
            );

            auto vol = volume();
            while (std::abs(vol) < 0.1)
                vol = volume();
            read("screachl_done")
                >> write("screachl_done" + std::to_string(note));
            read("screachr_done")
                >> write("screachr_done" + std::to_string(note));

            read("screachl_done" + std::to_string(note))
                >> amplify(vol)
                >> cut(position, 0, 20000, 0)
                >> mxl;
            read("screachr_done" + std::to_string(note))
                >> amplify(vol)
                >> cut(position, 0, 20000, 0)
                >> mxr;

            position += 6000 + uint64_t(2000 * delay_to_next());
        }

        mxl
            >> echo(250, 0.85, 0.5, 0.1, 0.02, 0.01)
            >> write("screachl_final");

        mxr
            >> echo(250, 0.85, 0.5, 0.1, 0.02, 0.01)
            >> write("screachr_final");
    }

    void strange_kites_write()
    {
        strange_kites();
        signal_to_wav("screachl_final");
        signal_to_wav("screachr_final");
    }

} // sonic_field
