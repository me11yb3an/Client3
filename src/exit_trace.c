/*
 * Build A-only diagnostic interposer target. Linked only when
 * --wrap=exit is enabled; prints the call stack before delegating to libc.
 */
#define _GNU_SOURCE
#include <execinfo.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

void __real_exit(int status);

void __wrap_exit(int status) {
    static int tracing;
    if (!tracing++) {
        void *frames[48];
        int count = backtrace(frames, (int)(sizeof(frames) / sizeof(frames[0])));
        fprintf(stderr, "BUILD_A_EXIT_TRACE status=%d pid=%ld\n", status, (long)getpid());
        backtrace_symbols_fd(frames, count, STDERR_FILENO);
        fflush(stderr);
    }
    __real_exit(status);
}
