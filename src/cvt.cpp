#include "at.hpp"
#include <algorithm>
#include <cmath>
namespace at {
CvtDecision CvtController::update(const Telemetry& t, const Tune& p, double wheel, double scale) {
    if(p.cvt!=1 || t.kind!=VehicleClass::Scooter || !valid(t) || t.shifting ||
       !std::isfinite(wheel) || wheel<0 || !std::isfinite(scale) || scale<=1) {reset();return {};}
    const double lo=t.ratios[t.gears], hi=t.ratios[1];
    const bool restart=vehicle_!=t.vehicle || lastTime_<0 || t.time<lastTime_ || t.time-lastTime_>.25 ||
                       lo!=minRatio_ || hi!=maxRatio_ || t.gear!=anchorGear_;
    if(restart) {
        vehicle_=t.vehicle; ratio_=t.ratios[t.gear]; lastTime_=t.time;
        minRatio_=lo; maxRatio_=hi; anchorGear_=t.gear;
    }
    const double dt=t.time-lastTime_; lastTime_=t.time;
    // The verified stock ratio range bounds this prototype. At rest it remains
    // at the launch ratio; engine/clutch physics still provide the launch slip.
    const double demand=std::clamp(t.throttle,0.0,1.0);
    const double target=p.cvtLow+(p.cvtHigh-p.cvtLow)*demand;
    const double desired=wheel>.05 ? std::clamp(target*scale/wheel,lo,hi) : hi;
    const double limit=hi*p.cvtRate*dt;
    ratio_+=std::clamp(desired-ratio_,-limit,limit);
    ratio_=std::clamp(ratio_,lo,hi);
    return {true,ratio_,target};
}
} // namespace at
