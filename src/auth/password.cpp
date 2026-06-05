#include "auth/password.h"

#include <algorithm>
#include <array>
#include <chrono>
#include <cstdint>
#include <iomanip>
#include <random>
#include <sstream>
#include <stdexcept>
#include <string_view>
#include <vector>

namespace oj::auth {
namespace {

constexpr std::size_t kSha256BlockSize = 64;
constexpr std::size_t kSha256DigestSize = 32;
constexpr int kDefaultIterations = 120000;

std::uint32_t rotr(std::uint32_t value, int bits) {
  return (value >> bits) | (value << (32 - bits));
}

std::uint32_t load_be32(const std::uint8_t* data) {
  return (static_cast<std::uint32_t>(data[0]) << 24) |
         (static_cast<std::uint32_t>(data[1]) << 16) |
         (static_cast<std::uint32_t>(data[2]) << 8) |
         static_cast<std::uint32_t>(data[3]);
}

void store_be32(std::uint32_t value, std::uint8_t* output) {
  output[0] = static_cast<std::uint8_t>(value >> 24);
  output[1] = static_cast<std::uint8_t>(value >> 16);
  output[2] = static_cast<std::uint8_t>(value >> 8);
  output[3] = static_cast<std::uint8_t>(value);
}

std::array<std::uint8_t, kSha256DigestSize> sha256(std::string_view input) {
  static constexpr std::array<std::uint32_t, 64> k = {
      0x428a2f98, 0x71374491, 0xb5c0fbcf, 0xe9b5dba5, 0x3956c25b,
      0x59f111f1, 0x923f82a4, 0xab1c5ed5, 0xd807aa98, 0x12835b01,
      0x243185be, 0x550c7dc3, 0x72be5d74, 0x80deb1fe, 0x9bdc06a7,
      0xc19bf174, 0xe49b69c1, 0xefbe4786, 0x0fc19dc6, 0x240ca1cc,
      0x2de92c6f, 0x4a7484aa, 0x5cb0a9dc, 0x76f988da, 0x983e5152,
      0xa831c66d, 0xb00327c8, 0xbf597fc7, 0xc6e00bf3, 0xd5a79147,
      0x06ca6351, 0x14292967, 0x27b70a85, 0x2e1b2138, 0x4d2c6dfc,
      0x53380d13, 0x650a7354, 0x766a0abb, 0x81c2c92e, 0x92722c85,
      0xa2bfe8a1, 0xa81a664b, 0xc24b8b70, 0xc76c51a3, 0xd192e819,
      0xd6990624, 0xf40e3585, 0x106aa070, 0x19a4c116, 0x1e376c08,
      0x2748774c, 0x34b0bcb5, 0x391c0cb3, 0x4ed8aa4a, 0x5b9cca4f,
      0x682e6ff3, 0x748f82ee, 0x78a5636f, 0x84c87814, 0x8cc70208,
      0x90befffa, 0xa4506ceb, 0xbef9a3f7, 0xc67178f2};

  std::vector<std::uint8_t> data(input.begin(), input.end());
  const std::uint64_t bit_length =
      static_cast<std::uint64_t>(data.size()) * 8;
  data.push_back(0x80);
  while ((data.size() % kSha256BlockSize) != 56) {
    data.push_back(0);
  }
  for (int i = 7; i >= 0; --i) {
    data.push_back(static_cast<std::uint8_t>(bit_length >> (i * 8)));
  }

  std::array<std::uint32_t, 8> h = {0x6a09e667, 0xbb67ae85, 0x3c6ef372,
                                    0xa54ff53a, 0x510e527f, 0x9b05688c,
                                    0x1f83d9ab, 0x5be0cd19};

  for (std::size_t offset = 0; offset < data.size();
       offset += kSha256BlockSize) {
    std::array<std::uint32_t, 64> w{};
    for (std::size_t i = 0; i < 16; ++i) {
      w[i] = load_be32(data.data() + offset + i * 4);
    }
    for (std::size_t i = 16; i < 64; ++i) {
      const std::uint32_t s0 =
          rotr(w[i - 15], 7) ^ rotr(w[i - 15], 18) ^ (w[i - 15] >> 3);
      const std::uint32_t s1 =
          rotr(w[i - 2], 17) ^ rotr(w[i - 2], 19) ^ (w[i - 2] >> 10);
      w[i] = w[i - 16] + s0 + w[i - 7] + s1;
    }

    std::uint32_t a = h[0];
    std::uint32_t b = h[1];
    std::uint32_t c = h[2];
    std::uint32_t d = h[3];
    std::uint32_t e = h[4];
    std::uint32_t f = h[5];
    std::uint32_t g = h[6];
    std::uint32_t hh = h[7];

    for (std::size_t i = 0; i < 64; ++i) {
      const std::uint32_t s1 = rotr(e, 6) ^ rotr(e, 11) ^ rotr(e, 25);
      const std::uint32_t ch = (e & f) ^ ((~e) & g);
      const std::uint32_t temp1 = hh + s1 + ch + k[i] + w[i];
      const std::uint32_t s0 = rotr(a, 2) ^ rotr(a, 13) ^ rotr(a, 22);
      const std::uint32_t maj = (a & b) ^ (a & c) ^ (b & c);
      const std::uint32_t temp2 = s0 + maj;

      hh = g;
      g = f;
      f = e;
      e = d + temp1;
      d = c;
      c = b;
      b = a;
      a = temp1 + temp2;
    }

    h[0] += a;
    h[1] += b;
    h[2] += c;
    h[3] += d;
    h[4] += e;
    h[5] += f;
    h[6] += g;
    h[7] += hh;
  }

  std::array<std::uint8_t, kSha256DigestSize> digest{};
  for (std::size_t i = 0; i < h.size(); ++i) {
    store_be32(h[i], digest.data() + i * 4);
  }
  return digest;
}

std::array<std::uint8_t, kSha256DigestSize> hmac_sha256(
    std::string_view key, std::string_view message) {
  std::array<std::uint8_t, kSha256BlockSize> key_block{};
  if (key.size() > kSha256BlockSize) {
    const auto digest = sha256(key);
    std::copy(digest.begin(), digest.end(), key_block.begin());
  } else {
    std::copy(key.begin(), key.end(), key_block.begin());
  }

  std::array<std::uint8_t, kSha256BlockSize> inner_key{};
  std::array<std::uint8_t, kSha256BlockSize> outer_key{};
  for (std::size_t i = 0; i < kSha256BlockSize; ++i) {
    inner_key[i] = key_block[i] ^ 0x36;
    outer_key[i] = key_block[i] ^ 0x5c;
  }

  std::string inner;
  inner.reserve(inner_key.size() + message.size());
  inner.append(reinterpret_cast<const char*>(inner_key.data()),
               inner_key.size());
  inner.append(message);
  const auto inner_digest = sha256(inner);

  std::string outer;
  outer.reserve(outer_key.size() + inner_digest.size());
  outer.append(reinterpret_cast<const char*>(outer_key.data()),
               outer_key.size());
  outer.append(reinterpret_cast<const char*>(inner_digest.data()),
               inner_digest.size());
  return sha256(outer);
}

std::array<std::uint8_t, kSha256DigestSize> pbkdf2_sha256(
    std::string_view password, std::string_view salt, int iterations) {
  std::string block_input(salt);
  block_input.push_back('\0');
  block_input.push_back('\0');
  block_input.push_back('\0');
  block_input.push_back('\1');

  auto u = hmac_sha256(password, block_input);
  auto output = u;
  for (int i = 1; i < iterations; ++i) {
    const std::string next(reinterpret_cast<const char*>(u.data()), u.size());
    u = hmac_sha256(password, next);
    for (std::size_t j = 0; j < output.size(); ++j) {
      output[j] ^= u[j];
    }
  }
  return output;
}

std::string to_hex(const std::uint8_t* data, std::size_t size) {
  std::ostringstream output;
  output << std::hex << std::setfill('0');
  for (std::size_t i = 0; i < size; ++i) {
    output << std::setw(2) << static_cast<int>(data[i]);
  }
  return output.str();
}

std::vector<std::string> split_hash(const std::string& value) {
  std::vector<std::string> parts;
  std::size_t start = 0;
  while (true) {
    const auto pos = value.find('$', start);
    if (pos == std::string::npos) {
      parts.push_back(value.substr(start));
      break;
    }
    parts.push_back(value.substr(start, pos - start));
    start = pos + 1;
  }
  return parts;
}

bool constant_time_equal(std::string_view a, std::string_view b) {
  if (a.size() != b.size()) {
    return false;
  }

  unsigned char diff = 0;
  for (std::size_t i = 0; i < a.size(); ++i) {
    diff |= static_cast<unsigned char>(a[i] ^ b[i]);
  }
  return diff == 0;
}

std::string make_salt() {
  std::random_device random;
  const auto now = std::chrono::steady_clock::now().time_since_epoch().count();
  std::ostringstream salt;
  salt << "oj-user-";
  salt << std::hex << random() << random() << now;
  return salt.str();
}

}  // namespace

bool verify_password(const std::string& password,
                     const std::string& password_hash) {
  const auto parts = split_hash(password_hash);
  if (parts.size() != 4 || parts[0] != "pbkdf2_sha256") {
    return false;
  }

  int iterations = 0;
  try {
    iterations = std::stoi(parts[1]);
  } catch (const std::exception&) {
    return false;
  }
  if (iterations <= 0) {
    return false;
  }

  const auto digest = pbkdf2_sha256(password, parts[2], iterations);
  const std::string digest_hex = to_hex(digest.data(), digest.size());
  return constant_time_equal(digest_hex, parts[3]);
}

std::string hash_password(const std::string& password) {
  const std::string salt = make_salt();
  const auto digest = pbkdf2_sha256(password, salt, kDefaultIterations);
  return "pbkdf2_sha256$" + std::to_string(kDefaultIterations) + "$" + salt +
         "$" + to_hex(digest.data(), digest.size());
}

}  // namespace oj::auth
