#include "openember/algorithm/checksum.hpp"

namespace openember::algorithm {

namespace {

const std::uint8_t *as_bytes(const void *data)
{
    return static_cast<const std::uint8_t *>(data);
}

}  // namespace

std::uint8_t xor_checksum(const void *data, std::size_t size)
{
    const auto *bytes = as_bytes(data);
    std::uint8_t value = 0;
    for (std::size_t i = 0; i < size; ++i) {
        value ^= bytes[i];
    }
    return value;
}

std::uint8_t sum8(const void *data, std::size_t size)
{
    const auto *bytes = as_bytes(data);
    std::uint8_t value = 0;
    for (std::size_t i = 0; i < size; ++i) {
        value = static_cast<std::uint8_t>(value + bytes[i]);
    }
    return value;
}

std::uint8_t crc8(const void *data, std::size_t size,
                  std::uint8_t polynomial,
                  std::uint8_t initial)
{
    const auto *bytes = as_bytes(data);
    std::uint8_t crc = initial;
    for (std::size_t i = 0; i < size; ++i) {
        crc ^= bytes[i];
        for (int bit = 0; bit < 8; ++bit) {
            crc = (crc & 0x80u) ? static_cast<std::uint8_t>((crc << 1u) ^ polynomial)
                                : static_cast<std::uint8_t>(crc << 1u);
        }
    }
    return crc;
}

std::uint16_t crc16_modbus(const void *data, std::size_t size,
                           std::uint16_t initial)
{
    const auto *bytes = as_bytes(data);
    std::uint16_t crc = initial;
    for (std::size_t i = 0; i < size; ++i) {
        crc ^= bytes[i];
        for (int bit = 0; bit < 8; ++bit) {
            crc = (crc & 0x0001u) ? static_cast<std::uint16_t>((crc >> 1u) ^ 0xA001u)
                                  : static_cast<std::uint16_t>(crc >> 1u);
        }
    }
    return crc;
}

std::uint16_t crc16_ccitt_false(const void *data, std::size_t size,
                                std::uint16_t initial)
{
    const auto *bytes = as_bytes(data);
    std::uint16_t crc = initial;
    for (std::size_t i = 0; i < size; ++i) {
        crc ^= static_cast<std::uint16_t>(bytes[i] << 8u);
        for (int bit = 0; bit < 8; ++bit) {
            crc = (crc & 0x8000u) ? static_cast<std::uint16_t>((crc << 1u) ^ 0x1021u)
                                  : static_cast<std::uint16_t>(crc << 1u);
        }
    }
    return crc;
}

std::uint32_t crc32(const void *data, std::size_t size,
                    std::uint32_t initial)
{
    const auto *bytes = as_bytes(data);
    std::uint32_t crc = initial;
    for (std::size_t i = 0; i < size; ++i) {
        crc ^= bytes[i];
        for (int bit = 0; bit < 8; ++bit) {
            crc = (crc & 1u) ? (crc >> 1u) ^ 0xEDB88320u : crc >> 1u;
        }
    }
    return ~crc;
}

}  // namespace openember::algorithm
