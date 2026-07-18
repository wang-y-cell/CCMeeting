#include "util/password_hasher.h"

#include <openssl/sha.h>

#include <iomanip>
#include <sstream>

namespace util {

std::string sha256_hex(const std::string& plain) {
    unsigned char digest[SHA256_DIGEST_LENGTH]{};
    SHA256(reinterpret_cast<const unsigned char*>(plain.data()),
           plain.size(),
           digest);

    std::ostringstream oss;
    oss << std::hex << std::setfill('0');
    for (unsigned char byte : digest) {
        oss << std::setw(2) << static_cast<unsigned int>(byte);
    }
    return oss.str();
}

bool verify_password(const std::string& plain, const std::string& expected_hash) {
    return sha256_hex(plain) == expected_hash;
}

}  // namespace util
