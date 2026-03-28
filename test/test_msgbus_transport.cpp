// Smoke test: TransportBackend factory returns a non-null implementation.
#include "transport_backend.hpp"

int main()
{
    auto t = openember::msgbus::CreateDefaultTransportBackend();
    return t ? 0 : 1;
}
