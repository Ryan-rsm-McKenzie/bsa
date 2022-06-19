#pragma once

#pragma push_macro("max")
#pragma push_macro("min")

#undef max
#undef min

#include <catch2/catch_all.hpp>

#pragma pop_macro("max")
#pragma pop_macro("min")
