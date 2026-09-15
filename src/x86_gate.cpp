// MSVC x86 only. Exact mid-function ABI is documented in docs/CE059-EVIDENCE.md.
// No assumptions about general-purpose, x87 or SSE scratch registers at this site.
#include <cstdint>
extern "C" void* atGateway = nullptr;
extern "C" void* atSkipTarget = nullptr;
extern "C" int __cdecl atDispatch(std::uintptr_t transmission,
                                  std::uintptr_t handling,
                                  const std::uint32_t* originalStack) noexcept;
extern "C" __declspec(naked) void atGate() {
    __asm {
        pushfd
        pushad
        sub esp, 544
        lea eax, [esp+15]
        and eax, -16
        mov [esp+532], eax
        fxsave [eax]
        // C++ must run with a clean x87 stack and masked/default FP environment.
        fninit
        mov dword ptr [esp+528], 1f80h
        ldmxcsr [esp+528]
        cld
        lea eax, [esp+580] // 544 reserved + 32 PUSHAD + 4 PUSHFD
        mov ecx, [esp+568] // saved ECX, transmission
        mov edx, [esp+548] // saved ESI, handling
        push eax
        push edx
        push ecx
        call atDispatch
        add esp, 12
        mov [esp+536], eax
        mov eax, [esp+532]
        fxrstor [eax]
        cmp dword ptr [esp+536], 0
        je vanilla
        add esp, 544
        popad
        popfd
        jmp dword ptr [atSkipTarget]
    vanilla:
        add esp, 544
        popad
        popfd
        jmp dword ptr [atGateway]
    }
}
