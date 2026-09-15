// Synthetic MSVC/x86 test of the actual production CVT ratio gates, without GTA IV or a
// hook library. The two landing pads stand in for the trampoline and skip target.
// The dispatcher intentionally violates even the callee-saved register rules:
// the gate promises to preserve the complete interrupted machine state.
#include <array>
#include <cstdint>
#include <cstdio>
#include <cstring>

#if !defined(_MSC_VER) || !defined(_M_IX86)
#error This CPU-state harness requires MSVC-compatible 32-bit x86 inline assembly.
#endif

extern "C" void* atCvtRpmGateway;
extern "C" void* atCvtRpmContinue;
extern "C" void atCvtRpmGate();
extern "C" void* atCvtLimitGateway;
extern "C" void* atCvtLimitContinue;
extern "C" void atCvtLimitGate();
extern "C" void* atCvtTorqueGateway;
extern "C" void* atCvtTorqueContinue;
extern "C" void atCvtTorqueGate();

namespace {
struct Capture {
    std::uint32_t eax, ebx, ecx, edx, esi, edi, ebp, esp, flags;
};
static_assert(sizeof(Capture) == 36);
alignas(16) unsigned char callerFx[512]{};
alignas(16) unsigned char expectedFx[512]{};
alignas(16) unsigned char actualFx[512]{};
alignas(16) unsigned char dispatchFx[512]{};
alignas(16) const std::uint32_t seedXmm[32] = {
    0x10111213, 0x14151617, 0x18191a1b, 0x1c1d1e1f,
    0x20212223, 0x24252627, 0x28292a2b, 0x2c2d2e2f,
    0x30313233, 0x34353637, 0x38393a3b, 0x3c3d3e3f,
    0x40414243, 0x44454647, 0x48494a4b, 0x4c4d4e4f,
    0x50515253, 0x54555657, 0x58595a5b, 0x5c5d5e5f,
    0x60616263, 0x64656667, 0x68696a6b, 0x6c6d6e6f,
    0x70717273, 0x74757677, 0x78797a7b, 0x7c7d7e7f,
    0x80818283, 0x84858687, 0x88898a8b, 0x8c8d8e8f
};
std::uint32_t savedHarnessEsp{}, expectedEsp{}, expectedFlags{}, seedFlags{};
std::uint32_t dispatchFlags{}, dispatchTransmission{}, dispatchHandling{};
std::uint32_t dispatchFramePointer{}, actualStack[10]{};
std::uint32_t decision{}, landing{}, selectedSite{}, dispatchSite{};
void* selectedGate=nullptr;
Capture actual{};
const std::uint16_t seedControl = 0x077f; // Nondefault x87 rounding.
const std::uint16_t clobberControl = 0x0f7f;
const std::uint32_t seedMxcsr = 0x3f80; // Nondefault SSE rounding.
const std::uint32_t clobberMxcsr = 0x7f80;

// No C/C++ code executes while the seeded DF or synthetic EBP is live. Capture
// first, restore the harness caller's complete state, and only then return.
__declspec(naked) void captureLanding() {
    __asm {
        mov dword ptr [actual], eax
        mov dword ptr [actual+4], ebx
        mov dword ptr [actual+8], ecx
        mov dword ptr [actual+12], edx
        mov dword ptr [actual+16], esi
        mov dword ptr [actual+20], edi
        mov dword ptr [actual+24], ebp
        mov dword ptr [actual+28], esp
        pushfd
        pop dword ptr [actual+32]
        fxsave [actualFx]
        mov eax, [esp]
        mov [actualStack], eax
        mov eax, [esp+4]
        mov [actualStack+4], eax
        mov eax, [esp+8]
        mov [actualStack+8], eax
        mov eax, [esp+12]
        mov [actualStack+12], eax
        mov eax, [esp+16]
        mov [actualStack+16], eax
        mov eax, [esp+20]
        mov [actualStack+20], eax
        mov eax, [esp+24]
        mov [actualStack+24], eax
        mov eax, [esp+28]
        mov [actualStack+28], eax
        mov eax, [esp+32]
        mov [actualStack+32], eax
        mov eax, [esp+36]
        mov [actualStack+36], eax
        cld
        fxrstor [callerFx]
        mov esp, [savedHarnessEsp]
        popad
        popfd
        ret
    }
}
__declspec(naked) void fallbackLanding() {
    __asm {
        mov dword ptr [landing], 1
        jmp captureLanding
    }
}
__declspec(naked) void skipLanding() {
    __asm {
        mov dword ptr [landing], 2
        jmp captureLanding
    }
}

__declspec(naked) void runGate() {
    __asm {
        pushfd
        pushad
        fxsave [callerFx]
        mov [savedHarnessEsp], esp
        // A synthetic game stack. Entry is by JMP, exactly like a midhook.
        sub esp, 40
        mov [expectedEsp], esp
        mov dword ptr [esp], 0a0000000h
        mov dword ptr [esp+4], 0a0000001h
        mov dword ptr [esp+8], 0a0000002h
        mov dword ptr [esp+12], 0a0000003h
        mov dword ptr [esp+16], 0a0000004h
        mov dword ptr [esp+20], 0a0000005h
        mov dword ptr [esp+24], 0a0000006h
        mov dword ptr [esp+28], 0a0000007h
        mov dword ptr [esp+32], 0a0000008h
        mov dword ptr [esp+36], 0a0000009h
        fninit
        fldcw [seedControl]
        fld1
        fldpi
        fldz
        ldmxcsr [seedMxcsr]
        movdqa xmm0, xmmword ptr [seedXmm]
        movdqa xmm1, xmmword ptr [seedXmm+16]
        movdqa xmm2, xmmword ptr [seedXmm+32]
        movdqa xmm3, xmmword ptr [seedXmm+48]
        movdqa xmm4, xmmword ptr [seedXmm+64]
        movdqa xmm5, xmmword ptr [seedXmm+80]
        movdqa xmm6, xmmword ptr [seedXmm+96]
        movdqa xmm7, xmmword ptr [seedXmm+112]
        fxsave [expectedFx]
        // Change only user arithmetic flags and DF, leaving privileged bits
        // alone. Record what POPFD actually accepted on this processor.
        pushfd
        pop eax
        and eax, 0fffff32ah
        or eax, [seedFlags]
        push eax
        popfd
        pushfd
        pop [expectedFlags]
        mov eax, 01010101h
        mov ebx, 02020202h
        mov ecx, 03030303h
        mov edx, 04040404h
        mov esi, 05050505h
        mov edi, 06060606h
        mov ebp, 07070707h
        jmp dword ptr [selectedGate]
    }
}

int failures = 0;
void check(bool condition, const char* name) {
    if (!condition) {
        std::printf("FAIL decision=%u DF=%u: %s\n", decision,
                    (seedFlags >> 10) & 1u, name);
        ++failures;
    }
}
std::uint16_t word(const unsigned char* bytes, unsigned offset) {
    std::uint16_t value;
    std::memcpy(&value, bytes + offset, sizeof value);
    return value;
}
std::uint32_t dword(const unsigned char* bytes, unsigned offset) {
    std::uint32_t value;
    std::memcpy(&value, bytes + offset, sizeof value);
    return value;
}
} // namespace

// This symbol replaces the real backend solely in the test executable.
// The synthetic EBP is captured as a value and is deliberately never dereferenced.
// Save the callback's initial environment before clobbering it. This also
// validates that the gate gave C++ an empty x87 stack, masked FP and clear DF.
extern "C" __declspec(naked) int __cdecl atCvtRatio(
    unsigned, std::uintptr_t, std::uintptr_t, float*) noexcept {
    __asm {
        pushfd
        pop [dispatchFlags]
        fxsave [dispatchFx]
        mov eax, [esp+4]
        mov [dispatchSite], eax
        mov eax, [esp+8]
        mov [dispatchTransmission], eax
        mov eax, [esp+12]
        mov [dispatchHandling], eax
        mov eax, [esp+16]
        mov [dispatchFramePointer], eax
        mov dword ptr [eax], 40000000h
        fninit
        fldcw [clobberControl]
        fldl2e
        fldln2
        ldmxcsr [clobberMxcsr]
        pxor xmm0, xmm0
        pxor xmm1, xmm1
        pxor xmm2, xmm2
        pxor xmm3, xmm3
        pxor xmm4, xmm4
        pxor xmm5, xmm5
        pxor xmm6, xmm6
        pxor xmm7, xmm7
        mov ebx, 0bbbbbbbbh
        mov ecx, 0cccccccch
        mov edx, 0ddddddddh
        mov esi, 0eeeeeeeeh
        mov edi, 0ffffffffh
        mov ebp, 099999999h
        xor eax, eax
        std
        mov eax, [decision]
        ret
    }
}

int main() {
    atCvtRpmGateway=atCvtLimitGateway=atCvtTorqueGateway=reinterpret_cast<void*>(&fallbackLanding);
    atCvtRpmContinue=atCvtLimitContinue=atCvtTorqueContinue=reinterpret_cast<void*>(&skipLanding);
    void* gates[]={reinterpret_cast<void*>(&atCvtRpmGate),reinterpret_cast<void*>(&atCvtLimitGate),reinterpret_cast<void*>(&atCvtTorqueGate)};
    for(selectedSite=0;selectedSite<3;++selectedSite) {
    selectedGate=gates[selectedSite];
    for (decision = 0; decision != 2; ++decision) {
        for (unsigned df = 0; df != 2; ++df) {
            seedFlags = 0x8d5u | (df << 10);
            landing = 0;
            runGate();
            if(decision) {
                const unsigned reg=selectedSite==0?6:selectedSite==1?0:1;
                float expected=2.0f;
                if(selectedSite==2) {std::memcpy(&expected,expectedFx+160+16*reg,4);expected*=2.0f;}
                else std::memset(expectedFx+160+16*reg,0,16); // MOVSS m32 zeros upper 96 bits.
                std::memcpy(expectedFx+160+16*reg,&expected,4);
            }
            check(dispatchSite==selectedSite,"correct scalar instruction site");
            check(landing == decision + 1, "correct fallback/skip destination");
            check(actual.eax == 0x01010101 && actual.ebx == 0x02020202 &&
                  actual.ecx == 0x03030303 && actual.edx == 0x04040404 &&
                  actual.esi == 0x05050505 && actual.edi == 0x06060606 &&
                  actual.ebp == 0x07070707, "all seven general-purpose registers");
            check(actual.esp == expectedEsp, "original ESP");
            check(actual.flags == expectedFlags, "complete observable EFLAGS");
            check(dispatchTransmission == 0x06060606 &&
                  dispatchHandling == 0x04040404, "dispatcher EDI/EDX arguments");
            check(dispatchFramePointer == expectedEsp-40, "ratio output uses reserved stack outside FXSAVE");
            for (unsigned i = 0; i != 10; ++i) {
                check(actualStack[i] == 0xa0000000u + i, "original stack remains intact");
            }
            check((dispatchFlags & 0x400) == 0, "dispatcher enters with clear DF");
            check(word(dispatchFx, 0) == 0x037f && word(dispatchFx, 2) == 0 &&
                  dispatchFx[4] == 0, "dispatcher enters with default, empty x87");
            check(dword(dispatchFx, 24) == 0x1f80, "dispatcher enters with default MXCSR");
            check(word(actualFx, 0) == word(expectedFx, 0), "x87 control word");
            check(word(actualFx, 2) == word(expectedFx, 2), "x87 status word and TOP");
            check(actualFx[4] == expectedFx[4], "x87 abridged tag word");
            check(dword(actualFx, 24) == dword(expectedFx, 24), "MXCSR including rounding");
            // Compare architectural values, excluding reserved save-area bytes
            // and FIP/FDP fields whose reporting differs across processors.
            for (unsigned i = 0; i != 3; ++i)
                check(std::memcmp(actualFx + 32 + 16*i,
                                  expectedFx + 32 + 16*i, 10) == 0,
                      "live x87 80-bit stack register");
            for (unsigned i = 0; i != 8; ++i)
                check(std::memcmp(actualFx + 160 + 16*i,
                                  expectedFx + 160 + 16*i, 16) == 0,
                      "complete 128-bit XMM register");
        }
    }
    }
    if (failures) {
        std::printf("%d CVT x86 gate checks failed.\n", failures);
        return 1;
    }
    std::puts("PASS: 12 CVT x86 gate paths preserve stack, GPRs, EFLAGS, XMM, x87 and MXCSR.");
    return 0;
}
