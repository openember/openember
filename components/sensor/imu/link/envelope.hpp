#pragma once
#include "openember/serialization.hpp"
#include "openember/sensor/imu/link.hpp"
#include <algorithm>
#include <array>
#include <openssl/evp.h>
#include <openssl/rand.h>
#include <stdexcept>
namespace openember::sensor::link::envelope {
using Bytes = openember::transport::ByteBuffer;
constexpr std::size_t MaxBytes = 32768;
inline bool Id(const std::string &s) {
  return !s.empty() && s.size() <= 63 &&
         std::all_of(s.begin(), s.end(), [](unsigned char c) {
           return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
                  (c >= '0' && c <= '9') || c == '_' || c == '-';
         });
}
inline std::string Source(const Bytes &b) {
  if (b.size() < 34 || b.size() > MaxBytes || b[0] != 'O' || b[1] != 'E' ||
      b[2] != 'I' || b[3] != '1' || !b[4] || b[4] > 63 || b.size() < 33u + b[4])
    throw std::runtime_error("invalid IMU frame");
  const std::string source(b.begin() + 5, b.begin() + 5 + b[4]);
  if (!Id(source))
    throw std::runtime_error("invalid IMU source");
  return source;
}
inline Bytes Crypt(const Credential &c, const std::string &endpoint,
                   const Bytes &body, bool encrypt) {
  if (!Id(c.robot_id) || !Id(c.source_id) || c.secret.size() != 64 ||
      body.size() > MaxBytes)
    throw std::runtime_error("invalid IMU envelope");
  std::array<unsigned char, 32> key{};
  auto hex = [](char x) -> unsigned {
    if (x >= '0' && x <= '9')
      return x - '0';
    if (x >= 'a' && x <= 'f')
      return x - 'a' + 10;
    if (x >= 'A' && x <= 'F')
      return x - 'A' + 10;
    throw std::runtime_error("invalid IMU key");
  };
  for (unsigned i = 0; i < 32; ++i)
    key[i] = hex(c.secret[i * 2]) * 16 + hex(c.secret[i * 2 + 1]);
  std::array<unsigned char, 12> nonce{};
  std::array<unsigned char, 16> tag{};
  std::size_t offset = 0;
  if (encrypt) {
    if (body.size() > MaxBytes - 128 ||
        RAND_bytes(nonce.data(), nonce.size()) != 1)
      throw std::runtime_error("IMU nonce failure");
  } else {
    if (Source(body) != c.source_id)
      throw std::runtime_error("IMU source mismatch");
    offset = 5 + c.source_id.size();
    std::copy_n(body.data() + offset, 12, nonce.data());
    std::copy_n(body.data() + offset + 12, 16, tag.data());
    offset += 28;
  }
  const std::string aad = "openember/imu/v1\n" + c.robot_id + "\n" +
                          endpoint + "\n" + c.source_id;
  std::unique_ptr<EVP_CIPHER_CTX, decltype(&EVP_CIPHER_CTX_free)> ctx(
      EVP_CIPHER_CTX_new(), EVP_CIPHER_CTX_free);
  Bytes out(body.size() + 16);
  int n = 0, total = 0;
  auto require = [](bool b) {
    if (!b)
      throw std::runtime_error("IMU authentication failed");
  };
  require(bool(ctx));
  require(EVP_CipherInit_ex(ctx.get(), EVP_aes_256_gcm(), nullptr, key.data(),
                            nonce.data(), encrypt ? 1 : 0) == 1);
  require(EVP_CipherUpdate(ctx.get(), nullptr, &n,
                           reinterpret_cast<const unsigned char *>(aad.data()),
                           aad.size()) == 1);
  if (body.size() > offset) {
    require(EVP_CipherUpdate(ctx.get(), out.data(), &n, body.data() + offset,
                             body.size() - offset) == 1);
    total = n;
  }
  if (!encrypt)
    require(EVP_CIPHER_CTX_ctrl(ctx.get(), EVP_CTRL_GCM_SET_TAG, tag.size(),
                                tag.data()) == 1);
  require(EVP_CipherFinal_ex(ctx.get(), out.data() + total, &n) == 1);
  out.resize(total + n);
  if (!encrypt)
    return out;
  require(EVP_CIPHER_CTX_ctrl(ctx.get(), EVP_CTRL_GCM_GET_TAG, tag.size(),
                              tag.data()) == 1);
  Bytes frame{'O', 'E', 'I', '1',
              static_cast<unsigned char>(c.source_id.size())};
  frame.insert(frame.end(), c.source_id.begin(), c.source_id.end());
  frame.insert(frame.end(), nonce.begin(), nonce.end());
  frame.insert(frame.end(), tag.begin(), tag.end());
  frame.insert(frame.end(), out.begin(), out.end());
  return frame;
}
template <class T> T Decode(const Bytes &b) {
  T out;
  if (b.size() > MaxBytes ||
      !out.ParseFromArray(b.data(), static_cast<int>(b.size())))
    throw std::runtime_error("invalid IMU protobuf");
  return out;
}
} // namespace openember::sensor::link::envelope
