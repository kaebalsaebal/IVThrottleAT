// Diagnostic entry gates: always run the original function afterward.
#include <cstdint>
extern "C" void* atShiftEntryGateway=nullptr;
extern "C" void* atUpdateEntryGateway=nullptr;
extern "C" void __cdecl atTraceEntry(unsigned kind, std::uintptr_t transmission, const std::uint32_t* stack) noexcept;
extern "C" __declspec(naked) void atShiftEntryGate() {
    __asm {
        pushfd
        pushad
        sub esp, 544
        lea eax, [esp+15]
        and eax, -16
        mov [esp+532], eax
        fxsave [eax]
        fninit
        mov dword ptr [esp+528], 1f80h
        ldmxcsr [esp+528]
        cld
        lea eax, [esp+580]
        mov ecx, [esp+568]
        push eax
        push ecx
        push 0
        call atTraceEntry
        add esp, 12
        mov eax, [esp+532]
        fxrstor [eax]
        add esp, 544
        popad
        popfd
        jmp dword ptr [atShiftEntryGateway]
    }
}
extern "C" __declspec(naked) void atUpdateEntryGate() {
    __asm {
        pushfd
        pushad
        sub esp, 544
        lea eax, [esp+15]
        and eax, -16
        mov [esp+532], eax
        fxsave [eax]
        fninit
        mov dword ptr [esp+528], 1f80h
        ldmxcsr [esp+528]
        cld
        lea eax, [esp+580]
        mov ecx, [esp+568]
        push eax
        push ecx
        push 1
        call atTraceEntry
        add esp, 12
        mov eax, [esp+532]
        fxrstor [eax]
        add esp, 544
        popad
        popfd
        jmp dword ptr [atUpdateEntryGateway]
    }
}
