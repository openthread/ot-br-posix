#ifndef SIMPLE_WEB_CRYPTO_HPP
#define SIMPLE_WEB_CRYPTO_HPP

#include <cmath>
#include <iomanip>
#include <istream>
#include <memory>
#include <sstream>
#include <string>
#include <vector>

#include <openssl/buffer.h>
#include <openssl/evp.h>
#include <openssl/md5.h>
#include <openssl/sha.h>

namespace SimpleWeb {
// TODO 2017: remove workaround for MSVS 2012
#if _MSC_VER == 1700                       // MSVS 2012 has no definition for round()
  inline double round(double x) noexcept { // Custom definition of round() for positive numbers
    return floor(x + 0.5);
  }
#endif

  class Crypto {
    const static std::size_t buffer_size = 131072;

  public:
    class Base64 {
    public:
      /// Returns Base64 encoded string from input string.
      static std::string encode(const std::string &input) noexcept {
        std::string base64;

        BIO *bio, *b64;
        auto bptr = BUF_MEM_new();

        b64 = BIO_new(BIO_f_base64());
        BIO_set_flags(b64, BIO_FLAGS_BASE64_NO_NL);
        bio = BIO_new(BIO_s_mem());
        BIO_push(b64, bio);
        BIO_set_mem_buf(b64, bptr, BIO_CLOSE);

        // Write directly to base64-buffer to avoid copy
        auto base64_length = static_cast<std::size_t>(round(4 * ceil(static_cast<double>(input.size()) / 3.0)));
        base64.resize(base64_length);
        bptr->length = 0;
        bptr->max = base64_length + 1;
        bptr->data = &base64[0];

        if(BIO_write(b64, &input[0], static_cast<int>(input.size())) <= 0 || BIO_flush(b64) <= 0)
          base64.clear();

        // To keep &base64[0] through BIO_free_all(b64)
        bptr->length = 0;
        bptr->max = 0;
        bptr->data = nullptr;

        BIO_free_all(b64);

        return base64;
      }

      /// Returns Base64 decoded string from base64 input.
      static std::string decode(const std::string &base64) noexcept {
        std::string ascii((6 * base64.size()) / 8, '\0'); // The size is a up to two bytes too large.

        BIO *b64, *bio;

        b64 = BIO_new(BIO_f_base64());
        BIO_set_flags(b64, BIO_FLAGS_BASE64_NO_NL);
// TODO: Remove in 2022 or later
#if(defined(OPENSSL_VERSION_NUMBER) && OPENSSL_VERSION_NUMBER < 0x1000214fL) || (defined(LIBRESSL_VERSION_NUMBER) && LIBRESSL_VERSION_NUMBER < 0x2080000fL)
        bio = BIO_new_mem_buf(const_cast<char *>(&base64[0]), static_cast<int>(base64.size()));
#else
        bio = BIO_new_mem_buf(&base64[0], static_cast<int>(base64.size()));
#endif
        bio = BIO_push(b64, bio);

        auto decoded_length = BIO_read(bio, &ascii[0], static_cast<int>(ascii.size()));
        if(decoded_length > 0)
          ascii.resize(static_cast<std::size_t>(decoded_length));
        else
          ascii.clear();

        BIO_free_all(b64);

        return ascii;
      }
    };

    /// Returns hex string from bytes in input string.
    static std::string to_hex_string(const std::string &input) noexcept {
      std::stringstream hex_stream;
      hex_stream << std::hex << std::internal << std::setfill('0');
      for(auto &byte : input)
        hex_stream << std::setw(2) << static_cast<int>(static_cast<unsigned char>(byte));
      return hex_stream.str();
    }

    /// Return hash value using specific EVP_MD from input string.
    static std::string message_digest(const std::string &str, const EVP_MD *evp_md, std::size_t digest_length) noexcept {
      std::string md(digest_length, '\0');

      auto ctx = EVP_MD_CTX_create();
      EVP_MD_CTX_init(ctx);
      EVP_DigestInit_ex(ctx, evp_md, nullptr);
      EVP_DigestUpdate(ctx, str.data(), str.size());
      EVP_DigestFinal_ex(ctx, reinterpret_cast<unsigned char *>(&md[0]), nullptr);
      EVP_MD_CTX_destroy(ctx);

      return md;
    }

    /// Return hash value using specific EVP_MD from input stream.
    static std::string stream_digest(std::istream &stream, const EVP_MD *evp_md, std::size_t digest_length) noexcept {
      std::string md(digest_length, '\0');
      std::unique_ptr<char[]> buffer(new char[buffer_size]);
      std::streamsize read_length;

      auto ctx = EVP_MD_CTX_create();
      EVP_MD_CTX_init(ctx);
      EVP_DigestInit_ex(ctx, evp_md, nullptr);
      while((read_length = stream.read(buffer.get(), buffer_size).gcount()) > 0)
        EVP_DigestUpdate(ctx, buffer.get(), static_cast<std::size_t>(read_length));
      EVP_DigestFinal_ex(ctx, reinterpret_cast<unsigned char *>(&md[0]), nullptr);
      EVP_MD_CTX_destroy(ctx);

      return md;
    }

    /// Returns md5 hash value from input string.
    static std::string md5(const std::string &input, std::size_t iterations = 1) noexcept {
      auto evp_md = EVP_md5();
      auto hash = message_digest(input, evp_md, MD5_DIGEST_LENGTH);
      for(std::size_t i = 1; i < iterations; ++i)
        hash = message_digest(hash, evp_md, MD5_DIGEST_LENGTH);
      return hash;
    }

    /// Returns md5 hash value from input stream.
    static std::string md5(std::istream &stream, std::size_t iterations = 1) noexcept {
      auto evp_md = EVP_md5();
      auto hash = stream_digest(stream, evp_md, MD5_DIGEST_LENGTH);
      for(std::size_t i = 1; i < iterations; ++i)
        hash = message_digest(hash, evp_md, MD5_DIGEST_LENGTH);
      return hash;
    }

    /// Returns sha1 hash value from input string.
    static std::string sha1(const std::string &input, std::size_t iterations = 1) noexcept {
      auto evp_md = EVP_sha1();
      auto hash = message_digest(input, evp_md, SHA_DIGEST_LENGTH);
      for(std::size_t i = 1; i < iterations; ++i)
        hash = message_digest(hash, evp_md, SHA_DIGEST_LENGTH);
      return hash;
    }

    /// Returns sha1 hash value from input stream.
    static std::string sha1(std::istream &stream, std::size_t iterations = 1) noexcept {
      auto evp_md = EVP_sha1();
      auto hash = stream_digest(stream, evp_md, SHA_DIGEST_LENGTH);
      for(std::size_t i = 1; i < iterations; ++i)
        hash = message_digest(hash, evp_md, SHA_DIGEST_LENGTH);
      return hash;
    }

    /// Returns sha256 hash value from input string.
    static std::string sha256(const std::string &input, std::size_t iterations = 1) noexcept {
      auto evp_md = EVP_sha256();
      auto hash = message_digest(input, evp_md, SHA256_DIGEST_LENGTH);
      for(std::size_t i = 1; i < iterations; ++i)
        hash = message_digest(hash, evp_md, SHA256_DIGEST_LENGTH);
      return hash;
    }

    /// Returns sha256 hash value from input stream.
    static std::string sha256(std::istream &stream, std::size_t iterations = 1) noexcept {
      auto evp_md = EVP_sha256();
      auto hash = stream_digest(stream, evp_md, SHA256_DIGEST_LENGTH);
      for(std::size_t i = 1; i < iterations; ++i)
        hash = message_digest(hash, evp_md, SHA256_DIGEST_LENGTH);
      return hash;
    }

    /// Returns sha512 hash value from input string.
    static std::string sha512(const std::string &input, std::size_t iterations = 1) noexcept {
      auto evp_md = EVP_sha512();
      auto hash = message_digest(input, evp_md, SHA512_DIGEST_LENGTH);
      for(std::size_t i = 1; i < iterations; ++i)
        hash = message_digest(hash, evp_md, SHA512_DIGEST_LENGTH);
      return hash;
    }

    /// Returns sha512 hash value from input stream.
    static std::string sha512(std::istream &stream, std::size_t iterations = 1) noexcept {
      auto evp_md = EVP_sha512();
      auto hash = stream_digest(stream, evp_md, SHA512_DIGEST_LENGTH);
      for(std::size_t i = 1; i < iterations; ++i)
        hash = message_digest(hash, evp_md, SHA512_DIGEST_LENGTH);
      return hash;
    }

    /**
     * Returns PBKDF2 derived key from the given password.
     *
     * @param password   The password to derive key from.
     * @param salt       The salt to be used in the algorithm.
     * @param iterations Number of iterations to be used in the algorithm.
     * @param key_size   Number of bytes of the returned key.
     *
     * @return The PBKDF2 derived key.
     */
    static std::string pbkdf2(const std::string &password, const std::string &salt, int iterations, int key_size) noexcept {
      std::string key(static_cast<std::size_t>(key_size), '\0');
      PKCS5_PBKDF2_HMAC_SHA1(password.c_str(), password.size(),
                             reinterpret_cast<const unsigned char *>(salt.c_str()), salt.size(), iterations,
                             key_size, reinterpret_cast<unsigned char *>(&key[0]));
      return key;
    }
  };
} // namespace SimpleWeb
#endif /* SIMPLE_WEB_CRYPTO_HPP */
