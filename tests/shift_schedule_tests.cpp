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
int main(int argc,char** argv) {
    try {
        if(argc!=2) throw std::runtime_error("config required");
        std::ifstream file(argv[1]); if(!file) throw std::runtime_error("config missing");
        const auto cfg=at::parseConfig(file);
        const auto tune=cfg.resolve(at::VehicleClass::Passenger,"SULTAN");
        const auto light=at::shiftBands(.2,tune);
        check(light.up<.24,"Sultan economy band stays low at 20 percent throttle");
        check(at::shiftBands(.3,tune).up<=.25,"30 percent remains economy demand");
        check(at::shiftBands(.5,tune).up>light.up && at::shiftBands(1,tune).up>.9,"medium and full demand separated");
        // Ramp physical wheel speed, acknowledge commands, apply real ratio drops.
        std::vector<double> firstShift;
        for(double fps:{30.,60.,144.}) {
            at::Controller c; auto t=sample(); double wheel=0; int shifts=0; double first=0;
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
            check(first<.31,"early first shift is not blocked by next-gear RPM floor");
            firstShift.push_back(first);
        }
        check(std::abs(firstShift.front()-firstShift.back())<.025,"economy shift timing stable across frame rates");
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
