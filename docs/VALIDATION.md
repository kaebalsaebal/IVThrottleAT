# Validation — ThrottleAT 0.5.1-test CE059 / LCP experimental

Toolchain: Visual Studio 2022 Build Tools, MSVC Win32 Release, static C/C++ runtime. The revised source was built and all nine suites passed before publishing.

Nine CTest suites:

- controller_and_fallback: checks covering maps, kickdown, hysteresis, configuration, fallback, and gear acknowledgment independent of slow clutch recovery.
- ce059_bridge: checks using allocated synthetic vehicle/player/pool/handling memory. Includes actual adapter writes, player ownership, lifecycle gaps, notifications, LCP frame translation, early first upshift with launch clutch slip, unchanged native revs, and missing-frame pass-through, plus active Motorcycle/Faggio control and unverified-route/contact/ownership rejection.
- x86_gate_cpu_state: four vanilla midhook paths, preserving arguments, stack, GPRs, EFLAGS, XMM, x87 and MXCSR.
- x86_trace_cpu_state: four diagnostic entry-gate paths. These entry observers are retained in source but not installed by 0.3.0.
- x86_lcp_cpu_state: four LCP midhook paths with pass-through/custom branches and initial direction flag clear/set.
- throttle_schedule_scenarios: representative 30/60/144 Hz driving sequences, early common Passenger shifts through fifth, no hunting, full-throttle hold, kickdown, braking, lift-off and new config validation.
- asi_lifecycle: the actual ASI loads outside GTA IV, rejects the unsupported host version, handles lifecycle calls, and unloads safely.

The reference upload was read without executing it. Its five LCP instruction windows and processGears call target match the runtime verification code. An independent review found no blocking ABI or control-write issue.

The packaged 0.5.1-test ASI was PE32/x86, importing only KERNEL32.dll, USER32.dll and VERSION.dll. No CLR, ScriptHook or ScriptHookDotNet dependency. This repository contains source, not prebuilt binaries.

Earlier user driving logs establish the LCP route conflict; they do not validate the new integration. Actual 0.5.1-test driving, LVS display RPM, 30/60+ FPS behavior, load/pause/vehicle changes, and crash-free gameplay remain unverified. The mock and CPU-state tests do not prove in-game compatibility. See LCP-COMPATIBILITY.md and CE059-EVIDENCE.md for exact evidence and limits.

- motorcycle_classes_and_cvt: four-class mapping and overrides, legacy configuration, CVT validation, ratio limits/rate, convergence at 30/60/144 Hz and reset/fallback conditions.
- x86_cvt_cpu_state: twelve paths across RPM, limiter and torque gates with custom/original operands and both initial direction-flag states. Scalar SSE results and preserved state are checked.

ce059_bridge additionally exercises dispatch-to-engine CVT lease consumption, optional limiter, matching ratios in engine paths, unchanged handling and integer gear, identity/time/OFF rejection and Scooter stepped fallback. The five CVT runtime signature windows and call target match the read-only reference executable. See CVT-050-EVIDENCE.md. These checks cannot establish stable in-game CVT behavior.

0.5.1 adds native clutch-blend equilibrium, intermediate pedal sweeps, jitter and immediate CVT re-entry/higher-gear slip regression tests; all nine suites passed. User reports of 0.5.0 behavior motivated this correction but do not validate 0.5.1.
