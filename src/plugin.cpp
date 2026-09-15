#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#ifdef AT_CE059
#include "ce059.hpp"
#endif
namespace { HMODULE module=nullptr; }
extern "C" __declspec(dllexport) int __cdecl AT_Initialize() noexcept {
#ifdef AT_CE059
    const auto status=ce059::initialize(module);
    return status==1||status==2 ? 1 : 0;
#else
    return 0;
#endif
}
// Actual Ultimate ASI Loader startup contract, verified at UAL 2155f21.
extern "C" __declspec(dllexport) void __cdecl InitializeASI() noexcept { AT_Initialize(); }
extern "C" __declspec(dllexport) void __cdecl AT_Tick() noexcept {
    // CE059 invokes the controller inside its transmission hook; no worker polling.
}
extern "C" __declspec(dllexport) int __cdecl AT_Status() noexcept {
#ifdef AT_CE059
    return ce059::status();
#else
    return 0;
#endif
}
extern "C" __declspec(dllexport) void __cdecl AT_Shutdown() noexcept {
#ifdef AT_CE059
    ce059::deactivate();
#endif
}
BOOL WINAPI DllMain(HINSTANCE instance,DWORD reason,LPVOID) {
    if(reason==DLL_PROCESS_ATTACH) module=instance;
    return TRUE;
}