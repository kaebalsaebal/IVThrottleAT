#include "at.hpp"
#include <cmath>
#include <fstream>
#include <iostream>
#include <limits>
#include <sstream>
#include <stdexcept>

int checks = 0;
void check(bool ok, const char* label) {
    ++checks;
    if (!ok) throw std::runtime_error(label);
}
at::Telemetry car(double throttle=.1, double rpm=.55) {
    at::Telemetry t;
    t.vehicle=1; t.model="TEST"; t.playerDriver=true; t.grounded=true;
    t.throttle=throttle; t.rpm=rpm; t.speed=20; t.gear=2; t.gears=5;
    t.ratios={0, 3.2, 2.1, 1.5, 1.1, .85, 0, 0, 0};
    return t;
}
at::Decision run(at::Controller& c, at::Telemetry& t, int frames=110) {
    at::Decision d;
    for (int i=0; i<frames; ++i) { t.time+=.01; d=c.update(t, at::Tune{}); }
    return d;
}
struct Fake final : at::Backend {
    at::Telemetry t=car();
    bool available=true, readable=true, writable=true;
    int writes=0, releases=0, target=0;
    bool verified() const noexcept override { return available; }
    bool sample(at::Telemetry& out) noexcept override { out=t; return readable; }
    bool apply(std::uint64_t, int gear) noexcept override { ++writes; target=gear; return writable; }
    void release() noexcept override { ++releases; }
};
int main(int argc, char** argv) {
    try {
        for (const auto& test : {std::array<double,3>{.1,.55,3}, {.5,.35,3}, {.5,.70,3}, {1,.90,2}, {1,.96,3}, {1,.45,1}, {1,.60,2}}) {
            at::Controller c; auto t=car(test[0],test[1]);
            check(run(c,t).gear == static_cast<int>(test[2]), "shift map / kickdown / overrev guard");
        }
        at::Controller c; auto t=car(.1,.55);
        check(run(c,t,15).gear==2, "initial synchronization hold");
        check(run(c,t,30).gear==3, "confirmed upshift after synchronization");
        t.gear=3; t.rpm=.05;
        check(run(c,t,50).gear==3, "cooldown after acknowledgement");
        check(run(c,t,40).gear==2, "low rpm downshift");
        c.reset(); t=car(1,.45); t.brake=.5;
        check(run(c,t).gear==2, "braking inhibits kickdown");
        c.reset(); t=car(); t.gear=5;
        check(run(c,t).gear==5, "top gear bounded");
        c.reset(); t=car(); t.speed=0;
        check(run(c,t).gear==2, "stationary upshift inhibited");
        c.reset(); t=car(); run(c,t,80); t.time+=.4;
        check(c.update(t,{}).reason==at::Reason::Fallback, "long frame gap fallback");
        c.reset(); t=car(); run(c,t,80);
        bool timedOut=false; for (int i=0;i<110;++i) { t.time+=.01; if(c.update(t,{}).reason==at::Reason::Fallback) timedOut=true; } check(timedOut, "unacknowledged request timeout");
        c.reset(); t=car(); run(c,t,80); t.gear=3; t.shifting=true;
        bool slowClutchHeld=true;
        for(int i=0;i<180;++i) { t.time+=.01; const auto d=c.update(t,{}); slowClutchHeld=slowClutchHeld&&d.gear==3&&d.reason==at::Reason::Hold; }
        check(slowClutchHeld,"acknowledged gear with slow clutch recovery is not a failed request");
        t=car(); t.rpm=std::numeric_limits<double>::quiet_NaN(); check(!at::valid(t), "nan rejected");
        t=car(); t.age=.2; check(!at::valid(t), "stale rejected");
        t=car(); t.gear=-1; check(!at::valid(t), "reverse rejected");
        t=car(); t.ratios[2]=0; check(!at::valid(t), "unknown ratios rejected");
        t=car(); t.grounded=false; check(!at::valid(t), "airborne fallback");

        at::Config cfg; cfg.enabled=true; Fake b; at::Runtime r(b,cfg);
        r.tick(); check(b.writes==1, "valid hold applied");
        b.t.vehicle=2; r.tick(); check(b.releases==1, "old vehicle released");
        b.readable=false; r.tick(); check(b.releases==2 && b.writes==2, "telemetry failure restores stock");
        b.readable=true; b.writable=false; r.tick(); const int failedWrites=b.writes;
        b.writable=true; r.tick(); check(b.writes==failedWrites, "control failure latches");
        r.stop(); r.tick(); check(b.writes==failedWrites+1, "explicit reset clears latch");
        b.available=false; r.tick(); check(b.writes==failedWrites+1, "unverified backend never writes");
        at::Config disabled; Fake d; at::Runtime off(d,disabled); off.tick(); check(d.writes==0, "disabled never writes");

        std::istringstream ini("[General]\nEnabled=1\n[Class:Sport]\nHigh=.96\n[Model:infernus]\nHigh=.98\n");
        auto parsed=at::parseConfig(ini);
        check(parsed.enabled, "enabled parsed");
        check(parsed.resolve(at::VehicleClass::Sport,"INFERNUS").high==.98, "model wins");
        check(parsed.resolve(at::VehicleClass::Heavy,"BUS").low==.20, "heavy preset");
        std::istringstream legacyKick("[Class:Passenger]\nKickThrottle=.82\n[Model:BUS]\nKickThrottle=.92\n");
        const auto migratedKick=at::parseConfig(legacyKick);
        check(migratedKick.resolve(at::VehicleClass::Passenger,"TEST").kickThrottle==1, "old class kick threshold cannot enable partial-pedal kickdown");
        check(migratedKick.resolve(at::VehicleClass::Heavy,"BUS").kickThrottle==1, "old model kick threshold cannot enable partial-pedal kickdown");
        for (const auto* bad : {"[General]\nEnabled=yes", "[Class:Sport]\nHigh=nan", "[Class:Sport]\nHigh=.4", "[Class:Sport]\nLow=.4junk", "[Oops]\nX=1"}) {
            bool rejected=false; try { std::istringstream s(bad); at::parseConfig(s); } catch (...) { rejected=true; }
            check(rejected,"bad config rejected");
        }
        if (argc > 1) { std::ifstream example(argv[1]); check(bool(example),"example opens"); at::parseConfig(example); }
        std::cout << checks << " checks passed\n";
        return 0;
    } catch (const std::exception& e) { std::cerr << "FAILED: " << e.what() << '\n'; return 1; }
}
