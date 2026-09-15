> Current 0.5.2 persistence policy supersedes the historical fallback descriptions below; see [CHANGES-052.md](CHANGES-052.md). Engine hook addresses and CVT target curve remain unchanged.

# 0.5.0-test motorcycle presets and experimental CVT

## Scope and classification

Cars retain 0.4.1 settings, including no Sultan override. Motorcycle type is checked before model classification. Known names are registered even when episode IDE discovery is incomplete.

- SportBike: AKUMA, BATI, BATI2, HAKUCHOU, HAKUCHOU2, NRG900, DOUBLE, DOUBLE2.
- CruiserBike: FREEWAY, ANGEL, DAEMON, DIABOLUS, HELLFURY, HEXER, LYCAN, NIGHTBLADE, REVENANT, WOLFSBANE, ZOMBIE.
- Scooter: FAGGIO.
- StandardBike: all remaining bikes, including PCJ, VADER, SANCHEZ.

These are tuning categories, not a claim about real motorcycle specifications. `[Model:NAME] BikeClass=SportBike|CruiserBike|StandardBike|Scooter` can override classification for actual bike types. Legacy Motorcycle overrides precede specific class and model overrides. See the distributed INI for complete presets.

## Continuous ratio controller

Scooter Cvt=1 enables the attempt when runtime engine checks pass. In 0.5.1-test, filtered pedal x follows an exponential response (CvtResponse=.25 seconds rising, 1.5 times that falling). Target is a quadratic Bezier with endpoints CvtLow=.20 and CvtHigh=.82; the middle control point is clamp(2*CvtMid-.5*(low+high),low,high), default CvtMid=.44. This yields a monotone continuous curve with no middle-throttle slope break. Extreme custom mid values are constrained to maintain monotonicity.

Static original CE059 instructions at C2FE98..C2FEBE blend `(1-clutch)*absGas + clutch*mechanicalRevs`. The revised ratio request inverts this blend: mechanical=max(0,(target-(1-clutch)*rawPedal)/clutch), then ratio=mechanical*handlingVelocity/wheelSpeed. Clutch <= .05 or wheel speed <= .05 selects launch ratio to avoid ill-conditioned inversion. Factory first/top ratio bounds remain. The actuator uses a .12 second exponential response plus the existing firstGearRatio*CvtRate slew ceiling. Launch/disengagement and ratio saturation make some requested RPM unreachable; this is explicitly not a physical RPM guarantee.

CVT now accepts clutch slip even in higher integer gears: it does not request an integer change or reset clutch, so the stepped-AT shifting guard was inappropriate. Valid CVT entry/time-gap resync takes control immediately from the current gear ratio; it no longer gives stock forward selection an unnecessary tick. Invalid telemetry still releases control. The stock near-zero-pedal/low-speed early-return path precedes the existing gate and remains original; loss of contact and other safety exclusions also remain. Without new driving logs, the exact contribution of these paths to the reported gear jumps is not established.

The original integer gear is held while three engine instructions consume the virtual ratio. No shared handling, displayed RPM or direct engine-force memory is overwritten. Native clutch, engine inertia and throttle response remain. Mechanical target is not an actual RPM calibration and native revs can differ during clutch slip. This is not a full belt, pulley inertia or centrifugal clutch model.

## Version-specific static evidence

Read-only disassembly of the available GTAIV.exe 1.2.0.59 reference was compared against the exact instruction bytes used by runtime checks. Preferred image base is 0x400000; use RVAs after relocation. These addresses are **only verified for this layout**. Other builds require independent verification. No whole-file hash or PC identity restriction is imposed.

Original engine call VA C2FB6B (RVA 82FB6B) calls C2FDB0 (RVA 82FDB0). Entry bytes: `55 8B EC 83 E4 F0 83 EC 28 56 57 8B F9`. EDI holds the transmission; ECX is then the vehicle argument. C2FDCE (RVA 82FDCE), `8B 91 C8 0D 00 00`, loads EDX = vehicle handling pointer at +DC8.

| Purpose | RVA | Original instruction bytes | Original operation | Continue RVA |
|---|---|---|---|---|
| Mechanical revs | 82FE3F | F3 0F 10 74 82 54 | movss xmm6,[edx+eax*4+54] | 82FE45 |
| Limiter | 82FFF2 | F3 0F 10 44 82 54 | movss xmm0,[edx+eax*4+54] | 82FFF8 |
| Drive force | 830075 | F3 0F 59 4C 82 54 | mulss xmm1,[edx+eax*4+54] | 83007B |

The first two loaded ratios are divided by handling +4C and multiplied by wheel/longitudinal speed respectively. The torque instruction follows the drive-force factor at handling +44 and feeds native clutch/throttle/engine calculations and driven-wheel division. These are the three forward gear-ratio uses in this engine function. The limiter path is conditional, so RPM then torque is also accepted. Bike routing is separately checked against the original game caller. LCP automobile engine integration is retained; no CVT hook is claimed inside LCP's replacement engine.

## Gate and ownership contract

Each gate saves GPRs, EFLAGS and aligned FXSAVE state, establishes a clean C++ FP environment, calls a noexcept callback and restores the saved state. Replacement MOVSS clears upper 96 bits as the original memory form does; MULSS preserves them and executes with restored MXCSR. No lease means execute the original instruction via MinHook's gateway.

Each accepted player shift callback issues a single ratio lease, bound to transmission, handling pointer, vehicle pool identity/generation, current game tick, owner thread and integer gear. Engine consumption rechecks player ownership and these bindings. The sequence is RPM, optional limiter, torque. Torque consumes the lease. Every next dispatch clears it; OFF clears it. A callback exception latches CVT unavailable until restart. CVT verification/creation failure uses Scooter stepped AT; invalid telemetry or unavailable base integration uses original safety fallback. Optional hooks are prepared before one queued activation with the base hooks; partial optional creation is removed.

## Evidence and limits

Nine automated suites passed in MSVC x86 Release. New tests cover classification/configuration, rate/bounds and 30/60/144 Hz convergence, synthetic adapter-to-engine lease consumption, no shared handling mutation, F8/identity/tick rejection, and twelve actual x86 gate paths (three sites, custom/original, direction flag clear/set). All five runtime CVT instruction windows and the engine call target match the reference executable.

No actual driving has been validated for this version. Test Faggio on level ground at low speed first, then gradually increase speed/throttle. Log `CVT ENGINE COUPLED` confirms callback consumption, not physical accuracy. Integer gear display stays fixed. Loss of both-wheel contact, disable or rejected telemetry can return to the factory ratio abruptly; original clutch behavior can also affect launch and indicated revs. Use `Cvt=0` to select ThrottleAT Scooter stepped shifts for comparison. Keep both LOG and CSV before restarting, since each run overwrites them.

## 0.5.1-test regression evidence

All nine suites pass after the revision. Pedal sweeps at 30/60/144 Hz and clutch .6/.8/1 verify distinct intermediate targets and convergence of the native blended target when achievable. Pedal jitter tests enforce smooth demand and ratio slew limits. Synthetic adapter tests retain CVT across entry, clutch slip, pedal jitter and integer gears 1/3/4, never writing a gear change. CPU gate code/addresses are unchanged. Actual game engine dynamics, mod interactions and dashboard behavior still require a new driving test.
