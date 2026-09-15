# 0.5.2-test

Published on the existing 0.5.0-test branch. Main remains separate.

## Stepped AT

Kickdown requires raw game pedal >=1.0 (99% is not full pedal). At most one confirmed adjacent kickdown per press. The latch rearms only after <=.90 for .15 seconds, avoiding repeated 5→4→3→2→1 while held. It is consumed on an actual request, not just entering full throttle. Brake, speed, predicted lower-gear RPM, confirmation and cooldown guards remain. During a full-pedal episode low-RPM recovery uses the base Down threshold, avoiding demand-inflated recovery acting as another kickdown. Real low-speed anti-lugging downshifts are retained.

Legacy KickThrottle is range-validated for compatibility, but resolved value is always 1 and the controller enforces full pedal. Filtering is not used as an exact-1 trigger, because exponential filters approach that limit asymptotically.

Default LightThrottle=.60, MidThrottle=.85. Low-to-Low+LightRise covers 0..60%; then continuous interpolation reaches Mid at 85% and High at 100%. Passenger up thresholds at 20/40/60% are .2333/.2467/.26. Bike Low/Down are reduced too, while mid/high and cooldown class differences remain. CVT demand curve is unchanged. Earlier shifts are targeted without asserting a physical RPM calibration.

## CVT persistence

Ground contact loss and excessive wheelspin hold a vehicle-bound ratio instead of handing selection to stock or stepped AT. Valid ownership, ratio layout and forward direction are still required. CVT unavailable/fault does not silently select Scooter stepped AT; only explicit Cvt=0 selects it. OFF, unsupported code, invalid memory/ownership and reverse still pass through. The original pre-hook closed-pedal low-speed clutch reset remains and does not select a different forward gear.

## Validation

Regression sequences cover sustained full pedal with acknowledged gear changes at 30/60/144 Hz, 99% input, release/repress rearming, jitter, short releases, overrev rejection and the 60% economy band. CVT tests cover contact loss, wheelspin, recovery, lifecycle and missing-hook behavior. See VALIDATION.md for build results. User reports validate the preceding CVT experience, not these new changes or displayed RPM targets.

The kickdown-used latch survives same-vehicle telemetry interruptions and F8 resynchronization; a different vehicle resets it. Confirmation is specific to both target gear and reason, so an ordinary downshift candidate cannot skip full-pedal confirmation.
