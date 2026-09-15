# ThrottleAT 0.4.1 — shift schedule and motorcycle integration

0.4.1 removes the Model:SULTAN override. Sultan uses the common Passenger tune; the 0.4.0 controller and bike integration are otherwise unchanged. Replace the INI as well as the ASI: an old INI still supplies the old model override.

## Behavior

This is a conventional stepped automatic shift controller. It does not replace the engine, torque curve or torque-converter physics. The shared controller evaluates the wheel/gear-ratio normalized speed; it does not assume LVS display RPM equals that value. Native engine revs remain untouched and are now logged alongside clutch, demand and thresholds.

| Demand | Passenger / Sultan up band | Motorcycle up band |
| --- | --- | --- |
| 10% | 0.2333 | 0.3333 |
| 20% | 0.2467 | 0.3467 |
| 30% | 0.2600 | 0.3600 |
| 50% | 0.4200 | 0.5429 |
| 65% | 0.5400 | 0.6800 |
| 100% | 0.9400 | 0.9700 |

These are steady filtered demand thresholds, not physical RPM or measured shift points. During a shift, confirmation/cooldown and the estimated next-gear RPM also affect the result. Closed throttle suppresses upshift even though the map has a mathematical zero-pedal endpoint.

The former next-gear minimum was Down+0.08: this could postpone an otherwise requested early shift. The new down band is min(Down+0.12*demand^2, up-0.06); the next-gear floor is down+0.035. Regular downshifts must land below up-0.04, while kickdown separately checks the predicted lower-gear revs against KickTarget. This preserves a no-hunt interval while admitting an earlier economy shift. It is a tuneable anti-lugging heuristic, not a measured engine torque model.

Throttle uses time-based exponential smoothing (0.08 s rising, 0.25 s falling). A single-tick release of at least 0.20 from at least 0.55 holds the current gear for LiftHold (default 0.45 s). Braking at 0.15 or above inhibits ordinary upshifts; a normalized limiter exception at 0.98 applies while throttle remains above 0.02. Low-speed downshift recovery remains available. New controller entry settles for 0.20 s plus the confirmation period instead of treating entry as a completed shift. Actual acknowledged shifts retain the full class cooldown. This does not detect road gradient, trailer load or a calibrated torque-demand map.

Configuration: LightThrottle is the end of the light-pedal band; Low+LightRise is its upper value; MidThrottle is the demand for Mid; High is the full-pedal endpoint. Old INIs load but retain their old overridden values, so replace ASI and INI together for this tune.

## Bike evidence (CE059 only)

The reference executable was read as data, never modified. VAs below use preferred base 0x400000; runtime uses image base plus RVA.

| VA / runtime RVA | Evidence |
| --- | --- |
| CED298 / 8ED298 | Direct call to C2F9A0, the same transmission process used by automobiles |
| CED282 / 8ED282 | ECX = vehicle EDI + 0x1090, same transmission subobject |
| CED154 / 8ED154 | Wheel count at vehicle+F84 |
| CED15E | Wheel storage at vehicle+F80 |
| CED1B9 / 8ED1B9 | Wheel contact bit0 at wheel+164 |
| CED24A / 8ED24A | Wheel element stride 0x170 |
| C2FEA4 / 82FEA4 | Shared engine routine distinguishes vehicle+1304 == 1 in its bike reverse branch |

The call plus five instruction windows are checked before enabling type-1 vehicles. Existing original-game midhook and shared shift ABI are reused; no additional speculative hook is installed. Bikes must have exactly two wheels and both contact bits. Passenger ownership, model/pool identity, valid ratios, direction, clutch and slip guards remain. Loss of telemetry/contact forwards that tick to the existing implementation. An unknown modified bike caller disables bike control while verified car support can remain available. Runtime patch detection is still initialization-time only.

## Scooters and CVT fallback

FAGGIO receives the Motorcycle tune and ThrottleAT actively selects its gears. It is not excluded merely because it is a scooter. Other type-1 scooters likewise use Motorcycle unless Model tuning overrides it.

A true CVT would require independently verified continuously variable ratio and torque/RPM coupling per vehicle. The current verified connection only changes the integer gear and the existing shift transition fields. Shared handling ratio mutation could also affect other instances and is not implemented. Therefore this revision uses the user-approved fallback: conventional ThrottleAT automatic shifts on scooters. No simulated CVT or cosmetic RPM effect is claimed. Safety fallback for unavailable telemetry remains separate from this CVT feature fallback.

## Validation and limits

MSVC Win32 Release build and all seven CTest suites passed. Scenario tests run representative five-speed gear-ratio fixtures with the shared Passenger tuning (also used by Sultan) at 30/60/144 Hz, acknowledge requests and apply gear-ratio RPM drops. They check early 1-to-2 through fifth, no hunting, full-pedal hold, kickdown, lift-off hold and braking. These fixtures are not measured Sultan telemetry. Existing gate CPU preservation tests remain unchanged. Adapter tests prove Faggio gets active Motorcycle hold/upshift rather than passenger tuning or a stock bypass, and reject unverified bike routes, lost contact and passenger ownership.

Actual 0.4.1 road testing, the user's precise dashboard RPM, and motorcycle runtime compatibility still require in-game validation. The public [LVS dashboard source](https://github.com/ekzestean/Liberty-Vehicle-Services-CE/blob/main/plugins/source/LVSCE_Dashboard_Bridge.cpp) uses display remapping and candidate/fallback inputs; no guessed dashboard offset is used for vehicle control. CSV now includes native_revs, clutch, filtered_throttle, up_threshold and down_threshold to distinguish map calibration from a display discrepancy.
