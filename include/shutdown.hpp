#pragma once

#include <atomic>

void install_shutdown_handlers();
bool shutdown_requested();
void request_shutdown();
