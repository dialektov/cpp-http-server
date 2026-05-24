#include "shutdown.hpp"

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#else
#include <csignal>
#endif

namespace {

std::atomic<bool> g_shutdown_requested{false};

#ifdef _WIN32
BOOL WINAPI console_ctrl_handler(DWORD ctrl_type) {
    if (ctrl_type == CTRL_C_EVENT || ctrl_type == CTRL_BREAK_EVENT || ctrl_type == CTRL_CLOSE_EVENT) {
        g_shutdown_requested.store(true, std::memory_order_relaxed);
        return TRUE;
    }
    return FALSE;
}
#else
void signal_handler(int) {
    g_shutdown_requested.store(true, std::memory_order_relaxed);
}
#endif

}  // namespace

void install_shutdown_handlers() {
#ifdef _WIN32
    SetConsoleCtrlHandler(console_ctrl_handler, TRUE);
#else
    std::signal(SIGINT, signal_handler);
    std::signal(SIGTERM, signal_handler);
#endif
}

bool shutdown_requested() {
    return g_shutdown_requested.load(std::memory_order_relaxed);
}

void request_shutdown() {
    g_shutdown_requested.store(true, std::memory_order_relaxed);
}
