#include "at.hpp"
#include <algorithm>
#include <cmath>

namespace at {
bool valid(const Telemetry& t) {
    const auto finite = [](double x) { return std::isfinite(x); };
    if (!t.vehicle || !t.playerDriver || !t.grounded ||
        !finite(t.time) || t.time < 0 || !finite(t.age) || t.age < 0 || t.age > .1 ||
        !finite(t.throttle) || t.throttle < 0 || t.throttle > 1 ||
        !finite(t.brake) || t.brake < 0 || t.brake > 1 ||
        !finite(t.speed) || t.speed < 0 || t.speed > 200 ||
        !finite(t.rpm) || t.rpm < 0 || t.rpm > 1.2 ||
        t.gears < 1 || t.gears > 8 || t.gear < 1 || t.gear > t.gears) return false;
    for (int i = 1; i <= t.gears; ++i) {
        if (!finite(t.ratios[i]) || t.ratios[i] <= 0 ||
            (i > 1 && t.ratios[i] >= t.ratios[i-1])) return false;
    }
    return true;
}
// A shallow light-pedal band keeps gentle acceleration in the economy range.
// These values are normalized mechanical speeds, not a calibrated tachometer.
ShiftBands shiftBands(double throttle, const Tune& p) {
    const double x = std::clamp(throttle, 0.0, 1.0);
    const auto blend = [](double a, double b, double f) { return a + (b-a)*f; };
    double up;
    if (x <= p.lightThrottle)
        up = blend(p.low, p.low+p.lightRise, x/p.lightThrottle);
    else if (x <= p.midThrottle)
        up = blend(p.low+p.lightRise, p.mid, (x-p.lightThrottle)/(p.midThrottle-p.lightThrottle));
    else
        up = blend(p.mid, p.high, (x-p.midThrottle)/(1-p.midThrottle));
    // Demand-dependent low-speed recovery, capped below the upshift band.
    const double down = std::min(p.down + .12*x*x, up - .06);
    return {up, down, down + .035};
}
void Controller::reset(bool keepKick) {
    const auto vehicle=vehicle_; const bool used=kickUsed_;
    *this = Controller{};
    if(keepKick) { vehicle_=vehicle; kickUsed_=used; }
}
Decision Controller::update(const Telemetry& t, const Tune& p) {
    if (!valid(t)) { reset(true); return {}; }
    if (vehicle_ && lastTime_ >= 0 && (t.time < lastTime_ || t.time - lastTime_ > .25)) {
        reset(true); return {}; // Release original control after a time discontinuity.
    }
    double dt = t.time - lastTime_;
    if (vehicle_ != t.vehicle || lastTime_ < 0) {
        reset(vehicle_ == t.vehicle); vehicle_ = t.vehicle; observed_ = t.gear;
        // Entry synchronization is not a gear change. Settle for 200 ms, then
        // allow an early first shift; actual shifts retain the full cooldown.
        lastShift_ = t.time - p.cooldown + .20;
        demand_ = previousPedal_ = t.throttle; dt = 0;
    }
    // One kickdown per full-pedal press. Filtering approaches 1 asymptotically,
    // so use the actual game pedal for full travel and debounce with confirm.
    const bool fullPedal = t.throttle >= 1.0;
    if (t.throttle <= .90) {
        if (kickReleaseSince_ < 0) kickReleaseSince_ = t.time;
        if (t.time - kickReleaseSince_ >= .15) kickUsed_ = false;
    } else kickReleaseSince_ = -1;
    // A deliberate pedal release holds the gear briefly for engine braking.
    if (previousPedal_ >= .55 && previousPedal_ - t.throttle >= .20)
        liftUntil_ = t.time + p.liftHold;
    previousPedal_ = t.throttle;
    if (dt > 0) {
        const double tau = t.throttle > demand_ ? .08 : .25;
        demand_ += (t.throttle-demand_) * (1-std::exp(-dt/tau));
    }
    lastTime_ = t.time;
    if (pending_) {
        if (t.gear == pending_) {
            observed_ = t.gear; pending_ = 0; lastShift_ = t.time;
        } else if (t.time - requestTime_ > 1.0) {
            reset(true); return {}; // Gear request never acknowledged.
        } else {
            return {pending_, Reason::Hold};
        }
    }
    if (t.gear != observed_) {
        observed_ = t.gear; lastShift_ = t.time; candidate_ = 0;
    }
    if (t.shifting || t.time - lastShift_ < p.cooldown) {
        candidate_ = 0; return {t.gear, Reason::Hold};
    }
    const auto bands = shiftBands(demand_, p);
    const auto predicted = [&](int gear) { return t.rpm * t.ratios[gear] / t.ratios[t.gear]; };
    int target = t.gear;
    Reason reason = Reason::Hold;
    if (t.gear > 1 && t.speed > 2 && t.brake < .1 &&
        fullPedal && !kickUsed_ &&
        t.rpm < p.kickTarget && predicted(t.gear - 1) <= p.kickTarget) {
        target--; reason = Reason::Kickdown;
    } else if (t.gear > 1 && t.rpm < ((fullPedal || kickUsed_) ? p.down : bands.down) && predicted(t.gear - 1) < bands.up - .04) {
        target--; reason = Reason::Downshift;
    } else if (t.gear < t.gears && t.speed > 2 && t.rpm >= bands.up &&
               predicted(t.gear + 1) > bands.minAfterUpshift &&
               // Never upshift on closed throttle; braking/lift retains the gear
               // except near the normalized limiter. Reverse remains backend-owned.
               t.throttle > .02 && ((t.brake < .15 && t.time >= liftUntil_) || t.rpm >= .98) &&
               (!fullPedal || t.rpm > p.kickTarget + .08)) {
        target++; reason = Reason::Upshift;
    }
    if (target == t.gear) { candidate_ = 0; return {t.gear, Reason::Hold}; }
    if (candidate_ != target || candidateReason_ != reason) { candidate_ = target; candidateReason_ = reason; since_ = t.time; }
    if (t.time - since_ < p.confirm) return {t.gear, Reason::Hold};
    if (reason == Reason::Kickdown) kickUsed_ = true;
    candidate_ = 0; pending_ = target; requestTime_ = t.time; lastShift_ = t.time;
    return {target, reason};
}
} // namespace at
