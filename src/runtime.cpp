#include "at.hpp"
#include <utility>
namespace at {
Runtime::Runtime(Backend& b, Config c) : backend_(b), config_(std::move(c)) {}
void Runtime::stop() noexcept {
    backend_.release(); controller_.reset(); active_ = 0; faulted_ = false;
}
Decision Runtime::tick() noexcept {
    const auto fallback = [&]() {
        backend_.release(); controller_.reset(); active_ = 0;
        return Decision{};
    };
    try {
        if (faulted_ || !config_.enabled || !backend_.verified()) return fallback();
        Telemetry t;
        if (!backend_.sample(t) || !valid(t)) return fallback();
        if (active_ && active_ != t.vehicle) {
            backend_.release(); controller_.reset();
        }
        active_ = t.vehicle;
        const auto decision = controller_.update(t, config_.resolve(t.kind, t.model));
        if (decision.reason == Reason::Fallback) { faulted_ = true; return fallback(); }
        if (!backend_.apply(t.vehicle, decision.gear)) { faulted_ = true; return fallback(); }
        return decision;
    } catch (...) { faulted_ = true; return fallback(); }
}
} // namespace at
