# CE059 native integration evidence and limits

Target: Windows x86 GTAIV.exe 1.2.0.59. The adapter is **experimental and statically verified**, not live-driving verified. The offset values below were read from the actual user-provided executable. Unknown versions are not scanned speculatively or accepted by version string alone.

The following hashes record the original analysis inputs only; they are not an executable allowlist.

## Exact files read

| File | Reported version | SHA-256 |
| --- | --- | --- |
| GTAIV.exe | 1.2.0.59 | 08759a5516f9837920ea504436236bbab89d0826a8e4d04ff106345177b5345d |
| dinput8.dll | 9.7.1 | 7675b24a2e408349c8a59b87e3ae8724ad8a89eff21c96a9c109650a3c1316e0 |
| plugins/GTAIV.EFLC.FusionFix.asi | 5.0.1 | 6c5ac36c9c6ca4aadb9be2fe4efc4ad8a51ca112673150491294cc9eeb18d7c2 |

No Rockstar executable bytes/files are redistributed in this package. The game executable is unchanged on disk. Five bytes of the game image are detoured during the process lifetime; when the verified LCP route is active, five bytes of its processGears image are also detoured. See LCP-COMPATIBILITY.md for the second ABI and evidence. Startup verifies file version 1.2.0.59 and critical in-memory instructions, with no whole-file hash check. Different bytes at the hook refuse installation. This does not establish compatibility with arbitrary future mods or updates.

## Provenance

The public [FusionFix 619f52d native wrappers](https://github.com/ThirteenAG/GTAIV.EFLC.FusionFix/blob/619f52d/source/natives.ixx) identify `GET_VEHICLE_ENGINE_REVS` and `GET_VEHICLE_GEAR`; their registration instructions in this EXE locate handlers BC64D0 and BC64F0. The implementations at BCBFB0/BCBFD0 independently confirm vehicle+1094 float revs and vehicle+1090 signed 16-bit gear.

[FusionFix comvars.ixx](https://github.com/ThirteenAG/GTAIV.EFLC.FusionFix/blob/619f52d/source/comvars.ixx) provided references for player lookup and pool structure. The relevant instructions were then read in this exact binary; the adapter uses guarded reads and only calls the verified text implementation for mode notifications. Some pool helper bodies are encrypted on disk, so they are deliberately not called or copied. Inline pool storage is corroborated by the loop at 5CFC61 computing `storage + slot*stride` then passing it as `this` to 5D27A0, which reads vehicle fields directly.

[Ultimate ASI Loader 2155f21](https://github.com/ThirteenAG/Ultimate-ASI-Loader/blob/2155f21/source/dllmain.cpp#L918) calls the `InitializeASI` export. That is now implemented. [MinHook v1.3.4](https://github.com/TsudaKageyu/minhook/tree/v1.3.4), commit c3fcafdc10146beb5919319d0683e44e3c30d537, is vendored with its license to construct the gateway and coordinate patching. No FusionFix or Manual Gearbox binary/code is bundled.

## Address and structure evidence

All VAs below use the EXE's preferred base 00400000. Code uses `loaded image base + RVA`, not fixed process addresses. **These values are specific to the CE059 layout. Changed game code/layout requires re-verification; unrelated whole-file differences are not rejected.**

| Evidence VA / member | Finding |
| --- | --- |
| C309D0 | Forward/reverse gear-selection routine; ECX is transmission subobject |
| C30B97, RVA 830B97 | 5-byte `divss xmm6,[esi+4C]`, after reverse handling and stock clutch update |
| C30B9C | Gateway resumes here after executing displaced DIVSS |
| C30C2E | Custom branch rejoins existing pop edi / pop esi / pop ecx / ret 18 epilogue |
| C30BE1..C30C2B | Stock forward shift changes gear, clutch value 0.1, flags 5/6 and last-shift timestamp |
| vehicle+1090 | Transmission subobject; +0 gear i16, +2 flags u16, +4 smoothed revs f32, +10 clutch f32, +18 timestamp u32 (member offsets hexadecimal) |
| vehicle+DC8 | Handling pointer, also present in ESI at the hook |
| handling+40 / +54+4*g / +4C | Gear count u8 / ratio[g] f32 / drive velocity scale f32, used by the original shift function |
| vehicle+1078 / +107C | Signed accelerator / brake signal read at C309DD..C30A12 |
| 8D20E0 | Local index [1036F14], player-info table [11A8808], ped at player-info+598 |
| 8D2110 | ped+26C bit2 indicates vehicle state; ped+B30 is vehicle |
| BCAE70 | vehicle+F50 is driver ped pointer |
| BCA830 | vehicle+2E signed model index; table [1295CD8]; modelInfo+3C is model hash |
| 12E22A4 | Vehicle pool pointer; +0 storage, +4 flags, +8 count, +C stride; free flag 80, generation lower bits |
| A493E0 | wheel array at vehicle+F80, count +F84, stride 170, contact flags +164 |
| 1173590 / 1173591 / 11735B4 | Pause bytes / game millisecond clock |

At the midhook, original stack +10 nominally contains the vehicle argument, **but vanilla writes a boolean to its high byte at +13**. The adapter therefore never reads that vehicle slot; it derives vehicle from ECX-1090 and cross-checks player ownership and handling. Stack +1C/+20 are average wheel speed and signed longitudinal speed, traced through the only direct caller C2FB3A in C2F9A0; all offsets here are hexadecimal.

The original native rev is clutch-smoothed and can reset to 0.9 at the limiter. In first gear (including ordinary launch slip), or once clutch >=0.99 in higher gears, controller RPM uses the same normalized mechanical signal as stock forward selection: `abs(wheel speed) * ratio[current] / drive velocity scale`. While clutch is reengaging in higher gears, the controller holds the gear and observes native revs. This is not physical RPM in revolutions per minute and does not add engine/clutch physics.

## Scope of control

The hook preserves all GPRs, EFLAGS, XMM0–7, x87 state and MXCSR. C++ runs with an empty x87 stack, default masked FP environment and clear direction flag. Every return restores original state. On pass-through the gateway executes the original DIVSS; on a valid custom decision it skips only stock forward gear selection.

Player ownership is rechecked before committing. The controller ID includes pool pointer, slot and generation. Only automobile type 0 is supported in this adapter. Bikes, aircraft and boats remain stock even though the portable controller has a motorcycle preset. Require all wheels' contact bit0, plausible input values and wheel/longitudinal speeds within max(3 m/s, 35%). This conservative contact/slip rule intentionally returns some valid driving situations to stock control. Unknown fields/ratios never receive fabricated defaults.

Only an actual adjacent shift writes the four fields the vanilla shift branch writes. Holding a gear changes no vehicle fields. There is no persistent stock-shift disable flag or handling modification: each invocation chooses independently. A fault, unsupported sample or missing callback cannot leave a per-vehicle suppression lease behind. The installed gate remains resident, pinned against unsafe hot unload, and forwards to vanilla whenever control is off. Exit the game before deleting/updating the ASI.

## Remaining live verification

Static signatures, mock-memory tests and CPU-state tests establish a basis for testing, not proof of gameplay stability. Still verify loader timing, real pool/ratio values, 30/60+ FPS, Windows/FusionFix coexistence, throttle sweeps, up/down/kickdown, brake/reverse, wheelspin, jumps, vehicle switch, driver exit/death, pause/reload and crash-free shutdown. Compare `ThrottleAT.csv` in observe and control modes. Lifecycle gaps resynchronize through a stock-controlled tick. Unacknowledged shifts or access failures latch stock fallback; F8 resets and retries. A changed callback thread requires a game restart. User driving logs were analyzed for earlier builds. No live drive of the new 0.3.0 integration has been verified.

## F8 notification revision

PRINT_STRING_WITH_LITERAL_STRING_NOW (native hash 0x0CA539D6) registers wrapper VA B8D0D0 in the inspected executable. Its four arguments are loaded from context+8 and passed to cdecl implementation B90A00 (RVA 790A00). The wrapper and implementation entry bytes are checked independently before enabling notifications. The implementation resolves the STRING GXT key and queues the literal message for 3000 ms; it is called from the existing game-thread callback. No script context, .NET or ScriptHook is required. A missing notification signature disables text only. Display is a game subtitle message, may interact with mission subtitles, and was reported visible by the user for the earlier build; the new ACTIVE transition still requires live verification. Tests validate message selection and arguments with a mock, not game rendering.

## 0.3.0 LCP route

The supplied 0.2.3 runtime log proves the automobile engine call targets LCP+E6D0. Startup now accepts either the original engine call or that specifically verified LCP layout; other routes fail closed. LCP forward decisions are hooked at EBCB and custom decisions rejoin EC70. The original game hook remains for paths that still use it. No entry observer hooks are installed in 0.3.0. OFF/fallback executes the selected original game or LCP branch. See [LCP evidence](LCP-COMPATIBILITY.md) for signatures, register/frame layout, load-order limitation and validation.
