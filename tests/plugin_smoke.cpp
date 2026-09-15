#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <iostream>
int main(int argc, char** argv) {
    if (argc != 2) return 1;
    HMODULE mod = LoadLibraryA(argv[1]);
    if (!mod) { std::cerr << "LoadLibrary failed: " << GetLastError() << '\n'; return 2; }
    using Init = int (__cdecl*)();
    using Call = void (__cdecl*)();
    auto init = reinterpret_cast<Init>(GetProcAddress(mod,"AT_Initialize"));
    auto tick = reinterpret_cast<Call>(GetProcAddress(mod,"AT_Tick"));
    auto stop = reinterpret_cast<Call>(GetProcAddress(mod,"AT_Shutdown"));
    if (!init || !tick || !stop) { FreeLibrary(mod); return 3; }
    tick(); // Before initialization is harmless.
    const int ready = init();
    for (int i=0; i<100; ++i) tick();
    stop(); stop(); tick();
    const bool unloaded = FreeLibrary(mod) != FALSE;
    if (ready != 0 || !unloaded) return 4;
    std::cout << "ASI loads, remains unavailable, ticks safely and unloads\n";
    return 0;
}
