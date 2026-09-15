# 0.5.0-test motorcycle presets and experimental CVT

## Scope and classification

Cars retain 0.4.1 settings, including no Sultan override. Motorcycle type is checked before model classification. Known names are registered even when episode IDE discovery is incomplete.

- SportBike: AKUMA, BATI, BATI2, HAKUCHOU, HAKUCHOU2, NRG900, DOUBLE, DOUBLE2.
- CruiserBike: FREEWAY, ANGEL, DAEMON, DIABOLUS, HELLFURY, HEXER, LYCAN, NIGHTBLADE, REVENANT, WOLFSBANE, ZOMBIE.
- Scooter: FAGGIO.
- StandardBike: all remaining bikes, including PCJ, VADER, SANCHEZ.

These are tuning categories, not a claim about real motorcycle specifications. `[Model:NAME] BikeClass=SportBike|CruiserBike|StandardBike|Scooter` can override classification for actual bike types. Legacy Motorcycle overrides precede specific class and model overrides. See the distributed INI for complete presets.

## Continuous ratio controller

Scooter Cvt=1 enables the attempt when runtime engine checks pass. Target normalized mechanical revs = CvtLow + (CvtHigh-CvtLow)*throttle, defaults 0.32..0.78. Desired ratio = target*handlingVelocity/absoluteWheelSpeed. The original first and top gear ratios bound it. Rate is limited to firstGearRatio*CvtRate per second (default 1.8). Entry starts at the actual integer gear ratio. Identity, time gaps and changed bounds/gear reset it.

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
