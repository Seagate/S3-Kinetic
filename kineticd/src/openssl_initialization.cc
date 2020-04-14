#include "openssl_initialization.h"

#include <iostream>
#include "glog/logging.h"
#include "openssl/err.h"
#include "openssl/ssl.h"


using namespace std; //NOLINT

namespace {

pthread_mutex_t *locks = NULL;
int lock_count = 0;

void initialize_locks() {
    lock_count = CRYPTO_num_locks();
    locks=(pthread_mutex_t *)OPENSSL_malloc(CRYPTO_num_locks() *
                                        sizeof(pthread_mutex_t));
    VLOG(1) << "Allocating " << lock_count << " locks for OpenSSL";//NO_SPELL
    //locks = new pthread_mutex_t[lock_count];
    for (int i = 0; i < lock_count; i++) {
        pthread_mutex_init(&(locks[i]), NULL);
    }
}

void free_locks() {
    for (int i = 0; i < lock_count; i++) {
        pthread_mutex_destroy(&(locks[i]));
    }
    OPENSSL_free(locks);
    locks = NULL;
}

void locking_function(int mode, int type, const char *, int) {
    CHECK(type >= 0 && type < lock_count);
    if (mode & CRYPTO_LOCK) {
        pthread_mutex_lock(&(locks[type]));
    } else {
        pthread_mutex_unlock(&(locks[type]));
    }
}

}  // namespace

namespace com {
namespace seagate {
namespace kinetic {

SSL_CTX *initialize_openssl(const std::string &private_key, const std::string &certificate) {
    SSL_library_init();
    initialize_locks();
    CRYPTO_set_locking_callback(locking_function);

    OpenSSL_add_all_algorithms();  /* load & register all cryptos, etc. */
    SSL_load_error_strings();   /* load all error messages */

    const SSL_METHOD *method = SSLv23_server_method();

    CHECK(method != NULL);
    SSL_CTX *context = SSL_CTX_new(method);
    CHECK(context != NULL);
    SSL_CTX_set_options(context, SSL_OP_NO_SSLv2 | SSL_OP_NO_SSLv3 | SSL_OP_NO_TLSv1);
    VLOG(1) << "Loading certificate: " << certificate;
    if (SSL_CTX_use_certificate_file(context, certificate.c_str(), SSL_FILETYPE_PEM) != 1) {
        LOG(ERROR) << "Failed to load certificate file: " << certificate;
        exit(EXIT_FAILURE);
    }
    VLOG(1) << "Loading private key: " << private_key;
    if (SSL_CTX_use_PrivateKey_file(context, private_key.c_str(), SSL_FILETYPE_PEM) != 1) {
        LOG(ERROR) << "Failed to load private key file: " << private_key;
        exit(EXIT_FAILURE);
    }
    VLOG(1) << "Verify private key: ";
    if (!SSL_CTX_check_private_key(context)) {
        LOG(ERROR) << "Private key does not match the public certificate.";
        exit(EXIT_FAILURE);
    }
    return context;
}

void free_openssl(SSL_CTX *ssl_context) {
    // This incantation frees the OpenSSL context object and most other
    // freeable OpenSSL internals.
    SSL_CTX_free(ssl_context);
    ERR_remove_thread_state(NULL);
    EVP_cleanup();
    CRYPTO_cleanup_all_ex_data();
    free_locks();
}

}  // namespace kinetic
}  // namespace seagate
}  // namespace com
