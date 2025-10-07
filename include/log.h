#pragma once

#include <signal.h>

#ifdef DEBUG
#define DEBUG_MSG printf
#else
#define DEBUG_MSG(...) do {} while(0)
#endif

#define PANIC_MSG(...) do { printf(__VA_ARGS__); printf("Emulator paniced by PANIC_MSG\n"); raise(SIGINT);} while(0)
