#include "at.hpp"
#include <algorithm>
#include <cmath>
namespace at {
CvtDecision CvtController::update(const Telemetry& t, const Tune& p, double wheel, double scale) {
    // Clutch slip is continuous in a CVT, not a request for stepped shifting.
    if(p.cvt!=1 || t.kind!=VehicleClass::Scooter || !valid(t) ||
       !std::isfinite(t.clutch) || t.clutch<0 || t.clutch>1 ||
       !std::isfinite(wheel) || wheel<0 || !std::isfinite(scale) || scale<=1) {reset();return {};}
    const double lo=t.ratios[t.gears], hi=t.ratios[1];
    const bool restart=vehicle_!=t.vehicle || lastTime_<0 || t.time<lastTime_ || t.time-lastTime_>.25 ||
                       lo!=minRatio_ || hi!=maxRatio_ || t.gear!=anchorGear_;
    if(restart) {
        vehicle_=t.vehicle; ratio_=t.ratios[t.gear]; lastTime_=t.time;
        demand_=t.throttle; minRatio_=lo; maxRatio_=hi; anchorGear_=t.gear;
    }
    const double dt=t.time-lastTime_; lastTime_=t.time;
    const double tau=p.cvtResponse*(t.throttle<demand_ ? 1.5 : 1.0);
    demand_+=(t.throttle-demand_)*(-std::expm1(-dt/tau));
    // Quadratic Bezier: continuous value AND slope through medium throttle.
    // The middle control point makes half throttle equal CvtMid.
    const double mid=std::clamp(2*p.cvtMid-.5*(p.cvtLow+p.cvtHigh),p.cvtLow,p.cvtHigh);
    const double x=std::clamp(demand_,0.0,1.0);
    const double target=(1-x)*(1-x)*p.cvtLow+2*x*(1-x)*mid+x*x*p.cvtHigh;
    // CE059 blends free revs and wheel revs before native engine inertia:
    // engineTarget = (1-clutch)*rawPedal + clutch*(wheel*ratio/scale).
    // Invert that blend, without writing revs or clutch. At disengagement
    // the ratio cannot control revs; use the launch ratio, never divide by zero.
    const double mechanical=t.clutch>.05 ?
        std::max(0.0,(target-(1-t.clutch)*t.throttle)/t.clutch) : target;
    const double desired=wheel>.05 && t.clutch>.05 ? std::clamp(mechanical*scale/wheel,lo,hi) : hi;
    // Smooth actuator response plus an absolute slew ceiling, independent of FPS.
    const double step=(desired-ratio_)*(-std::expm1(-dt/.12));
    const double limit=hi*p.cvtRate*dt;
    ratio_+=std::clamp(step,-limit,limit);
    ratio_=std::clamp(ratio_,lo,hi);
    return {true,ratio_,target};
}
} // namespace at
