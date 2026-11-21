#pragma once
#include <cstdint>
#include <string>
#include <vector>
namespace openember::sensor {
struct DiagnosticCounter { std::string name; std::uint64_t value; };
class ISensorDiagnostics {
public:
    virtual ~ISensorDiagnostics() = default;
    virtual std::vector<DiagnosticCounter> ReadDiagnostics() const = 0;
};
}
