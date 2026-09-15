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
void Controller::reset() { *this = Controller{}; }
Decision Controller::update(const Telemetry& t, const Tune& p) {
    if (!valid(t)) { reset(); return {}; }
    if (vehicle_ && (t.time < lastTime_ || t.time - lastTime_ > .25)) {
        reset(); return {}; // Suspend/resume or time discontinuity: release stock control.
    }
    if (vehicle_ != t.vehicle) {
        reset(); vehicle_ = t.vehicle; observed_ = t.gear; lastShift_ = t.time;
    }
    lastTime_ = t.time;
    if (pending_) {
        if (t.gear == pending_) {
            // Gear acknowledgement and clutch reengagement are separate.
            // Slow clutch recovery must not look like a failed gear command.
            observed_ = t.gear; pending_ = 0; lastShift_ = t.time;
        } else if (t.time - requestTime_ > 1.0) {
            reset(); return {}; // Gear request never acknowledged.
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
    const double up = t.throttle <= .5
        ? p.low + (p.mid - p.low) * (t.throttle * 2)
        : p.mid + (p.high - p.mid) * ((t.throttle - .5) * 2);
    const auto predicted = [&](int gear) { return t.rpm * t.ratios[gear] / t.ratios[t.gear]; };
    int target = t.gear;
    Reason reason = Reason::Hold;
    // One-gear kickdown. Brake inhibits kickdown but permits low-RPM recovery.
    if (t.gear > 1 && t.speed > 2 && t.brake < .1 &&
        t.throttle >= p.kickThrottle && t.rpm < p.kickTarget &&
        predicted(t.gear - 1) <= p.kickTarget) {
        target--; reason = Reason::Kickdown;
    } else if (t.gear > 1 && t.rpm < p.down && predicted(t.gear - 1) < up - .08) {
        target--; reason = Reason::Downshift;
    } else if (t.gear < t.gears && t.speed > 2 && t.rpm >= up &&
               predicted(t.gear + 1) > p.down + .08 &&
               // Avoid immediately kicking back down under the same demand.
               (t.throttle < p.kickThrottle || t.rpm > p.kickTarget + .08)) {
        target++; reason = Reason::Upshift;
    }
    if (target == t.gear) { candidate_ = 0; return {t.gear, Reason::Hold}; }
    if (candidate_ != target) { candidate_ = target; since_ = t.time; }
    if (t.time - since_ < p.confirm) return {t.gear, Reason::Hold};
    candidate_ = 0; pending_ = target; requestTime_ = t.time; lastShift_ = t.time;
    return {target, reason};
}
} // namespace at
