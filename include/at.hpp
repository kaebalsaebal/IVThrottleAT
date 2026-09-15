#pragma once
#include <array>
#include <cstdint>
#include <istream>
#include <map>
#include <string>

namespace at {
enum class VehicleClass { Passenger, Heavy, Sport, Motorcycle };
struct Tune {
    double low = .42, mid = .66, high = .94;
    double down = .23, kickThrottle = .82, kickTarget = .78;
    double cooldown = .65, confirm = .12;
};
struct Config {
    bool enabled = false; // Explicit opt-in; a verified backend is also required.
    std::map<std::string, std::map<std::string, double>> sections;
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
    double brake = 0, speed = 0, rpm = 0; // m/s; RPM / verified engine redline.
    int gear = 0, gears = 0; // Forward gears are 1..gears; neutral/reverse rejected.
    std::array<double, 9> ratios{}; // Positive verified ratios, index = forward gear.
};
bool valid(const Telemetry& t);
enum class Reason { Hold, Upshift, Downshift, Kickdown, Fallback };
struct Decision { int gear = 0; Reason reason = Reason::Fallback; };

class Controller {
public:
    Decision update(const Telemetry& t, const Tune& tune);
    void reset();
private:
    std::uint64_t vehicle_ = 0;
    double lastTime_ = -1, lastShift_ = 0, since_ = 0, requestTime_ = 0;
    int observed_ = 0, candidate_ = 0, pending_ = 0;
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
