// CE059 engine ratio gates. See docs/CVT-050-EVIDENCE.md.
#include <cstdint>
extern "C" int __cdecl atCvtRatio(unsigned site, std::uintptr_t transmission,
                                 std::uintptr_t handling, float* ratio) noexcept;
extern "C" void* atCvtRpmGateway = nullptr;
extern "C" void* atCvtRpmContinue = nullptr;
extern "C" __declspec(naked) void atCvtRpmGate() {
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
        lea eax, [esp+540]
        mov ecx, [esp+544]
        mov edx, [esp+564]
        push eax
        push edx
        push ecx
        push 0
        call atCvtRatio
        add esp, 16
        mov [esp+536], eax
        mov eax, [esp+532]
        fxrstor [eax]
        cmp dword ptr [esp+536], 0
        je fallback
        movss xmm6, dword ptr [esp+540]
        add esp, 544
        popad
        popfd
        jmp dword ptr [atCvtRpmContinue]
    fallback:
        add esp, 544
        popad
        popfd
        jmp dword ptr [atCvtRpmGateway]
    }
}
extern "C" void* atCvtLimitGateway = nullptr;
extern "C" void* atCvtLimitContinue = nullptr;
extern "C" __declspec(naked) void atCvtLimitGate() {
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
        lea eax, [esp+540]
        mov ecx, [esp+544]
        mov edx, [esp+564]
        push eax
        push edx
        push ecx
        push 1
        call atCvtRatio
        add esp, 16
        mov [esp+536], eax
        mov eax, [esp+532]
        fxrstor [eax]
        cmp dword ptr [esp+536], 0
        je fallback
        movss xmm0, dword ptr [esp+540]
        add esp, 544
        popad
        popfd
        jmp dword ptr [atCvtLimitContinue]
    fallback:
        add esp, 544
        popad
        popfd
        jmp dword ptr [atCvtLimitGateway]
    }
}
extern "C" void* atCvtTorqueGateway = nullptr;
extern "C" void* atCvtTorqueContinue = nullptr;
extern "C" __declspec(naked) void atCvtTorqueGate() {
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
        lea eax, [esp+540]
        mov ecx, [esp+544]
        mov edx, [esp+564]
        push eax
        push edx
        push ecx
        push 2
        call atCvtRatio
        add esp, 16
        mov [esp+536], eax
        mov eax, [esp+532]
        fxrstor [eax]
        cmp dword ptr [esp+536], 0
        je fallback
        mulss xmm1, dword ptr [esp+540]
        add esp, 544
        popad
        popfd
        jmp dword ptr [atCvtTorqueContinue]
    fallback:
        add esp, 544
        popad
        popfd
        jmp dword ptr [atCvtTorqueGateway]
    }
}
