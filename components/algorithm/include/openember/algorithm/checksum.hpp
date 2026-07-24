#ifndef OPENEMBER_ALGORITHM_CHECKSUM_HPP_
#define OPENEMBER_ALGORITHM_CHECKSUM_HPP_

#include <cstddef>
#include <cstdint>

namespace openember::algorithm {

std::uint8_t xor_checksum(const void *data, std::size_t size);
std::uint8_t sum8(const void *data, std::size_t size);

std::uint8_t crc8(const void *data, std::size_t size,
                  std::uint8_t polynomial = 0x07,
                  std::uint8_t initial = 0x00);

std::uint16_t crc16_modbus(const void *data, std::size_t size,
                           std::uint16_t initial = 0xFFFF);

std::uint16_t crc16_ccitt_false(const void *data, std::size_t size,
                                std::uint16_t initial = 0xFFFF);

std::uint32_t crc32(const void *data, std::size_t size,
                    std::uint32_t initial = 0xFFFFFFFFu);

}  // namespace openember::algorithm

#endif  // OPENEMBER_ALGORITHM_CHECKSUM_HPP_
