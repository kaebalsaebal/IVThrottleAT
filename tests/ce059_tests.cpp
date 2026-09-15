// Tests the actual bridge decision/read/write path in synthetic allocated memory.
// The layouts are based on static evidence; this does NOT validate live GTA IV.
#include "../src/ce059.cpp"
#include <iostream>
#include <limits>
template<class T> void put(std::uintptr_t a,T v) { std::memcpy(reinterpret_cast<void*>(a),&v,sizeof v); }
std::string noticeText;
unsigned noticeDuration=0;
void __cdecl captureNotice(const char* key,const char* text,unsigned duration,int flag) {
    if(std::string(key)!="STRING" || flag!=1) throw std::runtime_error("notice ABI");
    noticeText=text; noticeDuration=duration;
}
int main(int argc,char** argv) {
    if(argc==2) return supportedVersion(std::filesystem::path(argv[1])) ? 0 : 1;
    int checks=0;
    const auto check=[&](bool v,const char* why){ ++checks; if(!v) throw std::runtime_error(why); };
    auto memory=VirtualAlloc(nullptr,0x1000000,MEM_COMMIT|MEM_RESERVE,PAGE_READWRITE);
    if(!memory) return 1;
    try {
        printNotice=&captureNotice;
        notifyMode(2); check(noticeText=="ThrottleAT: ON (waiting for vehicle)" && noticeDuration==3000,"on notification");
        notifyMode(1); check(noticeText=="ThrottleAT: OFF (stock shifts)","off notification");
        notifyMode(4); check(noticeText=="ThrottleAT: ACTIVE","active notification is distinct from armed");
        notifyMode(3); check(noticeText=="ThrottleAT: ERROR (stock shifts)","fault notification");
        state=1; keyWasDown=false;
        processToggle(true); check(state==2 && noticeText=="ThrottleAT: ON (waiting for vehicle)","F8 works before any player memory exists");
        processToggle(true); check(state==2,"held key does not repeat");
        processToggle(false); processToggle(true); check(state==1,"next press disables");
        processToggle(false);
        printNotice=nullptr; notifyMode(2);
        check(!supportedVersion(L"missing-executable.exe"),"missing version fails closed");
        image=reinterpret_cast<std::uintptr_t>(memory);
        const auto info=image+0x1000, ped=image+0x2000, vehicle=image+0x4000;
        const auto handling=image+0x7000, pool=image+0x8000, flags=image+0x9000;
        const auto wheelArray=image+0xa000, modelInfo=image+0xc000, trans=vehicle+0x1090;
        put(image+localIndexRva,0); put(image+playersRva,info); put(info+0x598,ped);
        put(ped+0x26c,std::uint8_t{4}); put(ped+0xb30,vehicle); put(vehicle+0xf50,ped);
        put(vehicle+0xdc8,handling); put(vehicle+0x1304,0u);
        put(image+poolRva,pool); put(pool,vehicle); put(pool+4,flags); put(pool+8,1u); put(pool+12,0x2000u);
        put(flags,std::uint8_t{1});
        put(vehicle+0x2e,std::int16_t{0}); put(image+modelsRva,modelInfo); put(modelInfo+0x3c,joaat("ADMIRAL")); addModel("ADMIRAL");
        put(vehicle+0xf84,4); put(vehicle+0xf80,wheelArray);
        for(int i=0;i<4;++i) put(wheelArray+i*0x170+0x164,1u);
        put(handling+0x40,std::uint8_t{5});
        const float ratios[]={0,3.2f,2.1f,1.5f,1.1f,.85f};
        for(int i=1;i<=5;++i) put(handling+0x54+4*i,ratios[i]);
        put(handling+0x4c,80.0f); put(vehicle+0x1078,.1f); put(vehicle+0x107c,0.0f);
        put(trans,std::int16_t{2}); put(trans+4,.55f); put(trans+0x10,1.0f);
        std::array<std::uint32_t,10> stack{};
        const auto sp=reinterpret_cast<std::uintptr_t>(stack.data());
        put(sp+0x1c,20.0f); put(sp+0x20,20.0f);
        put(sp+4,vehicle);
        auto entryBefore=playerEntryCalls[0].load();
        atTraceEntry(0,trans,stack.data());
        check(playerEntryCalls[0]==entryBefore+1,"entry trace recognizes explicit player vehicle argument");
        check(read<std::int16_t>(trans)==2,"entry observer never changes gear");
        check(identity(vehicle)!=0,"generation-aware identity");
        state=1;
        check(atDispatch(trans,handling,stack.data())==0&&read<std::int16_t>(trans)==2,"observe does not write");
        state=2;
        for(unsigned i=0;i<85;++i) { put(image+clockRva,i*10); atDispatch(trans,handling,stack.data()); }
        check(read<std::int16_t>(trans)==3,"active low throttle upshift");
        check(read<std::uint16_t>(trans+2)==5,"stock upshift metadata");
        check(read<float>(trans+0x10)==.1f,"stock clutch transition");
        check(read<float>(trans+4)==.55f&&read<float>(vehicle+0x1078)==.1f,"RPM and pedal are not rewritten");
        ce059::deactivate();
        check(atDispatch(trans,handling,stack.data())==0,"deactivate immediately passes through");
        state=2; controller.reset(); put(ped+0xb30,std::uintptr_t{0});
        check(atDispatch(trans,handling,stack.data())==0,"other vehicle bypass"); put(ped+0xb30,vehicle);
        put(vehicle+0xf50,std::uintptr_t{0});
        check(atDispatch(trans,handling,stack.data())==0,"passenger bypass"); put(vehicle+0xf50,ped);
        put(flags,std::uint8_t{0x81}); check(atDispatch(trans,handling,stack.data())==0,"freed slot bypass"); put(flags,std::uint8_t{2});
        const auto generation=identity(vehicle); put(flags,std::uint8_t{3}); check(identity(vehicle)!=generation,"reuse changes identity");
        put(wheelArray+0x164,0u); check(atDispatch(trans,handling,stack.data())==0,"partial contact fallback"); put(wheelArray+0x164,1u);
        put(vehicle+0x1304,2u); check(atDispatch(trans,handling,stack.data())==0,"unsupported vehicle fallback"); put(vehicle+0x1304,0u);
        put(trans,std::int16_t{2}); put(trans+0x10,1.0f); put(handling+0x40,std::uint8_t{9});
        check(atDispatch(trans,handling,stack.data())==0,"invalid gear count fallback"); put(handling+0x40,std::uint8_t{5});
        put(sp+0x1c,50.0f); check(atDispatch(trans,handling,stack.data())==0,"wheelspin fallback"); put(sp+0x1c,20.0f);
        put(vehicle+0x1078,std::numeric_limits<float>::quiet_NaN()); check(atDispatch(trans,handling,stack.data())==0,"nan input rejected"); put(vehicle+0x1078,.1f);
        check(!commit(trans,2,5,0),"nonadjacent commit rejected");
        controller.reset(); state=2; put(image+clockRva,2000u); atDispatch(trans,handling,stack.data());
        put(image+clockRva,2400u); check(atDispatch(trans,handling,stack.data())==0&&state.load()==2,"time gap returns stock for resync without permanent fault");
        // LCP frame differs from the original midhook stack. Prove translation
        // and actual writes using first-gear launch slip and the early tune.
        config.sections["Class:Passenger"]={{"Low",.28},{"Mid",.56},{"Down",.12}};
        std::array<std::uint32_t,9> lcpFrame{};
        const auto fp=reinterpret_cast<std::uintptr_t>(lcpFrame.data());
        put(fp+8,std::numeric_limits<float>::quiet_NaN()); // clobbered vehicle slot is never read
        put(fp+0x14,10.0f); put(fp+0x18,10.0f);
        put(trans,std::int16_t{1}); put(trans+4,.32f); put(trans+0x10,.64f); put(vehicle+0x1078,.1f);
        controller.reset(); lastControlId=0; lastControlTime=-1; state=1;
        check(atLcpDispatch(trans,handling,lcpFrame.data())==0 && read<std::int16_t>(trans)==1,"LCP observe leaves original gear intact");
        state=2;
        for(unsigned i=0;i<90;++i) { put(image+clockRva,3000u+i*10); atLcpDispatch(trans,handling,lcpFrame.data()); }
        check(read<std::int16_t>(trans)==2,"LCP early first upshift despite launch clutch slip");
        check(read<std::uint16_t>(trans+2)==5 && read<float>(trans+0x10)==.1f,"LCP compatible shift metadata");
        check(read<float>(trans+4)==.32f,"LCP engine RPM value preserved");
        put(ped+0xb30,std::uintptr_t{0});
        check(atLcpDispatch(trans,handling,lcpFrame.data())==0,"LCP AI/other vehicle never controlled");
        put(ped+0xb30,vehicle);
        check(atLcpDispatch(trans,handling,nullptr)==0,"missing LCP frame safely forwards");
        // Verified bikes use the original shared gate. Scooters receive the
        // Motorcycle tune too: absence of CVT must not bypass custom control.
        config=at::Config{}; addModel("FAGGIO"); put(modelInfo+0x3c,joaat("FAGGIO"));
        put(vehicle+0x1304,1u); put(vehicle+0xf84,2);
        put(trans,std::int16_t{1}); put(trans+4,.31f); put(trans+0x10,.62f);
        put(sp+0x1c,6.5f); put(sp+0x20,6.5f); // mechanical revs=.26
        bikeVerified=false; state=2; controller.reset();
        check(atDispatch(trans,handling,stack.data())==0,"unverified bike route never writes");
        bikeVerified=true; lastControlTime=-1; lastControlId=0;
        int bikeResult=0;
        for(unsigned i=0;i<90;++i) { put(image+clockRva,5000u+i*10); bikeResult=atDispatch(trans,handling,stack.data()); }
        check(bikeResult==1 && read<std::int16_t>(trans)==1,"Faggio actively holds Scooter gear instead of passenger early shift or stock bypass");
        put(sp+0x1c,9.5f); put(sp+0x20,9.5f); // mechanical revs=.38, crosses Scooter band
        for(unsigned i=0;i<30;++i) { put(image+clockRva,5900u+i*10); atDispatch(trans,handling,stack.data()); }
        check(read<std::int16_t>(trans)==2,"Faggio conventional ThrottleAT upshift when CVT unavailable");
        check(read<float>(trans+4)==.31f,"bike native revs preserved");
        put(wheelArray+0x164,0u);
        check(atDispatch(trans,handling,stack.data())==0,"bike wheelie/contact loss restores original decision for that tick");
        put(wheelArray+0x164,1u); put(vehicle+0xf84,4);
        check(atDispatch(trans,handling,stack.data())==0,"bike inconsistent wheel count rejected");
        put(vehicle+0xf84,2); put(vehicle+0xf50,std::uintptr_t{0});
        check(atDispatch(trans,handling,stack.data())==0,"bike passenger ownership rejected");
        put(vehicle+0xf50,ped); put(vehicle+0x1304,0u); put(vehicle+0xf84,4);
        // A continuous ratio is published once, then consumed by engine RPM,
        // optional limiter and torque sites. Neither shared handling nor gear is changed.
        put(vehicle+0x1304,1u); put(vehicle+0xf84,2); put(trans,std::int16_t{1}); put(trans+0x10,1.0f);
        put(sp+0x1c,10.0f); put(sp+0x20,10.0f); cvtVerified=true; cvtFault=false;
        lastControlTime=-1; lastControlId=0; cvtController.reset(); state=2;
        std::array<unsigned char,0x100> originalHandling{};
        copyRead(originalHandling.data(),handling,originalHandling.size());
        float rpmRatio=0,limitRatio=0,torqueRatio=0;
        for(unsigned i=0;i<60;++i) {
            put(image+clockRva,7000u+i*10);
            const int applied=atDispatch(trans,handling,stack.data());
            if(cvtLease.active) {
                check(applied==1,"CVT holds the original shift decision");
                check(atCvtRatio(2,trans,handling,&torqueRatio)==0,"torque cannot consume a stale or out-of-order ratio");
                check(atCvtRatio(0,trans,handling,&rpmRatio)==1,"CVT RPM ratio");
                check(atCvtRatio(1,trans,handling,&limitRatio)==1,"CVT limiter ratio");
                check(atCvtRatio(2,trans,handling,&torqueRatio)==1,"CVT drive-force ratio");
                check(rpmRatio==limitRatio && rpmRatio==torqueRatio,"all engine sites share one ratio");
                check(atCvtRatio(0,trans,handling,&rpmRatio)==0,"consumed engine lease cannot be reused");
            }
        }
        check(rpmRatio>ratios[5] && rpmRatio<ratios[1],"continuous ratio lies between factory gears");
        check(read<std::int16_t>(trans)==1 && read<float>(trans+4)==.31f,"CVT decision does not fake RPM or force integer shifts");
        check(std::memcmp(originalHandling.data(),reinterpret_cast<void*>(handling),originalHandling.size())==0,"shared handling remains untouched");
        put(image+clockRva,7600u); atDispatch(trans,handling,stack.data());
        check(atCvtRatio(0,trans,handling+4,&rpmRatio)==0,"other handling cannot consume ratio");
        put(flags,std::uint8_t{4});
        check(atCvtRatio(0,trans,handling,&rpmRatio)==0,"recycled vehicle cannot inherit CVT ratio");
        put(image+clockRva,7610u); atDispatch(trans,handling,stack.data());
        put(image+clockRva,7611u);
        check(atCvtRatio(0,trans,handling,&rpmRatio)==0,"stale game tick cannot consume ratio");
        put(image+clockRva,7620u); atDispatch(trans,handling,stack.data()); state=1;
        check(atCvtRatio(0,trans,handling,&rpmRatio)==0,"OFF immediately disables all CVT substitutions"); state=2;
        put(image+clockRva,7630u); atDispatch(trans,handling,stack.data());
        check(atCvtRatio(0,trans,handling,&rpmRatio)==1 && atCvtRatio(2,trans,handling,&torqueRatio)==1,"limiter may be bypassed by original redline branch");
        // Re-entry and higher-gear clutch slip used to leak to stepped AT.
        for(int gear:{1,3,4}) {
            put(trans,static_cast<std::int16_t>(gear));put(trans+0x10,.6f);
            lastControlTime=-1;lastControlId=0;cvtController.reset();
            for(unsigned i=0;i<120;++i) {
                put(image+clockRva,8000u+gear*2000u+i*10);
                put(vehicle+0x1078,.4f+(i%2 ? .01f : -.01f));
                check(atDispatch(trans,handling,stack.data())==1 && cvtLease.active,"entry and pedal jitter never delegate safe CVT to stepped shifts");
                check(read<std::int16_t>(trans)==gear,"CVT retains integer gear during clutch slip");
                check(atCvtRatio(0,trans,handling,&rpmRatio)==1 && atCvtRatio(2,trans,handling,&torqueRatio)==1,"clutch slip retains engine ratio lease");
            }
        }
        config.sections["Class:Scooter"]["Cvt"]=0;
        put(image+clockRva,7640u); atDispatch(trans,handling,stack.data());
        check(!cvtLease.active,"Cvt=0 selects Scooter stepped AT");
        std::cout<<checks<<" CE059 bridge checks passed\n";
    } catch(const std::exception& e) { std::cerr<<e.what()<<'\n'; VirtualFree(memory,0,MEM_RELEASE);return 1; }
    VirtualFree(memory,0,MEM_RELEASE); return 0;
}
