#include "util/base64.h"

#include <openssl/bio.h>
#include <openssl/evp.h>

#include <vector>

namespace util {

std::optional<std::vector<std::uint8_t>> base64_decode(const std::string& encoded) {
    if (encoded.empty()) {
        return std::vector<std::uint8_t>{};
    }

    BIO* b64 = BIO_new(BIO_f_base64());
    if (!b64) {
        return std::nullopt;
    }
    BIO_set_flags(b64, BIO_FLAGS_BASE64_NO_NL);

    BIO* source = BIO_new_mem_buf(encoded.data(), static_cast<int>(encoded.size()));
    if (!source) {
        BIO_free_all(b64);
        return std::nullopt;
    }

    source = BIO_push(b64, source);

    std::vector<std::uint8_t> buffer(encoded.size());
    const int decoded_len =
        BIO_read(source, buffer.data(), static_cast<int>(buffer.size()));
    BIO_free_all(source);

    if (decoded_len < 0) {
        return std::nullopt;
    }

    buffer.resize(static_cast<std::size_t>(decoded_len));
    return buffer;
}

}  // namespace util
