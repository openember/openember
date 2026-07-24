#pragma once

#include <chrono>
#include <string>

namespace openember::link {

enum class Profile {
    kAuto,
    kPeer,
    kClient,
    kRouter,
};

struct Options {
    Profile profile = Profile::kAuto;
    std::string connect;
    std::string listen;
    std::chrono::milliseconds retry_interval{2000};
};

inline Options LocalRouterOptions() {
    Options options;
    options.profile = Profile::kRouter;
    options.listen = "tcp/127.0.0.1:7447";
    return options;
}

inline Options LocalClientOptions() {
    Options options;
    options.profile = Profile::kClient;
    options.connect = "tcp/127.0.0.1:7447";
    return options;
}

}  // namespace openember::link
