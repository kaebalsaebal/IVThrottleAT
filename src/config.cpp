#include "at.hpp"
#include <algorithm>
#include <cctype>
#include <cmath>
#include <stdexcept>

namespace at {
namespace {
std::string trim(std::string s) {
    const auto first = s.find_first_not_of(" \t\r\n");
    if (first == std::string::npos) return {};
    return s.substr(first, s.find_last_not_of(" \t\r\n") - first + 1);
}
std::string upper(std::string s) {
    for (char& c : s) c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
    return s;
}
const std::map<std::string, double Tune::*> fields = {
    {"Low", &Tune::low}, {"Mid", &Tune::mid}, {"High", &Tune::high},
    {"Down", &Tune::down}, {"KickThrottle", &Tune::kickThrottle},
    {"KickTarget", &Tune::kickTarget}, {"Cooldown", &Tune::cooldown}, {"Confirm", &Tune::confirm},
    {"LightThrottle", &Tune::lightThrottle}, {"MidThrottle", &Tune::midThrottle},
    {"LightRise", &Tune::lightRise}, {"LiftHold", &Tune::liftHold}
};
void overlay(Tune& p, const Config& c, const std::string& section) {
    const auto it = c.sections.find(section);
    if (it != c.sections.end()) for (const auto& kv : it->second) p.*fields.at(kv.first) = kv.second;
}
}
void validate(const Tune& p) {
    for (const auto& f : fields) if (!std::isfinite(p.*f.second)) throw std::runtime_error("Nonfinite tuning");
    if (!(p.down >= .04 && p.down + .04 < p.low && p.low <= p.mid && p.mid <= p.high && p.high <= .98 &&
          p.kickThrottle >= .5 && p.kickThrottle <= 1 && p.kickTarget > p.down && p.kickTarget <= .88 &&
          p.cooldown >= .2 && p.cooldown <= 5 && p.confirm >= .04 && p.confirm <= .5 &&
          p.lightThrottle >= .1 && p.lightThrottle <= .45 && p.midThrottle > p.lightThrottle && p.midThrottle <= .85 &&
          p.lightRise >= 0 && p.low + p.lightRise <= p.mid && p.liftHold >= 0 && p.liftHold <= 2))
        throw std::runtime_error("Invalid tuning ranges or hysteresis");
}
Tune Config::resolve(VehicleClass kind, const std::string& model) const {
    Tune p;
    std::string name = "Passenger";
    switch (kind) {
    case VehicleClass::Heavy: p.low=.20; p.mid=.48; p.high=.88; p.down=.07; p.cooldown=.9; name="Heavy"; break;
    case VehicleClass::Sport: p.low=.25; p.mid=.64; p.high=.97; p.down=.09; p.cooldown=.45; name="Sport"; break;
    case VehicleClass::Motorcycle: p.low=.32; p.mid=.68; p.high=.97; p.down=.11; p.cooldown=.40; name="Motorcycle"; break;
    default: break;
    }
    overlay(p, *this, "Class:" + name);
    overlay(p, *this, "Model:" + upper(model));
    validate(p);
    return p;
}
Config parseConfig(std::istream& input) {
    Config c;
    std::string line, section;
    while (std::getline(input, line)) {
        const auto comment = line.find_first_of(";#");
        line = trim(line.substr(0, comment));
        if (line.empty()) continue;
        if (line.front() == '[' && line.back() == ']') {
            section = trim(line.substr(1, line.size()-2));
            if (section.rfind("Model:", 0) == 0 && section.size() > 6) section = "Model:" + upper(section.substr(6));
            else if (section != "General" && section != "Class:Passenger" && section != "Class:Heavy" &&
                     section != "Class:Sport" && section != "Class:Motorcycle") throw std::runtime_error("Unknown section");
            continue;
        }
        const auto equals = line.find('=');
        if (equals == std::string::npos || section.empty()) throw std::runtime_error("Expected section/key=value");
        const auto key = trim(line.substr(0, equals));
        const auto value = trim(line.substr(equals+1));
        if (section == "General") {
            if (key != "Enabled" || (value != "0" && value != "1")) throw std::runtime_error("Expected Enabled=0 or 1");
            c.enabled = value == "1";
        } else {
            if (!fields.count(key)) throw std::runtime_error("Unknown tuning key");
            std::size_t used = 0;
            const double v = std::stod(value, &used);
            if (used != value.size() || !std::isfinite(v)) throw std::runtime_error("Invalid numeric value");
            c.sections[section][key] = v;
        }
    }
    if (input.bad()) throw std::runtime_error("Config read failure");
    // Validate every override for every possible class before enabling control.
    for (auto kind : {VehicleClass::Passenger, VehicleClass::Heavy, VehicleClass::Sport, VehicleClass::Motorcycle}) {
        c.resolve(kind, "");
        for (const auto& s : c.sections) if (s.first.rfind("Model:", 0) == 0) c.resolve(kind, s.first.substr(6));
    }
    return c;
}
} // namespace at
