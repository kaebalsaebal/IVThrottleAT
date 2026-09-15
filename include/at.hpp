#pragma once
#include <array>
#include <cstdint>
#include <istream>
#include <map>
#include <string>

namespace at {
enum class VehicleClass { Passenger, Heavy, Sport, Motorcycle, SportBike, CruiserBike, StandardBike, Scooter };
struct Tune {
    double low = .22, mid = .54, high = .94;
    double down = .07, kickThrottle = .82, kickTarget = .78;
    double cooldown = .65, confirm = .12;
    double lightThrottle = .30, midThrottle = .65, lightRise = .04;
    double liftHold = .45;
    double cvt = 0, cvtLow = .32, cvtHigh = .78, cvtRate = 1.8;
};
struct Config {
    bool enabled = false; // Explicit opt-in; a verified backend is also required.
    std::map<std::string, std::map<std::string, double>> sections;
    std::map<std::string, VehicleClass> bikeClasses;
    VehicleClass bikeClass(const std::string& model) const;
    Tune resolve(VehicleClass kind, const std::string& model) const;
};
Config parseConfig(std::istream& input); // Throws on unknown or invalid options.
void validate(const Tune& tune);

struct Telemetry {
    std::uint64_t vehicle = 0; // Stable handle WITH generation, never a raw pointer.
    std::string model;
    VehicleClass kind = VehicleClass::Passenger;
    bool playerDriver = false, grounded = false, shifting = false;
    double time = 0;       // Monotonic simulation seconds, not wall clock.
    double age = 0;        // Seconds since this snapshot was acquired.
    double throttle = 0;  // Effective analog accelerator AFTER game input mapping.
    double brake = 0, speed = 0, rpm = 0; // m/s; normalized mechanical wheel/ratio signal, NOT physical RPM.
    double nativeRevs = -1, clutch = 1; // Adapter diagnostics, never used as guessed physical RPM.
    int gear = 0, gears = 0; // Forward gears are 1..gears; neutral/reverse rejected.
    std::array<double, 9> ratios{}; // Positive verified ratios, index = forward gear.
};
struct ShiftBands { double up, down, minAfterUpshift; };
ShiftBands shiftBands(double throttle, const Tune& tune);
bool valid(const Telemetry& t);
enum class Reason { Hold, Upshift, Downshift, Kickdown, Fallback };
struct Decision { int gear = 0; Reason reason = Reason::Fallback; };

class Controller {
public:
    Decision update(const Telemetry& t, const Tune& tune);
    void reset();
    double demand() const noexcept { return demand_; }
private:
    std::uint64_t vehicle_ = 0;
    double lastTime_ = -1, lastShift_ = 0, since_ = 0, requestTime_ = 0;
    int observed_ = 0, candidate_ = 0, pending_ = 0;
    double demand_ = 0, previousPedal_ = 0, liftUntil_ = 0;
};

struct CvtDecision { bool active = false; double ratio = 0, targetRevs = 0; };
class CvtController {
public:
    CvtDecision update(const Telemetry& t, const Tune& tune, double wheelSpeed, double velocityScale);
    void reset() { *this = CvtController{}; }
private:
    std::uint64_t vehicle_ = 0;
    double lastTime_ = -1, ratio_ = 0;
    double minRatio_ = 0, maxRatio_ = 0;
    int anchorGear_ = 0;
};

// All methods execute on ONE verified game/physics thread, never a polling worker.
// Backend must independently fail open if this caller stops ticking.
class Backend {
public:
    virtual ~Backend() = default;
    virtual bool verified() const noexcept = 0;
    virtual bool sample(Telemetry&) noexcept = 0;
    // Atomically validate generation, acquire/renew a one-tick control lease,
    // suppress stock shift decisions for this vehicle only, and hold/request gear.
    // false MUST leave stock control restored, even after a partial failure.
    virtual bool apply(std::uint64_t vehicle, int gear) noexcept = 0;
    // Idempotent: safely restore all owned control, including deleted vehicles.
    virtual void release() noexcept = 0;
};
class UnavailableBackend final : public Backend {
public:
    bool verified() const noexcept override { return false; }
    bool sample(Telemetry&) noexcept override { return false; }
    bool apply(std::uint64_t, int) noexcept override { return false; }
    void release() noexcept override {}
};
class Runtime {
public:
    Runtime(Backend& backend, Config config);
    Decision tick() noexcept;
    void stop() noexcept;
private:
    Backend& backend_;
    Config config_;
    Controller controller_;
    std::uint64_t active_ = 0;
    bool faulted_ = false; // Latched until explicit stop/reinitialization.
};
} // namespace at
