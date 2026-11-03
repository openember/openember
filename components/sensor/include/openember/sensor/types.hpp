#pragma once

namespace openember::sensor {

struct Vector3 {
    double x = 0.0;
    double y = 0.0;
    double z = 0.0;
};

struct Quaternion {
    double w = 0.0, x = 0.0, y = 0.0, z = 0.0; // Zero is invalid, not identity.
};

}  // namespace openember::sensor
