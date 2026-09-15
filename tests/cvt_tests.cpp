#include "at.hpp"
#include <cmath>
#include <fstream>
#include <iostream>
#include <sstream>
#include <stdexcept>
int checks=0;
void check(bool ok,const char* why){++checks;if(!ok)throw std::runtime_error(why);}
int main(int argc,char** argv){try{
    if(argc!=2)throw std::runtime_error("config required");
    std::ifstream file(argv[1]); const auto cfg=at::parseConfig(file);
    check(cfg.bikeClass("AKUMA")==at::VehicleClass::SportBike,"Akuma sport class");
    check(cfg.bikeClass("FREEWAY")==at::VehicleClass::CruiserBike,"Freeway cruiser class");
    check(cfg.bikeClass("PCJ")==at::VehicleClass::StandardBike,"PCJ standard class");
    check(cfg.bikeClass("faggio")==at::VehicleClass::Scooter,"Faggio scooter class");
    check(cfg.bikeClass("ADDON")==at::VehicleClass::StandardBike,"unknown bike standard fallback");
    const auto sport=cfg.resolve(cfg.bikeClass("AKUMA"),"AKUMA");
    const auto cruiser=cfg.resolve(cfg.bikeClass("FREEWAY"),"FREEWAY");
    const auto standard=cfg.resolve(cfg.bikeClass("PCJ"),"PCJ");
    const auto scooter=cfg.resolve(cfg.bikeClass("FAGGIO"),"FAGGIO");
    check(sport.high>cruiser.high && sport.cooldown<cruiser.cooldown,"sport and cruiser shift characteristics differ");
    check(cruiser.low<standard.low && standard.low<sport.low,"low-pedal bike classes ordered");
    check(scooter.cvt==1 && sport.cvt==0,"CVT limited to scooter preset");
    std::istringstream custom("[Class:Motorcycle]\nCooldown=.8\n[Model:ADDON]\nBikeClass=CruiserBike\nLow=.3\n");
    const auto override=at::parseConfig(custom);
    check(override.bikeClass("addon")==at::VehicleClass::CruiserBike && override.resolve(override.bikeClass("addon"),"addon").low==.3,"custom model classification and tuning");
    check(override.resolve(at::VehicleClass::StandardBike,"").cooldown==.8,"legacy shared Motorcycle settings supported");
    for(const char* bad:{"[Model:X]\nBikeClass=Boat", "[Class:Scooter]\nCvt=2", "[Class:Scooter]\nCvtRate=0", "[Class:Scooter]\nCvtLow=.9\nCvtHigh=.2"}) {
        bool rejected=false;try{std::istringstream in(bad);at::parseConfig(in);}catch(...){rejected=true;}check(rejected,"invalid class/CVT settings rejected");
    }
    at::Telemetry t; t.vehicle=1;t.playerDriver=true;t.grounded=true;t.kind=at::VehicleClass::Scooter;t.model="FAGGIO";
    t.gear=1;t.gears=4;t.ratios={0,3.2,2.1,1.5,.85,0,0,0,0};t.rpm=.4;t.throttle=.3;t.speed=10;
    for(double fps:{30.,60.,144.}){
        at::CvtController c; t.time=0;double last=t.ratios[1];at::CvtDecision d;
        for(int i=0;i<int(fps*4);++i){t.time+=1/fps;d=c.update(t,scooter,20,80);
            check(d.active && d.ratio>=.85 && d.ratio<=3.2,"CVT stays within verified gear-ratio bounds");
            check(std::abs(d.ratio-last)<=3.2*scooter.cvtRate/fps+1e-8,"ratio cannot jump faster than slew limit");last=d.ratio;
        }
        check(std::abs(20*d.ratio/80-d.targetRevs)<.001,"steady wheel speed converges to target mechanical revs");
        const double oldRatio=d.ratio;t.time+=1/fps;d=c.update(t,scooter,30,80);
        check(d.ratio<oldRatio,"increasing wheel speed continuously lowers ratio");
        t.time+=1/fps;t.vehicle++;d=c.update(t,scooter,30,80);
        check(d.ratio==t.ratios[t.gear],"new vehicle starts from its existing ratio");
        t.grounded=false;t.time+=1/fps;check(!c.update(t,scooter,30,80).active,"airborne fails open");t.grounded=true;
    }
    at::CvtController c;t.time=0; t.gear=2;t.shifting=true;
    check(!c.update(t,scooter,20,80).active,"unfinished stepped shift cannot enter CVT");
    t.shifting=false;auto disabled=scooter;disabled.cvt=0;
    check(!c.update(t,disabled,20,80).active,"Cvt=0 delegates to stepped AT");
    t.gear=1;t.time=.01;check(c.update(t,scooter,0,80).ratio==3.2,"standstill uses launch ratio without division by zero");
    t.time=.02;check(!c.update(t,scooter,-1,80).active,"reverse speed rejected");
    std::cout<<checks<<" motorcycle/CVT checks passed\n";
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
