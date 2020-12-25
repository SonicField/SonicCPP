#pragma once
#include "../sonic_field.h"

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

	void make_full()
	{
		SF_SCOPE("mak_full");

		auto single_run = [&](uint64_t x, double pitch)
		{
			SF_SCOPE("single_run");
			auto sig_name = "temp" + std::to_string(x);

			auto final_blend = mix(mixer_type::ADD);
			generate_rich_base(10000, pitch) >> amplify(0.5) >> final_blend;
			generate_windy_base(10000, pitch) >> final_blend;
			//generate_windy_base(10000, pitch * 0.75) >> final_blend;
			final_blend >> write(sig_name);
		};

		for (uint64_t x{ 0 }; x < 10; ++x)
		{
			single_run(x, 128);
		}

		for (uint64_t x{ 10 }; x < 20; ++x)
		{
			single_run(x, 128.0 * 7.0 / 8.0);
		}

		{
			auto mx = mix(mixer_type::ADD);
			for (uint64_t x{ 0 }; x < 10; ++x)
			{
				auto sig_name = "temp" + std::to_string(x);
				read(sig_name) >> amplify(x / 10.0) >> mx;
			}
			auto pd = mix(mixer_type::APPEND);
			mx >> pd;
			generate_silence(5000) >> pd;
			auto env = mix(mixer_type::MULTIPLY);
			generate_linear({ {0, 0.0}, {100, 1.0}, {5000, 1.0}, {10000, 0.0}, {15000, 0.0} })
				>> filter_rbj(filter_type::LOWPASS, 100, 1, 0.0)
				>> env;
			pd >> env;
			env >> write("left_a");
		}

		{
			auto mx = mix(mixer_type::ADD);
			for (uint64_t x{ 0 }; x < 10; ++x)
			{
				auto sig_name = "temp" + std::to_string(x);
				read(sig_name) >> amplify(1.0 - x / 10.0) >> mx;
			}
			auto pd = mix(mixer_type::APPEND);
			mx >> pd;
			generate_silence(5000) >> pd;
			auto env = mix(mixer_type::MULTIPLY);
			generate_linear({ {0, 0.0}, {100, 1.0}, {5000, 1.0}, {10000, 0.0} ,{15000, 0.0} })
				>> filter_rbj(filter_type::LOWPASS, 100, 1, 0.0)
				>> env;
			pd >> env;
			env >> write("right_a");
		}

		{
			auto mx = mix(mixer_type::ADD);
			for (uint64_t x{ 10 }; x < 20; ++x)
			{
				auto sig_name = "temp" + std::to_string(x);
				read(sig_name) >> amplify(x / 10.0) >> mx;
			}
			auto pd = mix(mixer_type::APPEND);
			generate_silence(5000) >> pd;
			mx >> pd;
			auto env = mix(mixer_type::MULTIPLY);
			generate_linear({ {0, 0.0}, {5000, 0.0}, {10000, 1.0}, {15000 - 100, 1.0 }, {15000, 0.0 } })
				>> filter_rbj(filter_type::LOWPASS, 100, 1, 0.0)
				>> env;
			pd >> env;
			env >> write("left_b");
		}

		{
			auto mx = mix(mixer_type::ADD);
			for (uint64_t x{ 10 }; x < 20; ++x)
			{
				auto sig_name = "temp" + std::to_string(x);
				read(sig_name) >> amplify(1.0 - x / 10.0) >> mx;
			}
			auto pd = mix(mixer_type::APPEND);
			generate_silence(5000) >> pd;
			mx >> pd;
			auto env = mix(mixer_type::MULTIPLY);
			generate_linear({ {0, 0.0}, {5000, 0.0}, {10000, 1.0}, {15000 - 100,1.0 }, {15000, 0.0 } })
				>> filter_rbj(filter_type::LOWPASS, 100, 1, 0.0)
				>> env;
			pd >> env;
			env >> write("right_b");
		}

		SF_MARK_STACK;
		{
			auto mx = mix(mixer_type::ADD);
			read("left_a") >> mx;
			read("left_b") >> mx;
			mx >> write("left");
		}

		SF_MARK_STACK;
		{
			auto mx = mix(mixer_type::ADD);
			SF_MARK_STACK;
			read("right_a") >> mx;
			SF_MARK_STACK;
			read("right_b") >> mx;
			mx >> write("right");
		}

		SF_MARK_STACK;
		signal_to_wav("left");
		signal_to_wav("right");
	}

	void loopy()
	{
		SF_MARK_STACK;
		SF_SCOPE("loopy");
		for (uint64_t idx{ 0 }; idx < 1; ++idx)
		{
			auto thing = []() {
				SF_SCOPE("thing");
				//auto runner = run();
				//auto gener = generate_noise(10000);
				//auto reper = repeat(3, { filter_rbj(filter_type::PEAK, 245, 0.1, 20) });
				//gener >> reper;
				//auto x = reper >> runner;
				auto mx = mix(mixer_type::ADD);
				SF_MARK_STACK;
				generate_noise(10000) >> repeat(1, { filter_rbj(filter_type::PEAK, 245, 0.1, 20) }) >> mx;
				SF_MARK_STACK;
				mx;
			};
			{
				auto x = idx;
				double pitch = 256;
				SF_MARK_STACK;
				auto sig_name = "temp" + std::to_string(x);

				SF_MARK_STACK;
				thing();
				SF_MARK_STACK;
			}
		}
	}
	/*
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
	*/
	void test_reverb()
	{
		SF_SCOPE("reverb");
		auto reverb = mreverberate(
			"revl",
			"revr",
			5000.0,    // Damping frequ
			0.1,       // Density
			10000.0,   // Bandwidth frequ
			1.0,       // Decay 0-1
			100.0,     // Preday ms
			1.0,       // Size
			1.0,       // Gain
			0.75,      // Mix
			0.50       // Early mix
		);
		auto mxl = mix(mixer_type::APPEND);
		auto mxr = mix(mixer_type::APPEND);
		generate_windy_base(1000, 256) >> mxl;
		generate_silence(20000) >> mxl;
		generate_windy_base(1000, 256) >> mxr;
		generate_silence(20000) >> mxr;
		mxl >> reverb;
		mxr >> reverb;
		signal_to_wav("revl");
		signal_to_wav("revr");
}
}
