# Validation — ThrottleAT 0.4.0 CE059 / LCP experimental

Toolchain: Visual Studio 2022 Build Tools, MSVC Win32 Release, static C/C++ runtime. The revised source was built and all seven suites passed before publishing.

Seven CTest suites:

- controller_and_fallback: 38 checks covering maps, kickdown, hysteresis, configuration, fallback, and gear acknowledgment independent of slow clutch recovery.
- ce059_bridge: 41 checks using allocated synthetic vehicle/player/pool/handling memory. Includes actual adapter writes, player ownership, lifecycle gaps, notifications, LCP frame translation, early first upshift with launch clutch slip, unchanged native revs, and missing-frame pass-through, plus active Motorcycle/Faggio control and unverified-route/contact/ownership rejection.
- x86_gate_cpu_state: four vanilla midhook paths, preserving arguments, stack, GPRs, EFLAGS, XMM, x87 and MXCSR.
- x86_trace_cpu_state: four diagnostic entry-gate paths. These entry observers are retained in source but not installed by 0.3.0.
- x86_lcp_cpu_state: four LCP midhook paths with pass-through/custom branches and initial direction flag clear/set.
- throttle_schedule_scenarios: representative 30/60/144 Hz driving sequences, early Sultan-tuned shifts through fifth, no hunting, full-throttle hold, kickdown, braking, lift-off and new config validation.
- asi_lifecycle: the actual ASI loads outside GTA IV, rejects the unsupported host version, handles lifecycle calls, and unloads safely.

The reference upload was read without executing it. Its five LCP instruction windows and processGears call target match the runtime verification code. An independent review found no blocking ABI or control-write issue.

The packaged 0.3.0 ASI was PE32/x86, importing only KERNEL32.dll, USER32.dll and VERSION.dll. No CLR, ScriptHook or ScriptHookDotNet dependency. This repository contains source, not prebuilt binaries.

Earlier user driving logs establish the LCP route conflict; they do not validate the new integration. Actual 0.4.0 driving, LVS display RPM, 30/60+ FPS behavior, load/pause/vehicle changes, and crash-free gameplay remain unverified. The mock and CPU-state tests do not prove in-game compatibility. See LCP-COMPATIBILITY.md and CE059-EVIDENCE.md for exact evidence and limits.
