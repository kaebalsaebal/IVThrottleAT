#include "at.hpp"
#include <cmath>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <sstream>
#include <vector>
int checks=0;
void check(bool ok,const char* what) { ++checks; if(!ok) throw std::runtime_error(what); }
at::Telemetry sample(double pedal=.2) {
    at::Telemetry t; t.vehicle=1; t.model="SULTAN"; t.playerDriver=true; t.grounded=true;
    t.throttle=pedal; t.speed=8; t.rpm=.10; t.gear=1; t.gears=5;
    t.ratios={0,3.2,2.1,1.5,1.1,.85,0,0,0}; return t;
}
// Acknowledge each request immediately and preserve wheel speed. Holding a fixed
// RPM after a downshift would hide the feedback that caused the original cascade.
struct Drive {
    at::Controller controller;
    at::Telemetry t;
    at::Tune tune;
    double fps;
    int kicks=0, downshifts=0, upshifts=0;
    Drive(const at::Tune& p,double rate,double rpm,double pedal=1) : t(sample(pedal)),tune(p),fps(rate) {
        t.gear=5; t.rpm=rpm;
    }
    void step(double pedal) {
        t.time+=1/fps; t.throttle=pedal;
        const auto d=controller.update(t,tune);
        check(d.reason!=at::Reason::Fallback,"continuous pedal scenario never faults");
        if(d.gear!=t.gear) {
            check(std::abs(d.gear-t.gear)==1,"each command changes exactly one gear");
            if(d.reason==at::Reason::Kickdown) ++kicks;
            else if(d.reason==at::Reason::Downshift) ++downshifts;
            else if(d.reason==at::Reason::Upshift) ++upshifts;
            else check(false,"new gear request carries a shift reason");
            t.rpm*=t.ratios[d.gear]/t.ratios[t.gear]; t.gear=d.gear;
        }
    }
    void hold(double pedal,double seconds) {
        for(int i=0;i<int(std::ceil(seconds*fps));++i) step(pedal);
    }
};
int main(int argc,char** argv) {
    try {
        if(argc!=2) throw std::runtime_error("config required");
        std::ifstream file(argv[1]); if(!file) throw std::runtime_error("config missing");
        const auto cfg=at::parseConfig(file);
        const auto tune=cfg.resolve(at::VehicleClass::Passenger,"SULTAN");
        const auto light=at::shiftBands(.2,tune);
        check(light.up<.26,"shared passenger economy band stays low at 20 percent throttle");
        check(at::shiftBands(.6,tune).up<=.27,"60 percent remains economy demand");
        check(at::shiftBands(.75,tune).up>at::shiftBands(.6,tune).up && at::shiftBands(1,tune).up>.9,"medium and full demand separated");
        check(std::abs(at::shiftBands(.600001,tune).up-at::shiftBands(.6,tune).up)<.00001,"economy boundary is continuous");
        for(auto kind:{at::VehicleClass::Passenger,at::VehicleClass::Heavy,at::VehicleClass::Sport,
                       at::VehicleClass::Motorcycle,at::VehicleClass::SportBike,at::VehicleClass::CruiserBike,
                       at::VehicleClass::StandardBike,at::VehicleClass::Scooter}) {
            const auto p=cfg.resolve(kind,"");
            check(p.kickThrottle==1,"every shipped class requires full pedal for kickdown");
            check(at::shiftBands(.6,p).up<=p.low+p.lightRise+.000001,"every class retains its economy band through 60 percent");
        }
        check(cfg.resolve(at::VehicleClass::Heavy,"BUS").kickThrottle==1,"model override also requires full pedal");
        // Ramp physical wheel speed, acknowledge commands, apply real ratio drops.
        std::vector<double> firstShift;
        for(double pedal:{.2,.6}) {
            firstShift.clear();
            for(double fps:{30.,60.,144.}) {
                at::Controller c; auto t=sample(pedal); double wheel=0; int shifts=0; double first=0;
                for(int i=0;i<int(fps*35);++i) {
                    t.time=(i+1)/fps; wheel+=.40/fps; t.speed=wheel*80/3.2;
                    t.rpm=wheel*t.ratios[t.gear]/3.2;
                    const auto d=c.update(t,tune);
                    check(d.reason!=at::Reason::Fallback,"continuous drive never faults");
                    if(d.gear!=t.gear) {
                        check(d.gear==t.gear+1,"light acceleration never hunts down");
                        if(!shifts) first=t.rpm;
                        ++shifts; t.gear=d.gear;
                    }
                    if(t.gear==5) break;
                }
                check(shifts==4,"economy drive reaches fifth gear");
                // RPM grows by .40 per second while confirmation is pending.
                // Threshold crossing and confirmation can each cost one frame.
                const double band=at::shiftBands(pedal,tune).up;
                check(first>=band+.40*tune.confirm-1e-9,"first shift observes the complete confirmation delay");
                check(first<=band+.40*(tune.confirm+2/fps)+1e-9,"early first shift is not blocked by next-gear RPM floor");
                firstShift.push_back(first);
            }
            check(std::abs(firstShift.front()-firstShift.back())<.025,"economy shift timing stable across frame rates");
        }
        for(double fps:{30.,60.,144.}) {
            for(double rpm:{.20,.10}) {
                Drive drive(tune,fps,rpm);
                drive.hold(1,12);
                check(drive.kicks==1 && drive.t.gear==4,"one sustained full-pedal press permits one kickdown, not fifth-to-first cascade");
                check(drive.downshifts==0,"full demand does not disguise repeated kickdown as low-RPM recovery");
            }
            Drive drive(tune,fps,.20,.6);
            drive.hold(.6,1);
            drive.hold(.99,4);
            drive.hold(.999999,2);
            check(drive.kicks==0 && drive.t.gear==5,"even almost-full pedal never triggers kickdown");
            // Filtered demand approaches 1 asymptotically. Raw full pedal must
            // still start a confirmed kickdown promptly after this partial input.
            drive.hold(1,.6);
            check(drive.kicks==1 && drive.t.gear==4,"full pedal triggers despite filtered demand below one");
            for(int i=0;i<int(fps*3);++i) drive.step(i%2 ? .99 : 1);
            check(drive.kicks==1 && drive.t.gear==4,"near-full trigger jitter cannot rearm kickdown");
            drive.hold(.90,.05);
            drive.hold(1,2);
            check(drive.kicks==1 && drive.t.gear==4,"brief pedal dip cannot rearm kickdown");
            drive.hold(.90,.25);
            drive.hold(1,6);
            check(drive.kicks==2 && drive.t.gear==3,"deliberate release and second full press permit exactly one more kickdown");
            Drive overrev(tune,fps,.60);
            overrev.t.gear=2;
            overrev.hold(1,6);
            check(overrev.kicks==0 && overrev.t.gear==2,"full pedal cannot exceed next-gear RPM safety limit");
            Drive interrupted(tune,fps,.20);
            interrupted.hold(1,2);
            check(interrupted.kicks==1,"interrupted-drive setup has consumed one kickdown");
            for(int i=0;i<3;++i) {
                interrupted.t.grounded=false; interrupted.t.time+=1/fps;
                check(interrupted.controller.update(interrupted.t,tune).reason==at::Reason::Fallback,"contact loss still releases unsafe control");
                interrupted.t.grounded=true; interrupted.hold(1,1);
                check(interrupted.kicks==1 && interrupted.t.gear==4,"repeated contact loss does not rearm held full pedal");
            }
            interrupted.controller.reset(true);
            interrupted.hold(1,2);
            check(interrupted.kicks==1 && interrupted.t.gear==4,"adapter timing reset preserves same-vehicle kickdown latch");
            interrupted.t.time+=.30;
            check(interrupted.controller.update(interrupted.t,tune).reason==at::Reason::Fallback,"long timing gap releases control");
            interrupted.hold(1,2);
            check(interrupted.kicks==1 && interrupted.t.gear==4,"timing-gap recovery cannot repeat a held-pedal kickdown");
            interrupted.hold(.90,.25);
            interrupted.hold(1,2);
            check(interrupted.kicks==2 && interrupted.t.gear==3,"release after safety recovery still rearms one kickdown");
        }
        // Both low-RPM recovery and kickdown can name the same target gear.
        // The full-pedal press needs its own confirmation time, not the elapsed
        // time of a different pending decision made before that press.
        auto confirmingTune=tune; confirmingTune.cooldown=.20; confirmingTune.confirm=.20;
        Drive confirming(confirmingTune,100,.09,.5); confirming.t.gear=3;
        confirming.hold(.5,.35);
        check(confirming.t.gear==3,"low-RPM candidate has not yet completed confirmation");
        confirming.hold(1,.12);
        check(confirming.kicks==0 && confirming.t.gear==3,"kickdown cannot borrow low-RPM candidate confirmation time");
        confirming.hold(1,.15);
        check(confirming.kicks==1 && confirming.t.gear==2,"full-pedal candidate confirms after its own complete delay");
        at::Controller c; auto t=sample(1); t.rpm=.85;
        for(int i=0;i<120;++i) {t.time+=.01; check(c.update(t,tune).gear==1,"full throttle holds below redline band");}
        t.rpm=.96; at::Decision d;
        for(int i=0;i<20;++i) {t.time+=.01; d=c.update(t,tune);}
        check(d.gear==2,"full throttle eventually upshifts");
        c.reset(); t=sample(.2); t.gear=3; t.rpm=.22;
        for(int i=0;i<120;++i) {t.time+=.01; c.update(t,tune);}
        t.throttle=1;
        for(int i=0;i<50;++i) {t.time+=.01; d=c.update(t,tune);}
        check(d.gear==2,"pedal tip-in requests one-gear kickdown");
        c.reset(); t=sample(.8); t.gear=3; t.rpm=.40;
        for(int i=0;i<120;++i) {t.time+=.01; c.update(t,tune);}
        t.throttle=.1;
        for(int i=0;i<40;++i) {t.time+=.01; check(c.update(t,tune).gear==3,"lift-off briefly retains engine-braking gear");}
        for(int i=0;i<45;++i) {t.time+=.01; d=c.update(t,tune);}
        check(d.gear==4,"light throttle resumes economy shifts after lift hold");
        c.reset(); t=sample(.15); t.gear=3; t.rpm=.40; t.brake=.5;
        for(int i=0;i<120;++i) {t.time+=.01; check(c.update(t,tune).gear==3,"braking does not seek a higher gear");}
        c.reset(); t=sample(0); t.rpm=.6;
        for(int i=0;i<120;++i) {t.time+=.01; check(c.update(t,tune).gear==1,"closed throttle does not upshift");}
        // Sustain a narrow noisy operating point after an acknowledged upshift.
        c.reset(); t=sample(.2); t.rpm=.25;
        for(int i=0;i<100;++i) {t.time+=.01; d=c.update(t,tune);}
        check(d.gear==2,"setup economy upshift");
        t.gear=2;
        for(int i=0;i<800;++i) {
            t.time+=.01; t.throttle=.2+std::sin(i*.4)*.02; t.rpm=.16+std::sin(i*.3)*.005;
            check(c.update(t,tune).gear==2,"small pedal/RPM fluctuations do not hunt");
        }
        for(const char* bad:{"LightThrottle=0", "MidThrottle=.2", "LightRise=1", "LiftHold=-1"}) {
            bool rejected=false; try {std::istringstream in(std::string("[Class:Passenger]\n")+bad); at::parseConfig(in);} catch(...) {rejected=true;}
            check(rejected,"invalid new tuning rejected");
        }
        std::cout<<checks<<" schedule scenario checks passed\n";
    } catch(const std::exception& e) {std::cerr<<e.what()<<'\n';return 1;}
}
