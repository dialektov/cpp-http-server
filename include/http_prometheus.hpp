#pragma once

#include "http_metrics.hpp"

#include <string>

std::string format_prometheus_metrics(const ServerStats& stats);
