/* OpenSSL checks for the openssl port (porting/recipes/openssl.json): the digest, AEAD, HMAC and
 * ECDSA primitives WebKit's WebCrypto uses, then a complete TLS 1.3 handshake and an application
 * data exchange between an in-process client and server over a memory BIO pair, with a self-signed
 * EC certificate made here (the curl backend's CurlSSLVerifier and CertificateInfo sit on this API). */
#include <stdio.h>
#include <string.h>

#include <openssl/bio.h>
#include <openssl/err.h>
#include <openssl/evp.h>
#include <openssl/hmac.h>
#include <openssl/rand.h>
#include <openssl/ssl.h>
#include <openssl/x509.h>

static int fail(const char* what) {
  fprintf(stderr, "openssl_tls_test: %s\n", what);
  ERR_print_errors_fp(stderr);
  return 1;
}

static int hex_equals(const unsigned char* bytes, size_t length, const char* hex) {
  char text[2 * 64 + 1];
  size_t i;
  for (i = 0; i < length && i < 64; ++i) sprintf(text + 2 * i, "%02x", bytes[i]);
  return strcmp(text, hex) == 0;
}

static X509* self_signed(EVP_PKEY* key) {
  X509* cert = X509_new();
  X509_NAME* name;
  X509_set_version(cert, 2);
  ASN1_INTEGER_set(X509_get_serialNumber(cert), 1);
  X509_gmtime_adj(X509_getm_notBefore(cert), 0);
  X509_gmtime_adj(X509_getm_notAfter(cert), 3600);
  X509_set_pubkey(cert, key);
  name = X509_get_subject_name(cert);
  X509_NAME_add_entry_by_txt(name, "CN", MBSTRING_ASC, (const unsigned char*)"crt.test", -1, -1, 0);
  X509_set_issuer_name(cert, name);
  if (X509_sign(cert, key, EVP_sha256()) == 0) {
    X509_free(cert);
    return NULL;
  }
  return cert;
}

int main(void) {
  unsigned char digest[32];
  unsigned int digest_length = 0;
  EVP_MD_CTX* md;
  EVP_PKEY* key = NULL;
  EVP_PKEY_CTX* key_context;
  X509* cert;
  SSL_CTX *server_context, *client_context;
  SSL *server, *client;
  BIO *server_bio, *client_bio;
  int round, server_done = 0, client_done = 0;
  char buffer[32];

  /* SHA-256("abc") */
  md = EVP_MD_CTX_new();
  if (!EVP_DigestInit_ex(md, EVP_sha256(), NULL) || !EVP_DigestUpdate(md, "abc", 3) ||
      !EVP_DigestFinal_ex(md, digest, &digest_length) || digest_length != 32 ||
      !hex_equals(digest, 32, "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad")) {
    return fail("SHA-256 of abc is wrong");
  }
  EVP_MD_CTX_free(md);

  /* HMAC-SHA256 (RFC 4231 test case 2) */
  if (!HMAC(EVP_sha256(), "Jefe", 4, (const unsigned char*)"what do ya want for nothing?", 28, digest, &digest_length) ||
      !hex_equals(digest, 32, "5bdcc146bf60754e6a042426089575c75a003f089d2739839dec58b964ec3843")) {
    return fail("HMAC-SHA256 is wrong");
  }

  /* AES-256-GCM round trip and tag check */
  {
    unsigned char k[32], iv[12], plain[] = "web crypto goes through here", cipher[64], decoded[64], tag[16];
    int length = 0, total = 0;
    EVP_CIPHER_CTX* cipher_context = EVP_CIPHER_CTX_new();
    memset(k, 7, sizeof k);
    memset(iv, 9, sizeof iv);
    EVP_EncryptInit_ex(cipher_context, EVP_aes_256_gcm(), NULL, k, iv);
    EVP_EncryptUpdate(cipher_context, cipher, &length, plain, (int)sizeof plain);
    total = length;
    EVP_EncryptFinal_ex(cipher_context, cipher + total, &length);
    EVP_CIPHER_CTX_ctrl(cipher_context, EVP_CTRL_GCM_GET_TAG, 16, tag);
    EVP_CIPHER_CTX_free(cipher_context);
    cipher_context = EVP_CIPHER_CTX_new();
    EVP_DecryptInit_ex(cipher_context, EVP_aes_256_gcm(), NULL, k, iv);
    EVP_DecryptUpdate(cipher_context, decoded, &length, cipher, (int)sizeof plain);
    EVP_CIPHER_CTX_ctrl(cipher_context, EVP_CTRL_GCM_SET_TAG, 16, tag);
    if (EVP_DecryptFinal_ex(cipher_context, decoded + length, &length) <= 0 || memcmp(decoded, plain, sizeof plain) != 0) {
      return fail("AES-256-GCM round trip failed");
    }
    EVP_CIPHER_CTX_free(cipher_context);
  }

  /* EC P-256 key, self-signed certificate, ECDSA verify of the certificate's own signature */
  key_context = EVP_PKEY_CTX_new_id(EVP_PKEY_EC, NULL);
  if (!key_context || EVP_PKEY_keygen_init(key_context) <= 0 ||
      EVP_PKEY_CTX_set_ec_paramgen_curve_nid(key_context, NID_X9_62_prime256v1) <= 0 ||
      EVP_PKEY_keygen(key_context, &key) <= 0) {
    return fail("EC key generation failed");
  }
  EVP_PKEY_CTX_free(key_context);
  cert = self_signed(key);
  if (cert == NULL || X509_verify(cert, key) != 1) {
    return fail("self-signed certificate does not verify");
  }

  /* TLS 1.3 over a BIO pair */
  server_context = SSL_CTX_new(TLS_server_method());
  client_context = SSL_CTX_new(TLS_client_method());
  SSL_CTX_set_min_proto_version(server_context, TLS1_3_VERSION);
  SSL_CTX_set_min_proto_version(client_context, TLS1_3_VERSION);
  if (SSL_CTX_use_certificate(server_context, cert) != 1 || SSL_CTX_use_PrivateKey(server_context, key) != 1) {
    return fail("server credentials rejected");
  }
  server = SSL_new(server_context);
  client = SSL_new(client_context);
  if (!BIO_new_bio_pair(&server_bio, 0, &client_bio, 0)) {
    return fail("BIO pair");
  }
  SSL_set_bio(server, server_bio, server_bio);
  SSL_set_bio(client, client_bio, client_bio);
  SSL_set_accept_state(server);
  SSL_set_connect_state(client);
  for (round = 0; round < 50 && !(server_done && client_done); ++round) {
    if (!client_done) client_done = SSL_do_handshake(client) == 1;
    if (!server_done) server_done = SSL_do_handshake(server) == 1;
  }
  if (!server_done || !client_done || strcmp(SSL_get_version(client), "TLSv1.3") != 0) {
    return fail("TLS 1.3 handshake did not complete");
  }
  if (SSL_write(client, "hello over tls", 14) != 14 || SSL_read(server, buffer, sizeof buffer) != 14 ||
      memcmp(buffer, "hello over tls", 14) != 0) {
    return fail("application data was not delivered");
  }
  printf("openssl_tls_test: ok (%s, %s)\n", OpenSSL_version(OPENSSL_VERSION), SSL_get_cipher_name(client));
  return 0;
}
