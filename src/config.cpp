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
    {"LightRise", &Tune::lightRise}, {"LiftHold", &Tune::liftHold},
    {"Cvt", &Tune::cvt}, {"CvtLow", &Tune::cvtLow}, {"CvtHigh", &Tune::cvtHigh}, {"CvtRate", &Tune::cvtRate}
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
          p.lightRise >= 0 && p.low + p.lightRise <= p.mid && p.liftHold >= 0 && p.liftHold <= 2 &&
          (p.cvt == 0 || p.cvt == 1) && p.cvtLow >= .15 && p.cvtHigh >= p.cvtLow && p.cvtHigh <= .95 &&
          p.cvtRate >= .1 && p.cvtRate <= 5))
        throw std::runtime_error("Invalid tuning ranges or hysteresis");
}
VehicleClass Config::bikeClass(const std::string& model) const {
    const auto name=upper(model);
    const auto custom=bikeClasses.find(name);
    if(custom!=bikeClasses.end()) return custom->second;
    const std::string sports=" AKUMA BATI BATI2 HAKUCHOU HAKUCHOU2 NRG900 DOUBLE DOUBLE2 ";
    const std::string cruisers=" FREEWAY ANGEL DAEMON DIABOLUS HELLFURY HEXER LYCAN NIGHTBLADE REVENANT WOLFSBANE ZOMBIE ";
    if(name=="FAGGIO") return VehicleClass::Scooter;
    if(!name.empty() && sports.find(" "+name+" ")!=std::string::npos) return VehicleClass::SportBike;
    if(!name.empty() && cruisers.find(" "+name+" ")!=std::string::npos) return VehicleClass::CruiserBike;
    return VehicleClass::StandardBike; // PCJ, VADER, SANCHEZ and unknown bike models.
}
Tune Config::resolve(VehicleClass kind, const std::string& model) const {
    Tune p;
    std::string name = "Passenger";
    switch (kind) {
    case VehicleClass::Heavy: p.low=.20; p.mid=.48; p.high=.88; p.down=.07; p.cooldown=.9; name="Heavy"; break;
    case VehicleClass::Sport: p.low=.25; p.mid=.64; p.high=.97; p.down=.09; p.cooldown=.45; name="Sport"; break;
    case VehicleClass::Motorcycle: p.low=.32; p.mid=.68; p.high=.97; p.down=.11; p.cooldown=.40; name="Motorcycle"; break;
    case VehicleClass::SportBike: p.low=.40; p.mid=.78; p.high=.98; p.down=.14; p.cooldown=.30; p.confirm=.08; p.kickThrottle=.78; p.kickTarget=.82; p.liftHold=.25; name="SportBike"; break;
    case VehicleClass::CruiserBike: p.low=.26; p.mid=.56; p.high=.90; p.down=.09; p.cooldown=.60; p.kickThrottle=.88; p.kickTarget=.72; name="CruiserBike"; break;
    case VehicleClass::StandardBike: p.low=.32; p.mid=.68; p.high=.97; p.down=.11; p.cooldown=.40; name="StandardBike"; break;
    case VehicleClass::Scooter: p.low=.27; p.mid=.55; p.high=.88; p.down=.08; p.cooldown=.50; p.cvt=1; name="Scooter"; break;
    default: break;
    }
    // Legacy shared motorcycle settings remain a base; the four new classes override them.
    if(kind==VehicleClass::SportBike || kind==VehicleClass::CruiserBike || kind==VehicleClass::StandardBike || kind==VehicleClass::Scooter)
        overlay(p, *this, "Class:Motorcycle");
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
                     section != "Class:Sport" && section != "Class:Motorcycle" && section != "Class:SportBike" && section != "Class:CruiserBike" &&
                     section != "Class:StandardBike" && section != "Class:Scooter") throw std::runtime_error("Unknown section");
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
            if(key=="BikeClass" && section.rfind("Model:",0)==0) {
                const std::map<std::string,VehicleClass> choices={{"SPORTBIKE",VehicleClass::SportBike},{"CRUISERBIKE",VehicleClass::CruiserBike},{"STANDARDBIKE",VehicleClass::StandardBike},{"SCOOTER",VehicleClass::Scooter}};
                const auto choice=choices.find(upper(value));
                if(choice==choices.end()) throw std::runtime_error("Invalid BikeClass");
                c.bikeClasses[upper(section.substr(6))]=choice->second;
                c.sections[section]; // Ensure model registration even for class-only overrides.
                continue;
            }
            if (!fields.count(key)) throw std::runtime_error("Unknown tuning key");
            std::size_t used = 0;
            const double v = std::stod(value, &used);
            if (used != value.size() || !std::isfinite(v)) throw std::runtime_error("Invalid numeric value");
            c.sections[section][key] = v;
        }
    }
    if (input.bad()) throw std::runtime_error("Config read failure");
    // Validate every override for every possible class before enabling control.
    for (auto kind : {VehicleClass::Passenger, VehicleClass::Heavy, VehicleClass::Sport, VehicleClass::Motorcycle, VehicleClass::SportBike, VehicleClass::CruiserBike, VehicleClass::StandardBike, VehicleClass::Scooter}) {
        c.resolve(kind, "");
        for (const auto& s : c.sections) if (s.first.rfind("Model:", 0) == 0) c.resolve(kind, s.first.substr(6));
    }
    return c;
}
} // namespace at
