#include <iostream>

#include <windows.h>

BOOL WINAPI MyHandlerRoutine(DWORD dwCtrlType) {
	TerminateProcess(GetCurrentProcess(), 2);
	return TRUE;
}

static FILE* f1 = NULL;
static FILE* f2 = NULL;
static FILE* f3 = NULL;

static void AllocateConsole() {
    AllocConsole();
    SetConsoleCtrlHandler(MyHandlerRoutine, TRUE);
    freopen_s(&f1, "CONIN$",  "r", stdin);
    freopen_s(&f2, "CONOUT$", "w", stdout);
    freopen_s(&f3, "CONOUT$", "w", stderr);
}

static void DeallocateConsole() {
    fclose(f1);
    fclose(f2);
    fclose(f3);
    FreeConsole();
}


BOOL WINAPI DllMain(HINSTANCE hModule, DWORD fdwReason, LPVOID lpvReserved) {
    switch (fdwReason) {
        case DLL_PROCESS_ATTACH: {
            AllocateConsole();

            std::cout << "DllMain (DLL_PROCESS_ATTACH) called!" << std::endl;
            /* Init */
            break;
        }
        case DLL_PROCESS_DETACH: {
            std::cout << "DllMain (DLL_PROCESS_DETACH) called!" << std::endl;
            /* Deinit */

            DeallocateConsole();
            break;
        }
        default: {
            break;
        }
    }

    return TRUE;
}