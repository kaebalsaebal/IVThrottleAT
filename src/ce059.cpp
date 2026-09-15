#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include "ce059.hpp"
#include "at.hpp"
#include "MinHook.h"
#include <winver.h>
#include <tlhelp32.h>
#include <mutex>
#include <algorithm>
#include <array>
#include <atomic>
#include <cmath>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <map>
#include <memory>
#include <sstream>
#include <stdexcept>
#include <vector>

extern "C" void atGate();
extern "C" void atLcpGate();
extern "C" void* atLcpGateway;
extern "C" void* atLcpSkipTarget;
extern "C" void atShiftEntryGate();
extern "C" void atUpdateEntryGate();
extern "C" void* atShiftEntryGateway;
extern "C" void* atUpdateEntryGateway;
extern "C" void* atGateway;
extern "C" void* atSkipTarget;

namespace {
// CE059 RVAs derived from the reference executable documented in CE059-EVIDENCE.md.
// Statically inspected, but not yet validated by a live driving session.
using NoticeFunction = void (__cdecl*)(const char*, const char*, unsigned, int);
NoticeFunction printNotice=nullptr;
constexpr std::uintptr_t shiftRva=0x830b97, tailRva=0x830c2e;
constexpr std::uintptr_t localIndexRva=0xc36f14, playersRva=0xda8808;
constexpr std::uintptr_t poolRva=0xee22a4, modelsRva=0xe95cd8;
constexpr std::uintptr_t pauseRva=0xd73590, clockRva=0xd735b4;
constexpr std::size_t transmissionOffset=0x1090;
std::uintptr_t image=0;
bool lcpActive=false, bikeVerified=false;
std::atomic<unsigned long> lcpCalls{0};
HMODULE self=nullptr;
std::atomic<int> state{0}; // 0 unavailable, 1 observing, 2 control, 3 fault
std::atomic_flag callbackBusy=ATOMIC_FLAG_INIT;
bool initialized=false, keyWasDown=false, controlAnnounced=false;
DWORD ownerThread=0;
std::string lastReject;
at::Config config;
at::Controller controller;
std::ofstream logFile, csv;
std::mutex logMutex;
std::atomic<unsigned long> entryCalls[2]{}, playerEntryCalls[2]{}, midPlayerCalls{0};
std::atomic_flag traceBusy=ATOMIC_FLAG_INIT;
std::uint64_t lastVehicle=0, lastControlId=0;
double lastControlTime=-1;
std::uint32_t lastSampleTime=0;
std::uint64_t rows=0;
struct Model { std::string name; at::VehicleClass kind=at::VehicleClass::Passenger; };
std::map<std::uint32_t,Model> models;

void log(const std::string& s) {
    const std::lock_guard<std::mutex> guard(logMutex);
    if (logFile) { logFile << s << '\n'; logFile.flush(); }
}
// Guarded access is a last-resort exception boundary, not proof of field semantics.
bool copyRead(void* out, std::uintptr_t address, std::size_t size) noexcept {
    if (address<0x10000 || !size || address+size<address) return false;
    __try { std::memcpy(out,reinterpret_cast<void*>(address),size); return true; }
    __except(EXCEPTION_EXECUTE_HANDLER) { return false; }
}
template<class T> T read(std::uintptr_t address) {
    T value{};
    if (!copyRead(&value,address,sizeof value)) throw std::runtime_error("Invalid telemetry address");
    return value;
}
bool writable(std::uintptr_t a, std::size_t count) noexcept {
    MEMORY_BASIC_INFORMATION m{};
    if (!VirtualQuery(reinterpret_cast<void*>(a),&m,sizeof m) || m.State!=MEM_COMMIT ||
        (m.Protect&(PAGE_GUARD|PAGE_NOACCESS))) return false;
    const auto p=m.Protect&0xff;
    return (p==PAGE_READWRITE || p==PAGE_WRITECOPY || p==PAGE_EXECUTE_READWRITE || p==PAGE_EXECUTE_WRITECOPY) &&
        a+count>=a && a+count<=reinterpret_cast<std::uintptr_t>(m.BaseAddress)+m.RegionSize;
}
// Mirrors the game's forward shift commit at C30BE1..C30C2B only.
// No writes to RPM/handling. All four fields are within one validated memory region.
bool commit(std::uintptr_t t, int oldGear, int gear, std::uint32_t time) noexcept {
    if (gear==oldGear) return true;
    if (std::abs(gear-oldGear)!=1 || !writable(t,0x1c)) return false;
    std::array<unsigned char,0x1c> backup{};
    if (!copyRead(backup.data(),t,backup.size())) return false;
    __try {
        *reinterpret_cast<std::int16_t*>(t)=static_cast<std::int16_t>(gear);
        *reinterpret_cast<float*>(t+0x10)=0.1f;
        *reinterpret_cast<std::uint16_t*>(t+2)=gear>oldGear ? 5 : 6;
        *reinterpret_cast<std::uint32_t*>(t+0x18)=time;
        return true;
    } __except(EXCEPTION_EXECUTE_HANDLER) {
        __try { std::memcpy(reinterpret_cast<void*>(t),backup.data(),backup.size()); }
        __except(EXCEPTION_EXECUTE_HANDLER) {}
        return false;
    }
}
std::uint32_t joaat(std::string s) {
    std::uint32_t h=0;
    for (unsigned char c:s) { if(c>='A'&&c<='Z') c+=32; h+=c; h+=h<<10; h^=h>>6; }
    h+=h<<3; h^=h>>11; h+=h<<15; return h;
}
std::string trim(std::string s) {
    const auto a=s.find_first_not_of(" \t\r\n");
    return a==std::string::npos ? "" : s.substr(a,s.find_last_not_of(" \t\r\n")-a+1);
}
void addModel(std::string name) {
    name=trim(name);
    if(name.empty()) return;
    for(char& c:name) if(c>='a'&&c<='z') c-=32;
    Model m{name};
    const std::string heavy=" BUS BENSON BIFF BOXVILLE BURRITO BURRITO2 FLATBED MULE PHANTOM PONY STOCKADE STEED YANKEE AMBULANCE FIRETRUK TRASH ROMERO ";
    const std::string sport=" BANSHEE COMET COQUETTE INFERNUS SUPERGT TURISMO F620 BULLET GT SULTANRS ";
    if(heavy.find(" "+name+" ")!=std::string::npos) m.kind=at::VehicleClass::Heavy;
    else if(sport.find(" "+name+" ")!=std::string::npos) m.kind=at::VehicleClass::Sport;
    models[joaat(name)]=m;
}
void loadModels(const std::filesystem::path& root) {
    for(const auto* suffix:{L"common/data/vehicles.ide",L"TLAD/common/data/vehicles.ide",L"TBoGT/common/data/vehicles.ide",
                            L"update/common/data/vehicles.ide",L"update/TLAD/common/data/vehicles.ide",L"update/TBoGT/common/data/vehicles.ide"}) {
        std::ifstream f(root/suffix); std::string line; bool cars=false;
        while(std::getline(f,line)) {
            line=trim(line); if(line=="cars") {cars=true;continue;} if(line=="end") {cars=false;continue;}
            if(!cars||line.empty()||line.front()=='#') continue;
            const auto comma=line.find(','); if(comma!=std::string::npos) addModel(line.substr(0,comma));
        }
    }
    for(const auto* name:{"SULTAN","FAGGIO"}) addModel(name);
    for(const auto& section:config.sections) if(section.first.rfind("Model:",0)==0) addModel(section.first.substr(6));
}
bool supportedVersion(const std::filesystem::path& file) {
    DWORD ignored=0;
    const DWORD size=GetFileVersionInfoSizeW(file.c_str(),&ignored);
    if(!size) return false;
    std::vector<unsigned char> data(size);
    if(!GetFileVersionInfoW(file.c_str(),0,size,data.data())) return false;
    VS_FIXEDFILEINFO* info=nullptr; UINT length=0;
    if(!VerQueryValueW(data.data(),L"\\",reinterpret_cast<void**>(&info),&length) ||
       !info || length<sizeof(*info)) return false;
    return info->dwSignature==0xfeef04bd && info->dwFileVersionMS==0x00010002 && info->dwFileVersionLS==59;
}
// CE059 PRINT_STRING_WITH_LITERAL_STRING_NOW implementation, statically inspected.
// Called on the same game thread as the vehicle update, never from a worker thread.
void notifyMode(int mode) noexcept {
    if(!printNotice) return;
    const char* message=mode==4 ? "ThrottleAT: ACTIVE" :
                        mode==2 ? "ThrottleAT: ON (waiting for vehicle)" :
                        mode==1 ? "ThrottleAT: OFF (stock shifts)" : "ThrottleAT: ERROR (stock shifts)";
    __try { printNotice("STRING",message,3000,1); }
    __except(EXCEPTION_EXECUTE_HANDLER) { printNotice=nullptr; }
}
bool matches(std::uintptr_t va, std::initializer_list<unsigned char> expected) {
    std::vector<unsigned char> data(expected.size());
    return copyRead(data.data(),va,data.size())&&std::equal(data.begin(),data.end(),expected.begin());
}
std::uintptr_t callTarget(std::uintptr_t address) {
    if(read<unsigned char>(address)!=0xe8) return 0;
    return address+5+read<std::int32_t>(address+1);
}
bool verifiedLcpLayout(std::uintptr_t module) {
    // Exact instruction windows from the user-provided module, not guessed offsets.
    // Address operands are excluded from these windows to support module rebasing.
    return matches(module+0xe6d0,{0x55,0x8b,0xec,0x83,0xe4,0xf0,0x83,0xec,0x38}) &&
        callTarget(module+0xe849)==module+0xea00 &&
        matches(module+0xea00,{0x55,0x8b,0xec,0x83,0xec,0x10}) &&
        matches(module+0xebcb,{0xf3,0x0f,0x5e,0x52,0x4c,0x0f,0xb7,0x07,0x0f,0xbf,0xf8,0x89,0x7d,0x14}) &&
        matches(module+0xec61,{0x66,0x89,0x06,0x8b,0x45,0xf8,0x89,0x01,0xc7,0x47,0x10,0xcd,0xcc,0xcc,0x3d}) &&
        matches(module+0xec70,{0x5f,0x5e,0x8b,0xe5,0x5d,0xc2,0x18,0x00});
}
void moduleSnapshot() {
    HANDLE snapshot=CreateToolhelp32Snapshot(TH32CS_SNAPMODULE,GetCurrentProcessId());
    if(snapshot==INVALID_HANDLE_VALUE) { log("MODULE inventory unavailable"); return; }
    MODULEENTRY32W m{}; m.dwSize=sizeof m;
    if(Module32FirstW(snapshot,&m)) do {
        std::ostringstream out; out<<"MODULE "<<std::filesystem::path(m.szModule).u8string()
            <<" base=0x"<<std::hex<<reinterpret_cast<std::uintptr_t>(m.modBaseAddr)<<" size=0x"<<m.modBaseSize;
        log(out.str());
    } while(Module32NextW(snapshot,&m));
    CloseHandle(snapshot);
}
void codeSnapshot() {
    for(auto rva:{std::uintptr_t(0x82f9a0),std::uintptr_t(0x82fb3a),std::uintptr_t(0x8309d0),shiftRva,std::uintptr_t(0x835672)}) {
        std::array<unsigned char,12> bytes{};
        std::ostringstream out; out<<"CODE rva=0x"<<std::hex<<rva<<" bytes=";
        if(copyRead(bytes.data(),image+rva,bytes.size())) {
            for(auto b:bytes) out<<std::setw(2)<<std::setfill('0')<<static_cast<unsigned>(b)<<' ';
            if(bytes[0]==0xe8||bytes[0]==0xe9) {
                std::int32_t rel=0; std::memcpy(&rel,bytes.data()+1,4);
                const auto target=image+rva+5+rel; MEMORY_BASIC_INFORMATION region{};
                VirtualQuery(reinterpret_cast<void*>(target),&region,sizeof region);
                out<<"target=0x"<<target<<" allocation=0x"<<reinterpret_cast<std::uintptr_t>(region.AllocationBase);
            }
        } else out<<"unreadable";
        log(out.str());
    }
}
void installEntryTrace(std::uintptr_t rva,void* detour,void** gateway,const char* name,bool signature) {
    const auto target=reinterpret_cast<void*>(image+rva);
    if(!signature) { log(std::string("TRACE unavailable (entry differs): ")+name); return; }
    if(MH_CreateHook(target,detour,gateway)!=MH_OK) {log(std::string("TRACE create failed: ")+name);return;}
    if(MH_EnableHook(target)!=MH_OK) {MH_RemoveHook(target);log(std::string("TRACE enable failed: ")+name);return;}
    log(std::string("TRACE installed: ")+name);
}
std::uint64_t identity(std::uintptr_t vehicle) {
    const auto pool=read<std::uintptr_t>(image+poolRva);
    const auto storage=read<std::uintptr_t>(pool), flags=read<std::uintptr_t>(pool+4);
    const auto count=read<std::uint32_t>(pool+8), stride=read<std::uint32_t>(pool+12);
    if(count==0||count>65535||stride<0x1308||stride>0x10000||vehicle<storage||(vehicle-storage)%stride) return 0;
    const auto slot=(vehicle-storage)/stride; if(slot>=count) return 0;
    const auto generation=read<std::uint8_t>(flags+slot); if(generation&0x80) return 0;
    // Pool pointer plus native generation/index, rather than vehicle pointer alone.
    return (static_cast<std::uint64_t>(pool)<<32)|(slot<<8)|generation;
}
bool playerIsDriver(std::uintptr_t vehicle) {
    const auto index=read<std::int32_t>(image+localIndexRva);
    if(index<0||index>=32) return false;
    const auto info=read<std::uintptr_t>(image+playersRva+index*4); if(!info) return false;
    const auto ped=read<std::uintptr_t>(info+0x598); if(!ped) return false;
    return (read<std::uint8_t>(ped+0x26c)&4) && read<std::uintptr_t>(ped+0xb30)==vehicle &&
            read<std::uintptr_t>(vehicle+0xf50)==ped;
}
void record(const at::Telemetry& t, const at::Decision& d, bool applied, const char* label) {
    const auto now=static_cast<std::uint32_t>(t.time*1000);
    const bool change=d.reason!=at::Reason::Hold || t.vehicle!=lastVehicle;
    if(csv&&rows<60000&&(change||now-lastSampleTime>=100)) {
        const auto tune=config.resolve(t.kind,t.model);
        const double demand=state.load()==2 ? controller.demand() : t.throttle;
        const auto bands=at::shiftBands(demand,tune);
        csv<<now<<','<<state.load()<<','<<t.vehicle<<','<<t.model<<','<<t.throttle<<','<<t.brake<<','<<t.speed<<','
           <<t.rpm<<','<<t.gear<<','<<d.gear<<','<<static_cast<int>(d.reason)<<','<<applied<<','<<label<<','<<t.nativeRevs<<','<<t.clutch<<','<<demand<<','<<bands.up<<','<<bands.down<<'\n';
        if(change||rows%10==0) csv.flush(); ++rows; lastSampleTime=now;
    }
    lastVehicle=t.vehicle;
}
// Diagnostic revision: input is independent of player ownership checks.
void processToggle(bool down) {
    if(down&&!keyWasDown) {
        state=state.load()==2?1:2; controller.reset(); controlAnnounced=false;
        log(state.load()==2?"CONTROL enabled (F8)":"OBSERVE enabled (F8): stock shifts");
        notifyMode(state.load());
    }
    keyWasDown=down;
}
void traceCallback(std::uintptr_t t, std::uintptr_t handling) {
    static unsigned reports=0; static ULONGLONG last=0;
    const auto now=GetTickCount64();
    if(reports>=120 || (reports && now-last<1000)) return;
    last=now; ++reports;
    if(reports==1 || reports==10) { moduleSnapshot(); codeSnapshot(); }
    log("ROUTE update="+std::to_string(entryCalls[1].load())+" updatePlayer="+std::to_string(playerEntryCalls[1].load())+
        " shift="+std::to_string(entryCalls[0].load())+" shiftPlayer="+std::to_string(playerEntryCalls[0].load())+
        " midPlayer="+std::to_string(midPlayerCalls.load()));
    std::int32_t index=-999; std::uintptr_t info=0,ped=0,playerVehicle=0,driver=0;
    std::uint8_t flags=0;
    copyRead(&index,image+localIndexRva,sizeof index);
    if(index>=0&&index<32) copyRead(&info,image+playersRva+index*4,sizeof info);
    if(info) copyRead(&ped,info+0x598,sizeof ped);
    if(ped) { copyRead(&flags,ped+0x26c,sizeof flags); copyRead(&playerVehicle,ped+0xb30,sizeof playerVehicle); }
    const auto candidate=t>=transmissionOffset?t-transmissionOffset:0;
    if(candidate) copyRead(&driver,candidate+0xf50,sizeof driver);
    std::ostringstream out;
    out<<"DIAG callback #"<<reports<<" thread="<<GetCurrentThreadId()<<" mode="<<state.load()
       <<" playerIndex="<<index<<std::hex<<" transmission=0x"<<t<<" handling=0x"<<handling
       <<" candidate=0x"<<candidate<<" ped=0x"<<ped<<" pedFlags=0x"<<static_cast<unsigned>(flags)
       <<" playerVehicle=0x"<<playerVehicle<<" candidateDriver=0x"<<driver;
    if(playerVehicle) {
        std::uintptr_t actualDriver=0,actualHandling=0;
        std::int16_t actualGear=-99;
        float actualRevs=-99,actualGas=-99,actualClutch=-99;
        std::uint32_t actualType=0xffffffff;
        copyRead(&actualDriver,playerVehicle+0xf50,sizeof actualDriver);
        copyRead(&actualHandling,playerVehicle+0xdc8,sizeof actualHandling);
        copyRead(&actualGear,playerVehicle+0x1090,sizeof actualGear);
        copyRead(&actualRevs,playerVehicle+0x1094,sizeof actualRevs);
        copyRead(&actualGas,playerVehicle+0x1078,sizeof actualGas);
        copyRead(&actualClutch,playerVehicle+0x10a0,sizeof actualClutch);
        copyRead(&actualType,playerVehicle+0x1304,sizeof actualType);
        out<<" PLAYER driver=0x"<<actualDriver<<" handling=0x"<<actualHandling<<std::dec
           <<" type="<<actualType<<" gear="<<actualGear<<" nativeRevs="<<actualRevs
           <<" gas="<<actualGas<<" clutch="<<actualClutch;
    }
    log(out.str());
}
int dispatch(std::uintptr_t t, std::uintptr_t handling, const std::uint32_t* stack) {
    if(state.load()==0) return 0;
    // High-volume route diagnostics are retained for source debugging but disabled here.
    if(!ownerThread) ownerThread=GetCurrentThreadId();
    if(ownerThread!=GetCurrentThreadId()) { state=3; return 0; }
    DWORD foregroundProcess=0;
    GetWindowThreadProcessId(GetForegroundWindow(),&foregroundProcess);
    processToggle(foregroundProcess==GetCurrentProcessId() && (GetAsyncKeyState(VK_F8)&0x8000)!=0);
    const auto vehicle=t-transmissionOffset;
    if(t<transmissionOffset||!playerIsDriver(vehicle)) return 0; // AI still never receives control.
    ++midPlayerCalls;
    static bool playerSeen=false;
    if(!playerSeen) { playerSeen=true; log("Player driver matched; transmission telemetry path reached."); }
    const auto reject=[](const char* why) {
        controller.reset();
        if(lastReject!=why) { log(std::string("Stock fallback: ")+why); lastReject=why; }
        return 0;
    };
    if(state.load()==3) return 0;
    if(read<std::uint8_t>(image+pauseRva)||read<std::uint8_t>(image+pauseRva+1)) { controller.reset(); return 0; }
    const auto type=read<std::uint32_t>(vehicle+0x1304);
    if((type!=0 && type!=1) || read<std::uintptr_t>(vehicle+0xdc8)!=handling)
        return reject("unsupported type or handling mismatch");
    if(type==1 && !bikeVerified) return reject("bike route not verified");
    at::Telemetry sample;
    sample.vehicle=identity(vehicle); if(!sample.vehicle) return reject("vehicle pool identity unavailable");
    sample.time=read<std::uint32_t>(image+clockRva)/1000.0;
    sample.playerDriver=true; sample.gear=read<std::int16_t>(t); sample.gears=read<std::uint8_t>(handling+0x40);
    const float gameRevs=read<float>(t+4); sample.rpm=gameRevs;
    const float gas=read<float>(vehicle+0x1078), brake=read<float>(vehicle+0x107c), clutch=read<float>(t+0x10);
    if(!std::isfinite(gas)||gas<0||gas>1.01f||!std::isfinite(brake)||std::abs(brake)>1.01f||!std::isfinite(clutch)||clutch<0||clutch>1.01f) { controller.reset(); return 0; }
    sample.nativeRevs=gameRevs; sample.clutch=clutch;
    sample.throttle=std::min(1.0,static_cast<double>(gas)); sample.brake=std::min(1.0,std::abs(static_cast<double>(brake)));
    // In first gear both vanilla and LCP deliberately cap the clutch below
    // 1 at low revs. This is launch slip, not an outstanding gear change.
    // Use wheel/ratio RPM for early 1->2; the controller still debounces and
    // enforces its cooldown. In higher gears wait for clutch reengagement.
    sample.shifting=sample.gear>1 && clutch<.99f;
    // Original stack +0x10 vehicle slot has its high byte overwritten by vanilla.
    // Never use that slot! +0x1c/+0x20 are averaged wheel and longitudinal speeds.
    const auto sp=reinterpret_cast<std::uintptr_t>(stack);
    const float wheel=read<float>(sp+0x1c), longitudinal=read<float>(sp+0x20);
    sample.speed=std::abs(static_cast<double>(longitudinal));
    if(!std::isfinite(wheel)||!std::isfinite(longitudinal)||longitudinal<-.1f||
       std::abs(wheel-longitudinal)>std::max(3.0,sample.speed*.35)) { controller.reset(); return 0; }
    const int wheels=read<int>(vehicle+0xf84); const auto wheelArray=read<std::uintptr_t>(vehicle+0xf80);
    sample.grounded=wheels>=2&&wheels<=8&&wheelArray && (type!=1 || wheels==2);
    for(int i=0;sample.grounded&&i<wheels;++i) sample.grounded=(read<std::uint32_t>(wheelArray+i*0x170+0x164)&1)!=0;
    if(sample.gears>0&&sample.gears<=8) for(int i=1;i<=sample.gears;++i) sample.ratios[i]=read<float>(handling+0x54+i*4);
    const auto modelIndex=read<std::int16_t>(vehicle+0x2e);
    if(modelIndex<0) return 0;
    const auto modelInfo=read<std::uintptr_t>(image+modelsRva+static_cast<std::uintptr_t>(modelIndex)*4);
    const auto hash=read<std::uint32_t>(modelInfo+0x3c);
    const auto m=models.find(hash);
    if(m!=models.end()) { sample.model=m->second.name; sample.kind=m->second.kind; }
    if(type==1) sample.kind=at::VehicleClass::Motorcycle; // Includes FAGGIO: conventional AT fallback, not stock.
        const float flatVelocity=read<float>(handling+0x4c);
    if(!std::isfinite(flatVelocity)||flatVelocity<=1||!std::isfinite(gameRevs)||gameRevs<0||gameRevs>1.2f)
        return reject("invalid revs or handling velocity");
    // This is the normalized wheel/ratio signal used by stock forward shifts.
    // Native display revs are smoothed and sawtooth at the limiter. Use the
    // mechanical estimate in first gear or once the clutch reengages in higher gears.
    if(!sample.shifting&&sample.gear>=1&&sample.gear<=sample.gears&&sample.gear<=8)
        sample.rpm=std::abs(wheel)*sample.ratios[sample.gear]/flatVelocity;
    if(!at::valid(sample)) { record(sample,{},false,"invalid"); return reject("invalid sample / incomplete wheel contact / ratios"); }
    if(!lastReject.empty()) { log("Valid player telemetry recovered."); lastReject.clear(); }
    if(state.load()!=2) { record(sample,{sample.gear,at::Reason::Hold},false,"observe"); controller.reset(); return 0; }
        // Reverse/engine-off/vehicle-exit can bypass this hook entirely. Expected
    // lifecycle gaps reset the controller and give vanilla one tick, then resume.
    const bool discontinuity=lastControlId!=sample.vehicle || lastControlTime<0 ||
        sample.time<lastControlTime || sample.time-lastControlTime>.25;
    lastControlId=sample.vehicle; lastControlTime=sample.time;
    if(discontinuity) { controller.reset(); record(sample,{sample.gear,at::Reason::Hold},false,"resync"); return 0; }
    const auto decision=controller.update(sample,config.resolve(sample.kind,sample.model));
    if(decision.reason==at::Reason::Fallback) { state=3; notifyMode(3); log("Fallback latched: controller time gap or shift timeout. F8 retries."); record(sample,decision,false,"fault"); return 0; }
    if(!playerIsDriver(vehicle)||identity(vehicle)!=sample.vehicle||!commit(t,sample.gear,decision.gear,read<std::uint32_t>(image+clockRva))) {
        state=3; notifyMode(3); log("Fallback latched: identity/control failure."); return 0;
    }
    if(!controlAnnounced) {
        controlAnnounced=true;
        log("PLAYER CONTROL APPLIED: custom forward gear decisions active.");
        notifyMode(4);
    }
    if(decision.gear!=sample.gear) {
        std::ostringstream out; out<<"SHIFT "<<sample.model<<' '<<sample.gear<<"->"<<decision.gear
            <<" throttle="<<sample.throttle<<" filtered="<<controller.demand()<<" controlRpm="<<sample.rpm<<" nativeRevs="<<gameRevs
            <<" clutch="<<clutch<<" up="<<at::shiftBands(controller.demand(),config.resolve(sample.kind,sample.model)).up;
        log(out.str());
    }
    record(sample,decision,true,type==1?"control_motorcycle":(lcpActive?"control_lcp":"control"));
    return 1; // This invocation alone skips vanilla forward gear selection.
}
}

extern "C" void __cdecl atTraceEntry(unsigned kind,std::uintptr_t t,const std::uint32_t* stack) noexcept {
    if(kind>1) return;
    ++entryCalls[kind];
    if(traceBusy.test_and_set()) return;
    try {
        std::uintptr_t vehicle=0;
        if(copyRead(&vehicle,reinterpret_cast<std::uintptr_t>(stack)+4,sizeof vehicle) && vehicle && playerIsDriver(vehicle)) {
            const auto count=++playerEntryCalls[kind];
            if(count<=8) {
                std::ostringstream out; out<<"ENTRY PLAYER kind="<<(kind?"update":"shift")<<" thread="<<GetCurrentThreadId()
                    <<std::hex<<" vehicle=0x"<<vehicle<<" transmission=0x"<<t
                    <<" expected=0x"<<vehicle+transmissionOffset<<" caller=0x"<<read<std::uintptr_t>(reinterpret_cast<std::uintptr_t>(stack));
                log(out.str());
            }
        }
    } catch(...) {} // Diagnostics must never prevent the original function.
    traceBusy.clear();
}

extern "C" int __cdecl atDispatch(std::uintptr_t t,std::uintptr_t handling,const std::uint32_t* stack) noexcept {
    if(callbackBusy.test_and_set()) return 0;
    int result=0;
    try { result=dispatch(t,handling,stack); }
    catch(...) { state=3; notifyMode(3); try { log("Fallback latched: telemetry access exception. F8 retries."); } catch(...) {} } // Restore original branch; do not allow C++ exceptions into game.
    callbackBusy.clear(); return result;
}
// LCP+EBCB has a distinct ABI: EDI=transmission, EDX=handling, EBP=frame.
// [EBP+08] no longer holds a vehicle pointer; never read it as one.
extern "C" int __cdecl atLcpDispatch(std::uintptr_t t,std::uintptr_t handling,const std::uint32_t* frame) noexcept {
    ++lcpCalls;
    std::array<std::uint32_t,10> normalized{};
    const auto fp=reinterpret_cast<std::uintptr_t>(frame);
    if(!copyRead(&normalized[7],fp+0x14,sizeof(float)) || !copyRead(&normalized[8],fp+0x18,sizeof(float))) return 0;
    return atDispatch(t,handling,normalized.data());
}
namespace ce059 {
int status() noexcept { return state.load(); }
void deactivate() noexcept { if(state.load()!=0) state=1; } // Pass-through; installed/pinned gate stays valid until process exit.
int initialize(HMODULE module) noexcept {
    if(initialized) return state.load(); initialized=true; self=module;
    try {
        wchar_t buffer[32768]{};
        DWORD n=GetModuleFileNameW(module,buffer,32768); if(!n||n>=32768) return 0;
        auto basePath=std::filesystem::path(buffer);
        logFile.open(std::filesystem::path(basePath).replace_extension(L".log"),std::ios::trunc);
        log("ThrottleAT 0.4.0 adaptive AT and motorcycles CE059 Windows: live driving validation pending");
        n=GetModuleFileNameW(nullptr,buffer,32768); if(!n||n>=32768) return 0;
        const auto exe=std::filesystem::path(buffer);
        if(!supportedVersion(exe)) { log("Requires EXE file version 1.2.0.59: no hook installed."); return 0; }
        log("EXE version 1.2.0.59 accepted; whole-file hash is not checked.");
        image=reinterpret_cast<std::uintptr_t>(GetModuleHandleW(nullptr));
        // Verify the actual integration instructions; version alone is insufficient.
        if(!matches(image+shiftRva,{0xf3,0x0f,0x5e,0x76,0x4c,0x0f,0xb7,0x11,0x0f,0xbf,0xfa})||
           !matches(image+tailRva,{0x5f,0x5e,0x59,0xc2,0x18,0x00})||
           !matches(image+0x4d20e0,{0x8b,0x44,0x24,0x04,0x85,0xc0,0x75,0x18})||
           !matches(image+0x7cbfc3,{0xd9,0x80,0x94,0x10,0x00,0x00})||
           !matches(image+0x7cbfe3,{0x0f,0xbf,0x80,0x90,0x10,0x00,0x00})) {
            log("Runtime bytes differ: hook conflict or unsupported layout. No hook installed."); return 0;
        }
        // Native hash 0x0CA539D6 registers wrapper RVA 0x78D0D0, which calls
        // cdecl implementation RVA 0x790A00. Verify wrapper and implementation.
        if(matches(image+0x78d0d0,{0x8b,0x44,0x24,0x04,0x8b,0x40,0x08,0xff,0x70,0x0c,
                                  0xff,0x70,0x08,0xff,0x70,0x04,0xff,0x30,0xe8,0x19,0x39,0,0,0x83,0xc4,0x10,0xc3}) &&
           matches(image+0x790a00,{0x56,0x6a,0,0xff,0x74,0x24,0x0c,0xe8,0x64,0x19,0,0,0x83,0xc4,0x08}))
            printNotice=reinterpret_cast<NoticeFunction>(image+0x790a00);
        else log("In-game notification unavailable: notification code differs.");
        std::ifstream ini(std::filesystem::path(basePath).replace_extension(L".ini"));
        if(!ini) { log("Missing INI: no hook installed."); return 0; }
        config=at::parseConfig(ini); loadModels(exe.parent_path());
        csv.open(std::filesystem::path(basePath).replace_extension(L".csv"),std::ios::trunc);
        csv<<"time_ms,mode,vehicle_id,model,throttle,brake,speed_mps,control_rpm,gear,requested,reason,applied,status,native_revs,clutch,filtered_throttle,up_threshold,down_threshold\n";
        csv.flush();
        // Bike physics VA CED298 calls the same CTransmission::process.
        // Verify the shared layout and unmodified caller; other routes leave bikes alone.
        bikeVerified=callTarget(image+0x8ed298)==image+0x82f9a0 &&
            matches(image+0x8ed282,{0x8d,0x8f,0x90,0x10,0,0}) &&
            matches(image+0x8ed154,{0x8b,0xb7,0x84,0x0f,0,0}) &&
            matches(image+0x8ed1b9,{0xf6,0x80,0x64,0x01,0,0,0x01}) &&
            matches(image+0x8ed24a,{0x81,0xc2,0x70,0x01,0,0}) &&
            matches(image+0x82fea4,{0x83,0xb9,0x04,0x13,0,0,0x01});
        log(bikeVerified ? "Motorcycle route verified: bikes and scooters use ThrottleAT automatic shifts; CVT unavailable."
                         : "Motorcycle route differs: motorcycle control unavailable.");
        if(MH_Initialize()!=MH_OK) { log("MinHook initialization failed."); return 0; }
        const auto target=reinterpret_cast<void*>(image+shiftRva);
        void* lcpTarget=nullptr;
        const auto engineRoute=callTarget(image+0x835672);
        const auto lcp=reinterpret_cast<std::uintptr_t>(GetModuleHandleW(L"LibertyCityPlates.asi"));
        if(engineRoute!=image+0x82f9a0) {
            if(!lcp || engineRoute!=lcp+0xe6d0 || !verifiedLcpLayout(lcp)) {
                MH_Uninitialize(); log("Unsupported automobile engine route: no control hook installed."); return 0;
            }
            lcpTarget=reinterpret_cast<void*>(lcp+0xebcb);
            atLcpSkipTarget=reinterpret_cast<void*>(lcp+0xec70);
            log("LibertyCityPlates PatchEngine route verified; installing compatible forward-shift hook.");
        }
        if(MH_CreateHook(target,reinterpret_cast<void*>(&atGate),&atGateway)!=MH_OK) {
            MH_Uninitialize(); log("Hook creation failed."); return 0;
        }
        if(lcpTarget && MH_CreateHook(lcpTarget,reinterpret_cast<void*>(&atLcpGate),&atLcpGateway)!=MH_OK) {
            MH_RemoveHook(target); MH_Uninitialize(); log("LCP hook creation failed; no control installed."); return 0;
        }
        atSkipTarget=reinterpret_cast<void*>(image+tailRva);
        // A pinned DLL makes hot-unload impossible while other threads can use the gate.
        HMODULE pinned=nullptr;
        if(!GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS|GET_MODULE_HANDLE_EX_FLAG_PIN,
                             reinterpret_cast<LPCWSTR>(&atGate),&pinned)) {
            if(lcpTarget) MH_RemoveHook(lcpTarget);
            MH_RemoveHook(target); MH_Uninitialize(); log("Module pin failed."); return 0;
        }
        // Pin LCP too: both the gateway and skipped epilogue point into its code.
        if(lcpTarget && !GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS|GET_MODULE_HANDLE_EX_FLAG_PIN,
                                         reinterpret_cast<LPCWSTR>(lcpTarget),&pinned)) {
            MH_RemoveHook(lcpTarget); MH_RemoveHook(target); MH_Uninitialize(); log("LCP module pin failed."); return 0;
        }
        lcpActive=lcpTarget!=nullptr;
        state=config.enabled?2:1;
        if(MH_QueueEnableHook(target)!=MH_OK || (lcpTarget && MH_QueueEnableHook(lcpTarget)!=MH_OK) || MH_ApplyQueued()!=MH_OK) {
            state=0; MH_Uninitialize(); log("Hook enable failed; original gear selection retained."); return 0;
        }
        log(lcpActive?"LCP COMPATIBILITY ACTIVE: engine/RPM processing retained; forward shift decisions connected.":"Original game forward-shift route active.");
        log(config.enabled?"Hook installed. CONTROL enabled. F8 switches to stock shifts.":"Hook installed. OBSERVE only. F8 enables experimental control.");
        return state.load();
    } catch(const std::exception& e) { state=0; try { log(std::string("Initialization failed: ")+e.what()); } catch(...) {} return 0; }
    catch(...) { state=0; return 0; }
}
}
