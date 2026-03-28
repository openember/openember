/*
 * Transport backend factory: build-time selection only.
 *
 * The active C implementation of msg_bus_* comes from one of:
 *   lcm_wrapper.c / nng_wrapper.c / zmq_wrapper.c (stubs) / MQTT in openember_mqtt
 * as determined by CMake (OPENEMBER_MSGBUS_USE_*) and
 * core/inc/ember_config.h (EMBER_LIBS_USING_*).
 */
#include "transport_backend.hpp"

#include <memory>

namespace openember::msgbus {

std::unique_ptr<TransportBackend> detail_create_clib_transport_backend();

std::unique_ptr<TransportBackend> CreateDefaultTransportBackend()
{
    return detail_create_clib_transport_backend();
}

} // namespace openember::msgbus
