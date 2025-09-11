#include "config.h"

Config &Config::instance()
{
    static Config cfg;
    return cfg;
}

