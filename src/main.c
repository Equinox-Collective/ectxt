#include "libc/stdio.h"
#include "libc/string.h"

#define STD_OUTPUT_HANDLE ((uint32_t)-11)

__declspec(dllimport) void * __stdcall GetStdHandle(uint32_t nStdHandle);
__declspec(dllimport) int    __stdcall WriteFile(void *hFile, const void *lpBuffer, uint32_t nNumberOfBytesToWrite, uint32_t *lpNumberOfBytesWritten, void *lpOverlapped);
__declspec(dllimport) void   __stdcall ExitProcess(uint32_t uExitCode);

static void *h_stdout;

void term_print(const char *str) {
    uint32_t written;
    WriteFile(h_stdout, str, (uint32_t)strlen(str), &written, (void *)0);
}

int main(void) {
    h_stdout = GetStdHandle(STD_OUTPUT_HANDLE);
    if (!h_stdout) {
        return 1;
    }

    printf("ECTXT Core Initialized. Freestanding mode: ACTIVE.\n");
    printf("Platform: Windows x86_64 | Native NT Subsystem.\n");

    return 0;
}

void mainCRTStartup(void) {
    int ret = main();
    ExitProcess((uint32_t)ret);
}