#include <iostream>
#include <cstdint>
#include "test/tests.h"
#include "music/kites_over_water.h"
#include "music/filter_demo.h"
#include "music/fractured_mind.h"
#include "music/cactus.h"
#include "music/reconstructor.h"

// The current thing to run.
//auto target = []{ sonic_field::combe_demo(); };
auto target = []{ sonic_field::filter_demo(); };

int main(int argc, char** argv)
{
    bool track_memory = false;
    std::unordered_map<std::string, std::string> options{};
    // Argument and if it requires a value. Defaults not implement atm.
    std::unordered_map<std::string, bool> possible_options{
        {"--test", false},
        {"--test-data", true},
        {"--generate", false},
        {"--generate-named", true},
        {"--work-space", true},
        {"--output-space", true},
        {"--verbose", false},
        {"--help", false}
    };
    try
    {
        SF_MARK_STACK;
        // Parse the command line.
        for (int i{ 1 }; i < argc; ++i)
        {
            std::string command = argv[i];
            std::cout << "Reading command: " << command << std::endl;
            auto found = possible_options.find(command);
            if (found == possible_options.end())
            {
                SF_THROW(std::invalid_argument{ "Command line argument '" + command + "' not found" });
            }
            if (found->second)
            {
                ++i;
                if (i >= argc)
                {
                    SF_THROW(std::invalid_argument{ "Command line argument '" + command + "' requires a value" });
                }
                std::string value = argv[i];
                std::cout << "Reading value:   " << command << std::endl;
                if (value.substr(0, 2) == "--")
                {
                    SF_THROW(std::invalid_argument{ "Command line argument '" + command + "' requires a value" });
                }
                options[command] = value;
            }
            else
            {
                options[command] = "";
            }
        }

        auto in = [&](const auto& key)
        {
            return options.find(key) != options.end();
        };

        auto not_in = [&](const auto& key)
        {
            return !in(key);
        };

        // Print help and return.
        if (in("--help"))
        {
            std::cout << "SonicCpp Command Line Argments" << "\n";
            for (const auto& pair : possible_options)
            {
                std::cout << pair.first;
                if (pair.second)
                {
                    std::cout << " <value>";
                }
                std::cout << "\n";
            }
            return 0;
        }

        // First check arguments are rational.
        if (not_in("--work-space"))
        {
            SF_THROW(std::invalid_argument{ "Missing --work-space <value> from command line" });
        }
        if (not_in("--output-space"))
        {
            SF_THROW(std::invalid_argument{ "Missing --output-space <value> from command line" });
        }
        sonic_field::set_work_space(options["--work-space"]);
        sonic_field::set_output_space(options["--output-space"]);

        // Do verbose (in memory tracking)
        if (in("--verbose"))
        {
            track_memory = true;
            SF_TRACK_MEMORY_ON();
        }

        // Now see what is asked for.
        if (in("--generate-named"))
        {
            SF_THROW(std::invalid_argument{ "--generate-named not yet implemented" });
        }
        if (in("--test"))
        {
            if (in("--generate"))
            {
                SF_THROW(std::invalid_argument{ "--generate or --test but not both" });
            }
            if (not_in("--test-data"))
            {
                SF_THROW(std::invalid_argument{ "requires --test-data to be the test data directory" });
            }
            return sonic_field::run_tests(options["--test-data"]);
        }
        else if (in("--generate"))
        {
            sonic_field::time_it("Render", [] {
                    target();
                });
        }
        else
        {
            SF_THROW(std::invalid_argument{ "one of --generate or --test" });
        }
        SF_MARK_STACK;
    }
    catch (const std::exception& e)
    {
        std::cerr << "Exception during render: " << e.what() << std::endl;
        throw;
    }
    if (track_memory)
    {
        std::cout << "Memory tracking data:\n" 
                  << "=====================\n";
        sonic_field::clear_block_pool();
        SF_PRINT_TRACKED_MEMORY();
        SF_TRACK_MEMORY_OFF();
    }
    return 0;
}
