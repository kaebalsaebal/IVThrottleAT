// MSVC x86 mid-function ABI for the inspected LibertyCityPlates transmission.
// Module RVA 0xEBCB: EDI = transmission, EDX = handling, EBP = function frame.
// Forward decision skip target RVA 0xEC70 retains the original RET 0x18 epilogue.
// This gate must never be attached to the stock GTA IV shift function.
#include <cstdint>
extern "C" void* atLcpGateway = nullptr;
extern "C" void* atLcpSkipTarget = nullptr;
extern "C" int __cdecl atLcpDispatch(std::uintptr_t transmission,
                                     std::uintptr_t handling,
                                     const std::uint32_t* frame) noexcept;
extern "C" __declspec(naked) void atLcpGate() {
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
        // PUSHAD saves EDI, ESI, EBP, ESP, EBX, EDX, ECX, EAX in this order
        // from its final ESP; the floating-point save space adds 544 bytes.
        mov eax, [esp+552] // saved EBP, gear-function frame
        mov ecx, [esp+544] // saved EDI, transmission
        mov edx, [esp+564] // saved EDX, handling
        push eax
        push edx
        push ecx
        call atLcpDispatch
        add esp, 12
        mov [esp+536], eax
        mov eax, [esp+532]
        fxrstor [eax]
        cmp dword ptr [esp+536], 0
        je originalDecision
        add esp, 544
        popad
        popfd
        jmp dword ptr [atLcpSkipTarget]
    originalDecision:
        add esp, 544
        popad
        popfd
        jmp dword ptr [atLcpGateway]
    }
}