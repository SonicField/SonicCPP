#pragma once
#include "../sonic_field.h"

namespace sonic_field
{
    signal generate_rich_base(uint64_t length, double pitch);
    signal generate_windy_base(uint64_t length, double pitch);
}
