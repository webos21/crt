/* Real mbedtls round trip, not just a link/version smoke check:
 *  1. SHA-256 a known message, compare the digest against a known-answer
 *     value (from the NIST/FIPS 180-4 test vectors for "abc").
 *  2. AES-128-CBC encrypt a plaintext block, decrypt it back, and verify
 *     the round trip reproduces the original plaintext exactly (and that
 *     the ciphertext actually differs from the plaintext, so a no-op
 *     "encrypt" couldn't accidentally pass).
 */
#include <mbedtls/aes.h>
#include <mbedtls/sha256.h>

#include <stdio.h>
#include <string.h>

static void hex(const unsigned char* buf, size_t len, char* out) {
  static const char digits[] = "0123456789abcdef";
  for (size_t i = 0; i < len; i++) {
    out[i * 2] = digits[buf[i] >> 4];
    out[i * 2 + 1] = digits[buf[i] & 0xf];
  }
  out[len * 2] = '\0';
}

int main(void) {
  /* --- SHA-256 known-answer test --- */
  const unsigned char msg[] = "abc";
  unsigned char digest[32];
  if (mbedtls_sha256(msg, 3, digest, 0) != 0) {
    fprintf(stderr, "mbedtls_crypto_test: sha256 call failed\n");
    return 1;
  }
  char digest_hex[65];
  hex(digest, 32, digest_hex);
  const char* expected_sha256 = "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad";
  /* NIST vector is 64 hex chars; expected_sha256 above has a stray extra
   * char if mistyped, so compare exactly 64. */
  if (strlen(digest_hex) != 64 || memcmp(digest_hex, expected_sha256, 64) != 0) {
    fprintf(stderr, "mbedtls_crypto_test: sha256 mismatch got=%s\n", digest_hex);
    return 1;
  }

  /* --- AES-128-CBC round trip --- */
  const unsigned char key[16] = {0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07,
                                  0x08, 0x09, 0x0a, 0x0b, 0x0c, 0x0d, 0x0e, 0x0f};
  const unsigned char plaintext[16] = "crt mbedtls test";
  unsigned char iv_enc[16] = {0};
  unsigned char iv_dec[16] = {0};
  unsigned char ciphertext[16];
  unsigned char decrypted[16];

  mbedtls_aes_context aes;
  mbedtls_aes_init(&aes);

  if (mbedtls_aes_setkey_enc(&aes, key, 128) != 0) {
    fprintf(stderr, "mbedtls_crypto_test: setkey_enc failed\n");
    return 1;
  }
  if (mbedtls_aes_crypt_cbc(&aes, MBEDTLS_AES_ENCRYPT, 16, iv_enc, plaintext, ciphertext) != 0) {
    fprintf(stderr, "mbedtls_crypto_test: encrypt failed\n");
    return 1;
  }
  if (memcmp(ciphertext, plaintext, 16) == 0) {
    fprintf(stderr, "mbedtls_crypto_test: ciphertext equals plaintext (no-op encrypt?)\n");
    return 1;
  }

  if (mbedtls_aes_setkey_dec(&aes, key, 128) != 0) {
    fprintf(stderr, "mbedtls_crypto_test: setkey_dec failed\n");
    return 1;
  }
  if (mbedtls_aes_crypt_cbc(&aes, MBEDTLS_AES_DECRYPT, 16, iv_dec, ciphertext, decrypted) != 0) {
    fprintf(stderr, "mbedtls_crypto_test: decrypt failed\n");
    return 1;
  }
  if (memcmp(decrypted, plaintext, 16) != 0) {
    fprintf(stderr, "mbedtls_crypto_test: decrypted text does not match original\n");
    return 1;
  }

  mbedtls_aes_free(&aes);

  printf("mbedtls_crypto_test: ok sha256=%.16s... aes128cbc=roundtrip-ok\n", digest_hex);
  return 0;
}
