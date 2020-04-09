#pragma once

#include <eosio/symbol.hpp>

using namespace eosio;
using std::string;

namespace common {

    static const symbol         S_REWARD                        ("REWARD", 2);
    static const symbol         S_VOTE                          ("VOTE", 2);
    static const symbol         S_USD                           ("USD", 2);
   
    static const uint64_t       NO_ASSIGNMENT                   = -1;         

    static const uint64_t       MICROSECONDS_PER_HOUR   = (uint64_t)60 * (uint64_t)60 * (uint64_t)1000000;
    static const uint64_t       MICROSECONDS_PER_YEAR   = MICROSECONDS_PER_HOUR * (uint64_t)24 * (uint64_t)365;

    static const float          WEEK_TO_YEAR_RATIO     = (float) ((float)52 / (float)365.25);

};