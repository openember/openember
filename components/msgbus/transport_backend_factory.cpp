/*
 * Transport backend factory: build-time selection only.
 *
 * MsgBusNode / TransportBackend:
 *   - LCM + OPENEMBER_MSGBUS_LCM_USE_CPP=ON: lcm_transport_backend.cpp (lcm::LCM)
 *   - otherwise: transport_backend_clib.cpp → msg_bus_* (lcm_wrapper.c / nng_wrapper.c / …)
 *
 * Direct msg_bus_* C API still resolves to the same C wrappers regardless of the above.
 */
#include "transport_backend.hpp"

#include <memory>

namespace openember::msgbus {

std::unique_ptr<TransportBackend> detail_create_clib_transport_backend();

#if defined(EMBER_LIBS_USING_LCM) && (OPENEMBER_MSGBUS_LCM_USE_CPP == 1)
std::unique_ptr<TransportBackend> detail_create_lcm_cpp_transport_backend();
#endif

std::unique_ptr<TransportBackend> CreateDefaultTransportBackend()
{
#if defined(EMBER_LIBS_USING_LCM) && (OPENEMBER_MSGBUS_LCM_USE_CPP == 1)
    return detail_create_lcm_cpp_transport_backend();
#else
    return detail_create_clib_transport_backend();
#endif
}

} // namespace openember::msgbus
