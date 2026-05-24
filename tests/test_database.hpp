#pragma once

#include <cstdlib>
#include <string>

#include <doctest/doctest.h>

std::string test_database_url();
bool database_available();

#define SKIP_IF_NO_PG()                                                \
    do {                                                               \
        if (!database_available()) {                                   \
            WARN("PostgreSQL not available — skipping test");            \
            return;                                                    \
        }                                                              \
    } while (0)
