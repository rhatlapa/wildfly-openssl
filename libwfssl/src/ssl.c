
#include "wfssl.h"

#ifdef WIN32
#include <openssl\applink.c>
#endif


#if defined(SSL_OP_NO_TLSv1_1)
#define HAVE_TLSV1_1
#endif

#if defined(SSL_OP_NO_TLSv1_2)
#define HAVE_TLSV1_2
#endif

#ifdef __APPLE__
#define LIBCRYPTO_NAME "libcrypto.dylib"
#define LIBSSL_NAME "libssl.dylib"
#else
#ifdef WIN32
#define LIBCRYPTO_NAME "libeay32.dll"
#define LIBSSL_NAME "ssleay32.dll"
#define FALLBACK_LIBSSL_NAME "libssl32.dll"
#else
#define LIBCRYPTO_NAME "libcrypto.so"
#define LIBSSL_NAME "libssl.so"
#endif
#endif

static int ssl_initialized = 0;
static jclass byteArrayClass, stringClass;

ssl_dynamic_methods ssl_methods;
crypto_dynamic_methods crypto_methods;

/**
 * The cached SSL context class
 */
static jclass    ssl_context_class;
static jmethodID sni_java_callback;

/* indexes for customer SSL data */
static int SSL_app_data1_idx = -1; /* connection metadata */
static int SSL_app_data2_idx = -1; /* context metadata */
static int SSL_app_data3_idx = -1; /* handshake count */
static int SSL_CTX_app_data1_idx = -1; /* context metadata */

void init_app_data_idx(void)
{
    if (SSL_app_data1_idx > -1) {
        return;
    }

    if(ssl_methods.SSL_get_ex_new_index != NULL) {
        SSL_app_data1_idx = ssl_methods.SSL_get_ex_new_index(0, "First Application Data for SSL", NULL, NULL, NULL);
        SSL_app_data2_idx = ssl_methods.SSL_get_ex_new_index(0, "Second Application Data for SSL", NULL, NULL, NULL);
        SSL_app_data3_idx = ssl_methods.SSL_get_ex_new_index(0, "Third Application Data for SSL", NULL, NULL, NULL);
        SSL_CTX_app_data1_idx = ssl_methods.SSL_CTX_get_ex_new_index(0, "First Application Data for SSL_CTX", NULL, NULL, NULL);
    } else {
        SSL_app_data1_idx = ssl_methods.CRYPTO_get_ex_new_index(CRYPTO_EX_INDEX_SSL, 0, "First Application Data for SSL", NULL, NULL, NULL);
        SSL_app_data2_idx = ssl_methods.CRYPTO_get_ex_new_index(CRYPTO_EX_INDEX_SSL, 0, "Second Application Data for SSL", NULL, NULL, NULL);
        SSL_app_data3_idx = ssl_methods.CRYPTO_get_ex_new_index(CRYPTO_EX_INDEX_SSL, 0, "Third Application Data for SSL", NULL, NULL, NULL);
        SSL_CTX_app_data1_idx = ssl_methods.CRYPTO_get_ex_new_index(CRYPTO_EX_INDEX_SSL_CTX, 0, "First Application Data for SSL_CTX", NULL, NULL, NULL);
    }
}
/*the the SSL context structure associated with the context*/
tcn_ssl_conn_t *SSL_get_app_data1(const SSL *ssl)
{
    if(ssl_methods.SSL_get_ex_data != NULL) {
        return (tcn_ssl_conn_t *)ssl_methods.SSL_get_ex_data(ssl, SSL_app_data1_idx);
    } else {
        return (tcn_ssl_conn_t *)ssl_methods.CRYPTO_get_ex_data((CRYPTO_EX_DATA*)ssl, SSL_app_data1_idx);
    }
}

void SSL_set_app_data1(SSL *ssl, tcn_ssl_conn_t *arg)
{
    if(ssl_methods.SSL_set_ex_data != NULL) {
        ssl_methods.SSL_set_ex_data(ssl, SSL_app_data1_idx, (char *)arg);
    } else {
        ssl_methods.CRYPTO_set_ex_data((CRYPTO_EX_DATA*)ssl, SSL_app_data1_idx, (char *)arg);
    }
}
/*the the SSL context structure associated with the context*/
tcn_ssl_ctxt_t *SSL_get_app_data2(const SSL *ssl)
{
    if(ssl_methods.SSL_get_ex_data != NULL) {
        return (tcn_ssl_ctxt_t *)ssl_methods.SSL_get_ex_data(ssl, SSL_app_data2_idx);
    } else {
        return (tcn_ssl_ctxt_t *)ssl_methods.CRYPTO_get_ex_data((CRYPTO_EX_DATA*)ssl, SSL_app_data2_idx);
    }
}

void SSL_set_app_data2(SSL *ssl, tcn_ssl_ctxt_t *arg)
{
    if(ssl_methods.SSL_set_ex_data != NULL) {
        ssl_methods.SSL_set_ex_data(ssl, SSL_app_data2_idx, (char *)arg);
    } else {
        ssl_methods.CRYPTO_set_ex_data((CRYPTO_EX_DATA*)ssl, SSL_app_data2_idx, (char *)arg);
    }
}

void *SSL_get_app_data3(const SSL *ssl)
{
    if(ssl_methods.SSL_get_ex_data != NULL) {
        return (void *)ssl_methods.SSL_get_ex_data(ssl, SSL_app_data3_idx);
    } else {
        return (void *)ssl_methods.CRYPTO_get_ex_data((CRYPTO_EX_DATA*)ssl, SSL_app_data3_idx);
    }
}

void SSL_set_app_data3(SSL *ssl, void *arg)
{
    if(ssl_methods.SSL_set_ex_data != NULL) {
        ssl_methods.SSL_set_ex_data(ssl, SSL_app_data3_idx, (char *)arg);
    } else {
        ssl_methods.CRYPTO_set_ex_data((CRYPTO_EX_DATA*)ssl, SSL_app_data3_idx, (char *)arg);
    }
}


tcn_ssl_ctxt_t *SSL_CTX_get_app_data1(const SSL_CTX *ssl)
{
    if(ssl_methods.SSL_CTX_get_ex_data != NULL) {
        return (tcn_ssl_ctxt_t *)ssl_methods.SSL_CTX_get_ex_data(ssl, SSL_CTX_app_data1_idx);
    } else {
        return (tcn_ssl_ctxt_t *)ssl_methods.CRYPTO_get_ex_data((CRYPTO_EX_DATA*)ssl, SSL_CTX_app_data1_idx);
    }
}

void SSL_CTX_set_app_data1(SSL_CTX *ssl, void *arg)
{
    if(ssl_methods.SSL_CTX_set_ex_data != NULL) {
        ssl_methods.SSL_CTX_set_ex_data(ssl, SSL_CTX_app_data1_idx, (char *)arg);
    } else {
        ssl_methods.CRYPTO_set_ex_data((CRYPTO_EX_DATA*)ssl, SSL_CTX_app_data1_idx, (char *)arg);
    }
}

void SSL_BIO_close(BIO *bi)
{
    if (bi == NULL)
        return;
    else
        crypto_methods.BIO_free(bi);
}

/* Callback used when OpenSSL receives a client hello with a Server Name
 * Indication extension.
 */
int ssl_callback_ServerNameIndication(SSL *ssl, int *al, tcn_ssl_ctxt_t *c)
{
    /* TODO: Is it better to cache the JNIEnv* during the call to handshake? */

    /* Get the JNI environment for this callback */
    JavaVM *javavm = tcn_get_java_vm();
    JNIEnv *env;
    const char *servername;
    jstring hostname;
    jlong original_ssl_context, new_ssl_context;
    (*javavm)->AttachCurrentThread(javavm, (void **)&env, NULL);

    // Get the host name presented by the client
    servername = ssl_methods.SSL_get_servername(ssl, TLSEXT_NAMETYPE_host_name);

    // Convert parameters ready for the method call
    hostname = (*env)->NewStringUTF(env, servername);
    original_ssl_context = P2J(c->ctx);

    // Make the call
    new_ssl_context = (*env)->CallStaticLongMethod(env,
                                                            ssl_context_class,
                                                            sni_java_callback,
                                                            original_ssl_context,
                                                            hostname);

    if (original_ssl_context != new_ssl_context) {
        ssl_methods.SSL_set_SSL_CTX(ssl, J2P(new_ssl_context, SSL_CTX *));
    }

    return SSL_TLSEXT_ERR_OK;
}

#ifdef WIN32

#define REQUIRE_SYMBOL(handle, symb, target) target = (void*)GetProcAddress(handle, #symb); if(target == 0) { throwIllegalStateException(e, "Could not load required symbol from" #handle " " #symb); return 1;}
#define REQUIRE_SSL_SYMBOL(symb) ssl_methods.symb = (void*)GetProcAddress(ssl, #symb); if(ssl_methods.symb == 0) { throwIllegalStateException(e, "Could not load required symbol from libssl: " #symb); return 1;}
#define GET_SSL_SYMBOL(symb) ssl_methods.symb = (void*)GetProcAddress(ssl, #symb);
#define REQUIRE_CRYPTO_SYMBOL(symb) crypto_methods.symb = (void*)GetProcAddress(crypto, #symb); if(crypto_methods.symb == 0) { throwIllegalStateException(e, "Could not load required symbol from libcrypto: " #symb); return 1;}
#define GET_CRYPTO_SYMBOL(symb)  crypto_methods.symb = (void*)GetProcAddress(crypto, #symb);

#else

#define REQUIRE_SSL_SYMBOL_ALIAS(symb, alias) ssl_methods.alias = dlsym(ssl, #symb); if(ssl_methods.alias == 0) {throwIllegalStateException(e, "Could not load required symbol from libssl: " #symb); return 1;}
#define REQUIRE_SSL_SYMBOL(symb) REQUIRE_SSL_SYMBOL_ALIAS(symb, symb);
#define GET_SSL_SYMBOL(symb) ssl_methods.symb = dlsym(ssl, #symb);
#define REQUIRE_CRYPTO_SYMBOL_ALIAS(symb, alias) crypto_methods.alias = dlsym(crypto, #symb); if(crypto_methods.alias == 0) {throwIllegalStateException(e, "Could not load required symbol from libcrypto: " #symb); return 1;}
#define REQUIRE_CRYPTO_SYMBOL(symb) REQUIRE_CRYPTO_SYMBOL_ALIAS(symb, symb);
#define GET_CRYPTO_SYMBOL(symb) crypto_methods.symb = dlsym(crypto, #symb);

#endif

int load_openssl_dynamic_methods(JNIEnv *e, const char * path) {

#ifdef WIN32
	HMODULE crypto, ssl;

    if(path == NULL) {
	    crypto = LoadLibrary(LIBCRYPTO_NAME);
	} else {
        int pathLen = strlen(path);
        int size = (strlen(LIBCRYPTO_NAME) + pathLen + 1);
        char * full = malloc(sizeof(char) * size);
        strncpy(full, path, size);
        strncpy(full + pathLen, LIBCRYPTO_NAME, size - pathLen);
        crypto = LoadLibrary(full);
        free(full);
	}
    if( crypto == NULL ) {
        throwIllegalStateException(e, "Could not load libeay32.dll");
        return 1;
    }
    REQUIRE_SYMBOL(crypto, SSLeay, ssl_methods.SSLeay);
    if(path == NULL) {
	    ssl = LoadLibrary(LIBSSL_NAME);
	} else {
        int pathLen = strlen(path);
        int size = (strlen(LIBSSL_NAME) + pathLen + 1);
        char * full = malloc(sizeof(char) * size);
        strncpy(full, path, size);
        strncpy(full + pathLen, LIBSSL_NAME, size - pathLen);
        ssl = LoadLibrary(full);
        free(full);
    }
    if(ssl == NULL) {
        if(path == NULL) {
            ssl = LoadLibrary(FALLBACK_LIBSSL_NAME);
        } else {
            int pathLen = strlen(path);
            int size = (strlen(FALLBACK_LIBSSL_NAME) + pathLen + 1);
            char * full = malloc(sizeof(char) * size);
            strncpy(full, path, size);
            strncpy(full + pathLen, LIBSSL_NAME, size - pathLen);
            ssl = LoadLibrary(full);
            free(full);
        }
    }

    if( ssl == NULL ) {
        throwIllegalStateException(e, "Could not load ssleay32.dll or libssl32.dll");
        return 1;
    }
#else
    void * ssl;
    if(path == NULL) {
        ssl = dlopen(LIBSSL_NAME, RTLD_LAZY);
    } else {
        int pathLen = strlen(path);
        int size = (strlen(LIBSSL_NAME) + pathLen + 1);
        char * full = malloc(sizeof(char) * size);
        strncpy(full, path, size);
        strncpy(full + pathLen, LIBSSL_NAME, size - pathLen);
        ssl = dlopen(full, RTLD_LAZY);
        free(full);
    }

    void * crypto = dlopen(LIBCRYPTO_NAME, RTLD_LAZY);
    if(path == NULL) {
        crypto = dlopen(LIBCRYPTO_NAME, RTLD_LAZY);
    } else {
        int pathLen = strlen(path);
        int size = (strlen(LIBCRYPTO_NAME) + pathLen + 1);
        char * full = malloc(sizeof(char) * size);
        strncpy(full, path, size);
        strncpy(full + pathLen, LIBCRYPTO_NAME, size - pathLen);
        crypto = dlopen(full, RTLD_LAZY);
        free(full);
    }
    if( ssl == NULL ) {
        throwIllegalStateException(e, "Could not load libssl");
        return 1;
    }
    if( crypto == NULL ) {
        throwIllegalStateException(e, "Could not load libcrypto");
        return 1;
    }
    GET_SSL_SYMBOL(SSLeay);
    if(ssl_methods.SSLeay == NULL) {
        REQUIRE_SSL_SYMBOL_ALIAS(OpenSSL_version, SSLeay);
    }
#endif

    GET_SSL_SYMBOL(SSL_CTX_get_ex_new_index);
    if(ssl_methods.SSL_CTX_get_ex_new_index != NULL) {
        REQUIRE_SSL_SYMBOL(SSL_CTX_set_ex_data);
        REQUIRE_SSL_SYMBOL(SSL_get_ex_data);
        REQUIRE_SSL_SYMBOL(SSL_get_ex_data_X509_STORE_CTX_idx);
        REQUIRE_SSL_SYMBOL(SSL_get_ex_new_index);
        REQUIRE_SSL_SYMBOL(SSL_set_ex_data);
        REQUIRE_SSL_SYMBOL(SSL_CTX_get_ex_data);
    } else {
        REQUIRE_SSL_SYMBOL(CRYPTO_get_ex_new_index)
        REQUIRE_SSL_SYMBOL(CRYPTO_get_ex_data)
        REQUIRE_SSL_SYMBOL(CRYPTO_set_ex_data)
    }

    REQUIRE_SSL_SYMBOL(SSL_CIPHER_get_name);
    REQUIRE_SSL_SYMBOL(SSL_CTX_callback_ctrl);
    REQUIRE_SSL_SYMBOL(SSL_CTX_check_private_key);
    REQUIRE_SSL_SYMBOL(SSL_CTX_ctrl);
    REQUIRE_SSL_SYMBOL(SSL_CTX_free);
    REQUIRE_SSL_SYMBOL(SSL_CTX_get_cert_store);
    REQUIRE_SSL_SYMBOL(SSL_CTX_get_client_CA_list);
    REQUIRE_SSL_SYMBOL(SSL_CTX_get_timeout);
    REQUIRE_SSL_SYMBOL(SSL_CTX_load_verify_locations);
    REQUIRE_SSL_SYMBOL(SSL_CTX_new);
    REQUIRE_SSL_SYMBOL(SSL_CTX_sess_set_new_cb);
    REQUIRE_SSL_SYMBOL(SSL_CTX_ctrl);
    REQUIRE_SSL_SYMBOL(SSL_CIPHER_get_name);
    REQUIRE_SSL_SYMBOL(SSL_CTX_callback_ctrl);
    REQUIRE_SSL_SYMBOL(SSL_CTX_ctrl);
    REQUIRE_SSL_SYMBOL(SSL_CTX_sess_set_remove_cb);
    REQUIRE_SSL_SYMBOL(SSL_get_error);
    GET_SSL_SYMBOL(SSL_set_alpn_protos);
    GET_SSL_SYMBOL(SSL_CTX_set_alpn_select_cb);
    GET_SSL_SYMBOL(SSL_get0_alpn_selected);
    REQUIRE_SSL_SYMBOL(SSL_CTX_set_cert_verify_callback);
    REQUIRE_SSL_SYMBOL(SSL_CTX_set_cipher_list);
    REQUIRE_SSL_SYMBOL(SSL_CTX_set_default_verify_paths);
    REQUIRE_SSL_SYMBOL(SSL_CTX_set_session_id_context);
    REQUIRE_SSL_SYMBOL(SSL_CTX_set_timeout);
    REQUIRE_SSL_SYMBOL(SSL_CTX_set_verify);
    REQUIRE_SSL_SYMBOL(SSL_CTX_use_PrivateKey);
    REQUIRE_SSL_SYMBOL(SSL_CTX_use_certificate);
    REQUIRE_SSL_SYMBOL(SSL_SESSION_free);
    REQUIRE_SSL_SYMBOL(SSL_SESSION_get_id);
    REQUIRE_SSL_SYMBOL(SSL_SESSION_get_time);
    REQUIRE_SSL_SYMBOL(SSL_add_file_cert_subjects_to_stack);
    REQUIRE_SSL_SYMBOL(SSL_ctrl);
    REQUIRE_SSL_SYMBOL(SSL_do_handshake);
    REQUIRE_SSL_SYMBOL(SSL_free);
    REQUIRE_SSL_SYMBOL(SSL_get_ciphers);
    REQUIRE_SSL_SYMBOL(SSL_get_current_cipher);
    REQUIRE_SSL_SYMBOL(SSL_get_peer_cert_chain);
    REQUIRE_SSL_SYMBOL(SSL_get_peer_certificate);
    REQUIRE_SSL_SYMBOL(SSL_get_privatekey);
    REQUIRE_SSL_SYMBOL(SSL_get_servername);
    REQUIRE_SSL_SYMBOL(SSL_get_session);
    REQUIRE_SSL_SYMBOL(SSL_get1_session);
    REQUIRE_SSL_SYMBOL(SSL_set_session);
    REQUIRE_SSL_SYMBOL(SSL_get_shutdown);
    REQUIRE_SSL_SYMBOL(SSL_get_version);

    GET_SSL_SYMBOL(SSL_library_init);
    if(ssl_methods.SSL_library_init == NULL) {
        REQUIRE_SSL_SYMBOL(OPENSSL_init_ssl);
    } else {
        REQUIRE_SSL_SYMBOL(SSL_load_error_strings);
    }

    REQUIRE_SSL_SYMBOL(SSL_load_client_CA_file);
    REQUIRE_SSL_SYMBOL(SSL_new);
    REQUIRE_SSL_SYMBOL(SSL_pending);
    REQUIRE_SSL_SYMBOL(SSL_set_read_ahead);
    REQUIRE_SSL_SYMBOL(SSL_read);
    REQUIRE_SSL_SYMBOL(SSL_renegotiate);
    REQUIRE_SSL_SYMBOL(SSL_renegotiate_pending);
    REQUIRE_SSL_SYMBOL(SSL_set_SSL_CTX);
    REQUIRE_SSL_SYMBOL(SSL_set_accept_state);
    REQUIRE_SSL_SYMBOL(SSL_set_bio);
    REQUIRE_SSL_SYMBOL(SSL_set_cipher_list);
    REQUIRE_SSL_SYMBOL(SSL_set_connect_state);
    REQUIRE_SSL_SYMBOL(SSL_set_verify);
    REQUIRE_SSL_SYMBOL(SSL_set_verify_result);
    REQUIRE_SSL_SYMBOL(SSL_shutdown);
    REQUIRE_SSL_SYMBOL(SSL_set_info_callback);
    REQUIRE_SSL_SYMBOL(SSL_write);
    GET_SSL_SYMBOL(TLSv1_1_client_method);
    GET_SSL_SYMBOL(TLSv1_1_method);
    GET_SSL_SYMBOL(TLSv1_1_server_method);
    GET_SSL_SYMBOL(TLSv1_2_client_method);
    GET_SSL_SYMBOL(TLSv1_2_method);
    GET_SSL_SYMBOL(TLSv1_2_server_method);
    GET_SSL_SYMBOL(TLSv1_client_method);
    GET_SSL_SYMBOL(TLSv1_method);
    GET_SSL_SYMBOL(TLSv1_server_method);
    GET_SSL_SYMBOL(TLS_client_method);
    GET_SSL_SYMBOL(TLS_server_method);
    GET_SSL_SYMBOL(TLS_method);

    REQUIRE_CRYPTO_SYMBOL(ASN1_INTEGER_cmp);
    REQUIRE_CRYPTO_SYMBOL(BIO_ctrl);
    REQUIRE_CRYPTO_SYMBOL(BIO_ctrl_pending);
    REQUIRE_CRYPTO_SYMBOL(BIO_free);
    REQUIRE_CRYPTO_SYMBOL(BIO_new);
    REQUIRE_CRYPTO_SYMBOL(BIO_new_bio_pair);
    REQUIRE_CRYPTO_SYMBOL(BIO_printf);
    REQUIRE_CRYPTO_SYMBOL(BIO_read);
    REQUIRE_CRYPTO_SYMBOL(BIO_s_file);
    REQUIRE_CRYPTO_SYMBOL(BIO_s_mem);
    REQUIRE_CRYPTO_SYMBOL(BIO_write);
    REQUIRE_CRYPTO_SYMBOL(CRYPTO_free);
    GET_CRYPTO_SYMBOL(CRYPTO_num_locks);
    GET_CRYPTO_SYMBOL(CRYPTO_set_dynlock_create_callback);
    GET_CRYPTO_SYMBOL(CRYPTO_set_dynlock_destroy_callback);
    GET_CRYPTO_SYMBOL(CRYPTO_set_dynlock_lock_callback);
    GET_CRYPTO_SYMBOL(CRYPTO_set_id_callback);
    GET_CRYPTO_SYMBOL(CRYPTO_set_locking_callback);
    REQUIRE_CRYPTO_SYMBOL(CRYPTO_set_mem_functions);
    REQUIRE_CRYPTO_SYMBOL(ERR_error_string);
    REQUIRE_CRYPTO_SYMBOL(ERR_get_error);
    GET_CRYPTO_SYMBOL(ERR_load_crypto_strings);
    GET_CRYPTO_SYMBOL(OPENSSL_add_all_algorithms_noconf);
    REQUIRE_CRYPTO_SYMBOL(EVP_Digest);
    REQUIRE_CRYPTO_SYMBOL(EVP_PKEY_bits);
    REQUIRE_CRYPTO_SYMBOL(EVP_PKEY_free);
    REQUIRE_CRYPTO_SYMBOL(EVP_PKEY_type);
    REQUIRE_CRYPTO_SYMBOL(EVP_sha1);
    REQUIRE_CRYPTO_SYMBOL(OPENSSL_load_builtin_modules);
    REQUIRE_CRYPTO_SYMBOL(PEM_read_bio_PrivateKey);
    REQUIRE_CRYPTO_SYMBOL(X509_CRL_verify);
    REQUIRE_CRYPTO_SYMBOL(X509_LOOKUP_ctrl);
    REQUIRE_CRYPTO_SYMBOL(X509_LOOKUP_file);
    REQUIRE_CRYPTO_SYMBOL(X509_LOOKUP_hash_dir);
    //REQUIRE_CRYPTO_SYMBOL(X509_OBJECT_free_contents);
    REQUIRE_CRYPTO_SYMBOL(X509_STORE_CTX_cleanup);
    REQUIRE_CRYPTO_SYMBOL(X509_STORE_CTX_get_current_cert);
    REQUIRE_CRYPTO_SYMBOL(X509_STORE_CTX_get_error);
    REQUIRE_CRYPTO_SYMBOL(X509_STORE_CTX_get_error_depth);
    REQUIRE_CRYPTO_SYMBOL(X509_STORE_CTX_get_ex_data);
    REQUIRE_CRYPTO_SYMBOL(X509_STORE_CTX_init);
    REQUIRE_CRYPTO_SYMBOL(X509_STORE_CTX_set_error);
    REQUIRE_CRYPTO_SYMBOL(X509_STORE_add_lookup);
    REQUIRE_CRYPTO_SYMBOL(X509_STORE_free);
    //REQUIRE_CRYPTO_SYMBOL(X509_STORE_get_by_subject);
    REQUIRE_CRYPTO_SYMBOL(X509_STORE_new);
    REQUIRE_CRYPTO_SYMBOL(X509_STORE_set_flags);
    REQUIRE_CRYPTO_SYMBOL(X509_cmp_current_time);
    REQUIRE_CRYPTO_SYMBOL(X509_free);
    REQUIRE_CRYPTO_SYMBOL(X509_get_issuer_name);
    REQUIRE_CRYPTO_SYMBOL(X509_get_pubkey);
    REQUIRE_CRYPTO_SYMBOL(X509_get_serialNumber);
    REQUIRE_CRYPTO_SYMBOL(X509_get_subject_name);
    REQUIRE_CRYPTO_SYMBOL(d2i_X509);
    REQUIRE_CRYPTO_SYMBOL(i2d_X509);
    GET_CRYPTO_SYMBOL(sk_num);
    if(crypto_methods.sk_num == NULL) {
        REQUIRE_CRYPTO_SYMBOL_ALIAS(OPENSSL_sk_num, sk_num)
    }
    GET_CRYPTO_SYMBOL(sk_value);
    if(crypto_methods.sk_value == NULL) {
        REQUIRE_CRYPTO_SYMBOL_ALIAS(OPENSSL_sk_value, sk_value)
    }

    REQUIRE_CRYPTO_SYMBOL(X509_free);
    GET_CRYPTO_SYMBOL(ENGINE_load_builtin_engines);

    return 0;
}


WF_OPENSSL(jint, initialize) (JNIEnv *e, jobject o, jstring openSSLPath) {
    const char * path = NULL;
    TCN_ALLOC_CSTRING(openSSLPath);
    if(openSSLPath != NULL) {
        path = J2S(openSSLPath);
    }

    if(load_openssl_dynamic_methods(e, path) != 0) {
        TCN_FREE_CSTRING(openSSLPath);
        return 0;
    }
    TCN_FREE_CSTRING(openSSLPath);

    long version = ssl_methods.SSLeay();
    jclass clazz;
    jclass sClazz;

    /* Check if already initialized */
    if (ssl_initialized++) {
        return 0;
    }
    //require 1.0.1 or higher
    if (version < 0x010001000L) {
        ssl_initialized = 0;
        return throwIllegalStateException(e, "Invalid OpenSSL Version");
    }
    /* We must register the library in full, to ensure our configuration
     * code can successfully test the SSL environment.
     */
    crypto_methods.CRYPTO_set_mem_functions(malloc, realloc, free);
    if(ssl_methods.SSL_library_init != NULL) {
        /*OpenSSL 1.0.x */
        ssl_methods.SSL_load_error_strings();
        crypto_methods.ERR_load_crypto_strings();
        ssl_methods.SSL_library_init();
        crypto_methods.OPENSSL_add_all_algorithms_noconf();
    } else {
        ssl_methods.OPENSSL_init_ssl(0, NULL);
    }
    init_app_data_idx();
    if(crypto_methods.ENGINE_load_builtin_engines != NULL) {
        crypto_methods.ENGINE_load_builtin_engines();
    }
    crypto_methods.OPENSSL_load_builtin_modules();

    ssl_thread_setup();

    //TODO: engine support?

    /* Cache the byte[].class for performance reasons */
    clazz = (*e)->FindClass(e, "[B");
    byteArrayClass = (jclass) (*e)->NewGlobalRef(e, clazz);

    /* Cache the String.class for performance reasons */
    sClazz = (*e)->FindClass(e, "java/lang/String");
    stringClass = (jclass) (*e)->NewGlobalRef(e, sClazz);

    alpn_init(e);
    session_init(e);

    return (jint)0;
}

/* Initialize server context */
WF_OPENSSL(jlong, makeSSLContext)(JNIEnv *e, jobject o,
                                            jint protocol, jint mode)
{
    tcn_ssl_ctxt_t *c = NULL;
    SSL_CTX *ctx = NULL;
    jclass clazz;

    if (protocol == SSL_PROTOCOL_NONE) {
        throwIllegalStateException(e, "No SSL protocols requested");
        goto init_failed;
    }

    if (protocol == SSL_PROTOCOL_TLSV1_2) {
        if(ssl_methods.TLSv1_2_server_method != NULL) {
            if (mode == SSL_MODE_CLIENT)
                ctx = ssl_methods.SSL_CTX_new(ssl_methods.TLSv1_2_client_method());
            else if (mode == SSL_MODE_SERVER)
                ctx = ssl_methods.SSL_CTX_new(ssl_methods.TLSv1_2_server_method());
            else
                ctx = ssl_methods.SSL_CTX_new(ssl_methods.TLSv1_2_method());
        } else {
            throwIllegalStateException(e, "TLSV1_2 not supported");
            goto init_failed;
        }
    } else if (protocol == SSL_PROTOCOL_TLSV1_1) {
        if(ssl_methods.TLSv1_1_server_method != NULL) {
            if (mode == SSL_MODE_CLIENT)
                ctx = ssl_methods.SSL_CTX_new(ssl_methods.TLSv1_1_client_method());
            else if (mode == SSL_MODE_SERVER)
                ctx = ssl_methods.SSL_CTX_new(ssl_methods.TLSv1_1_server_method());
            else
                ctx = ssl_methods.SSL_CTX_new(ssl_methods.TLSv1_1_method());
        } else {
            throwIllegalStateException(e, "TLSV1_1 not supported");
            goto init_failed;
        }
    } else if (protocol == SSL_PROTOCOL_TLSV1) {
        if (mode == SSL_MODE_CLIENT)
            ctx = ssl_methods.SSL_CTX_new(ssl_methods.TLSv1_client_method());
        else if (mode == SSL_MODE_SERVER)
            ctx = ssl_methods.SSL_CTX_new(ssl_methods.TLSv1_server_method());
        else
            ctx = ssl_methods.SSL_CTX_new(ssl_methods.TLSv1_method());
    } else if (protocol == SSL_PROTOCOL_SSLV3) {
        throwIllegalStateException(e, "SSLV3 not supported");
        goto init_failed;
    } else if (protocol == SSL_PROTOCOL_SSLV2) {
        /* requested but not supported */
        throwIllegalStateException(e, "SSLV2 not supported");
        goto init_failed;
    } else if (protocol & SSL_PROTOCOL_TLSV1_2 && ssl_methods.TLSv1_2_server_method == NULL) {
        /* requested but not supported */
        throwIllegalStateException(e, "TLSV1_2 not supported");
        goto init_failed;
    } else if (protocol & SSL_PROTOCOL_TLSV1_1 && ssl_methods.TLSv1_1_server_method == NULL) {
        /* requested but not supported */
        throwIllegalStateException(e, "TLSV1_1 not supported");
        goto init_failed;
    } else if (ssl_methods.TLS_server_method == NULL) {
        throwIllegalStateException(e, "TLS not supported"); /*this should never happen, as all supported openssl versions should have this*/
    } else {
        if (mode == SSL_MODE_CLIENT)
            ctx = ssl_methods.SSL_CTX_new(ssl_methods.TLS_client_method());
        else if (mode == SSL_MODE_SERVER)
            ctx = ssl_methods.SSL_CTX_new(ssl_methods.TLS_server_method());
        else
            ctx = ssl_methods.SSL_CTX_new(ssl_methods.TLS_method());
    }
    if (!ctx) {
        char err[256];
        crypto_methods.ERR_error_string(crypto_methods.ERR_get_error(), err);
        throwIllegalStateException(e, err);
        goto init_failed;
    }
    if ((c = malloc(sizeof(tcn_ssl_ctxt_t))) == NULL) {
        throwIllegalStateException(e, "malloc failed");
        goto init_failed;
    }
    memset(c, 0, sizeof(*c));

    c->protocol = protocol;
    c->mode     = mode;
    c->ctx      = ctx;
    ssl_methods.SSL_CTX_ctrl((c->ctx),SSL_CTRL_OPTIONS,(SSL_OP_ALL),NULL);
    /* always disable SSLv2, as per RFC 6176 */
    ssl_methods.SSL_CTX_ctrl((c->ctx),SSL_CTRL_OPTIONS,(SSL_OP_NO_SSLv2),NULL);
    if (!(protocol & SSL_PROTOCOL_SSLV3))
        ssl_methods.SSL_CTX_ctrl((c->ctx),SSL_CTRL_OPTIONS,(SSL_OP_NO_SSLv3),NULL);
    if (!(protocol & SSL_PROTOCOL_TLSV1))
        ssl_methods.SSL_CTX_ctrl((c->ctx),SSL_CTRL_OPTIONS,(SSL_OP_NO_TLSv1),NULL);
    if(ssl_methods.TLSv1_1_server_method != NULL) { //we use the presence of the method to test if it is supported
        if (!(protocol & SSL_PROTOCOL_TLSV1_1))
            ssl_methods.SSL_CTX_ctrl((c->ctx),SSL_CTRL_OPTIONS,(SSL_OP_NO_TLSv1_1),NULL);
    }
    if(ssl_methods.TLSv1_2_server_method != NULL) {
        if (!(protocol & SSL_PROTOCOL_TLSV1_2))
            ssl_methods.SSL_CTX_ctrl((c->ctx),SSL_CTRL_OPTIONS,(SSL_OP_NO_TLSv1_2),NULL);
    }
    /*
     * Configure additional context ingredients
     */
    ssl_methods.SSL_CTX_ctrl((c->ctx),SSL_CTRL_OPTIONS,(SSL_OP_SINGLE_DH_USE),NULL);
//TODO: what do we do with these defines?
#ifdef HAVE_ECC
    ssl_methods.SSL_CTX_ctrl((c->ctx),SSL_CTRL_OPTIONS,(SSL_OP_SINGLE_ECDH_USE),NULL);
#endif
#ifdef SSL_OP_NO_COMPRESSION
    /* Disable SSL compression to be safe */
    ssl_methods.SSL_CTX_ctrl((c->ctx),SSL_CTRL_OPTIONS,(SSL_OP_NO_COMPRESSION),NULL);
#endif


    /** To get back the tomcat wrapper from CTX */
    SSL_CTX_set_app_data1(ctx, c);

#ifdef SSL_OP_NO_SESSION_RESUMPTION_ON_RENEGOTIATION
    /*
     * Disallow a session from being resumed during a renegotiation,
     * so that an acceptable cipher suite can be negotiated.
     */
    ssl_methods.SSL_CTX_ctrl((c->ctx),SSL_CTRL_OPTIONS,(SSL_OP_NO_SESSION_RESUMPTION_ON_RENEGOTIATION),NULL);
#endif
#ifdef SSL_MODE_RELEASE_BUFFERS
    /* Release idle buffers to the SSL_CTX free list */
    ssl_methods.SSL_CTX_ctrl((c->ctx),SSL_CTRL_MODE,(SSL_MODE_RELEASE_BUFFERS),NULL);
#endif
    setup_session_context(e, c);
    crypto_methods.EVP_Digest((const unsigned char *)SSL_DEFAULT_VHOST_NAME,
               (unsigned long)((sizeof SSL_DEFAULT_VHOST_NAME) - 1),
               &(c->context_id[0]), NULL, crypto_methods.EVP_sha1(), NULL);

    /* Set default Certificate verification level
     * and depth for the Client Authentication
     */
    c->verify_depth  = 1;
    c->verify_mode   = SSL_CVERIFY_UNSET;
    c->shutdown_type = SSL_SHUTDOWN_TYPE_UNSET;

    /* Set default password callback */
    //TODO: fixme, do we need to support these callbacks?
    //SSL_CTX_set_default_passwd_cb(c->ctx, (pem_password_cb *)SSL_password_callback);
    //SSL_CTX_set_default_passwd_cb_userdata(c->ctx, (void *)(&tcn_password_callback));
    //ssl_methods.SSL_CTX_set_info_callback(c->ctx, SSL_callback_handshake);

    /* Cache Java side SNI callback if not already cached */
    if (ssl_context_class == NULL) {
        ssl_context_class = (*e)->NewGlobalRef(e, o);
        sni_java_callback = (*e)->GetStaticMethodID(e, ssl_context_class,
                                                    "sniCallBack", "(JLjava/lang/String;)J");
    }

    /* Set up OpenSSL call back if SNI is provided by the client */
    ssl_methods.SSL_CTX_callback_ctrl(c->ctx,SSL_CTRL_SET_TLSEXT_SERVERNAME_CB,(void (*)(void))ssl_callback_ServerNameIndication);
    ssl_methods.SSL_CTX_ctrl(c->ctx,SSL_CTRL_SET_TLSEXT_SERVERNAME_ARG,0, (void *)c);

    /* Cache the byte[].class for performance reasons */
    clazz = (*e)->FindClass(e, "[B");
    byteArrayClass = (jclass) (*e)->NewGlobalRef(e, clazz);

    return P2J(c);
init_failed:
    return 0;
}


WF_OPENSSL(jobjectArray, getCiphers)(JNIEnv *e, jobject o, jlong ssl)
{
    STACK_OF(SSL_CIPHER) *sk;
    int len;
    jobjectArray array;
    SSL_CIPHER *cipher;
    const char *name;
    int i;
    jstring c_name;
    SSL *ssl_ = J2P(ssl, SSL *);

    if (ssl_ == NULL) {
        throwIllegalStateException(e, "ssl is null");
        return NULL;
    }

    sk = ssl_methods.SSL_get_ciphers(ssl_);

    len = crypto_methods.sk_num(sk);

    if (len <= 0) {
        /* No peer certificate chain as no auth took place yet, or the auth was not successful. */
        return NULL;
    }

    /* Create the byte[][] array that holds all the certs */
    array = (*e)->NewObjectArray(e, len, stringClass, NULL);

    for (i = 0; i < len; i++) {
        cipher = (SSL_CIPHER*) crypto_methods.sk_value(sk, i);
        name = ssl_methods.SSL_CIPHER_get_name(cipher);

        c_name = (*e)->NewStringUTF(e, name);
        (*e)->SetObjectArrayElement(e, array, i, c_name);
    }
    return array;
}

WF_OPENSSL(jboolean, setCipherSuites)(JNIEnv *e, jobject o, jlong ssl,
                                                         jstring ciphers)
{
    jboolean rv = JNI_TRUE;
    SSL *ssl_ = J2P(ssl, SSL *);
    TCN_ALLOC_CSTRING(ciphers);

    if (ssl_ == NULL) {
        throwIllegalStateException(e, "ssl is null");
        return JNI_FALSE;
    }

    UNREFERENCED(o);
    if (!J2S(ciphers)) {
        return JNI_FALSE;
    }
    if (!ssl_methods.SSL_set_cipher_list(ssl_, J2S(ciphers))) {
        char err[256];
        crypto_methods.ERR_error_string(crypto_methods.ERR_get_error(), err);
        throwIllegalStateException(e, err);
        rv = JNI_FALSE;
    }
    TCN_FREE_CSTRING(ciphers);
    return rv;
}

WF_OPENSSL(jint, freeSSLContext)(JNIEnv *e, jobject o, jlong ctx)
{
    tcn_ssl_ctxt_t *c = J2P(ctx, tcn_ssl_ctxt_t *);
    UNREFERENCED_STDARGS;
    /* Run and destroy the cleanup callback */
    if (c) {
        int i;
        if (c->crl) {
            crypto_methods.X509_STORE_free(c->crl);
        }
        c->crl = NULL;
        if (c->ctx) {
            ssl_methods.SSL_CTX_free(c->ctx);
        }
        c->ctx = NULL;
        for (i = 0; i < SSL_AIDX_MAX; i++) {
            if (c->certs[i]) {
                crypto_methods.X509_free(c->certs[i]);
                c->certs[i] = NULL;
            }
            if (c->keys[i]) {
                crypto_methods.EVP_PKEY_free(c->keys[i]);
                c->keys[i] = NULL;
            }
        }

        if (c->verifier) {
            JNIEnv *e;
            tcn_get_java_env(&e);
            (*e)->DeleteGlobalRef(e, c->verifier);
            c->verifier = NULL;
        }
        c->verifier_method = NULL;

        if (c->next_proto_data) {
            free(c->next_proto_data);
            c->next_proto_data = NULL;
        }
        c->next_proto_len = 0;
    }
    return 0;
}

WF_OPENSSL(void, setSSLContextOptions)(JNIEnv *e, jobject o, jlong ctx,
                                                 jint opt)
{
    tcn_ssl_ctxt_t *c = J2P(ctx, tcn_ssl_ctxt_t *);

    UNREFERENCED_STDARGS;
    TCN_ASSERT(ctx != 0);
//#ifndef SSL_OP_ALLOW_UNSAFE_LEGACY_RENEGOTIATION
    /* Clear the flag if not supported */
    if (opt & 0x00040000)
        opt &= ~0x00040000;
//#endif
	ssl_methods.SSL_CTX_ctrl((c->ctx),SSL_CTRL_OPTIONS,(opt),NULL);
}

WF_OPENSSL(void, clearSSLContextOptions)(JNIEnv *e, jobject o, jlong ctx,
                                                   jint opt)
{
    tcn_ssl_ctxt_t *c = J2P(ctx, tcn_ssl_ctxt_t *);

    UNREFERENCED_STDARGS;
    TCN_ASSERT(ctx != 0);
    ssl_methods.SSL_CTX_ctrl((c->ctx),SSL_CTRL_CLEAR_OPTIONS,(opt),NULL);
}

WF_OPENSSL(void, setSSLOptions)(JNIEnv *e, jobject o, jlong ssl,
                                                 jint opt)
{
    SSL *c = J2P(ssl, SSL *);

    UNREFERENCED_STDARGS;
//#ifndef SSL_OP_ALLOW_UNSAFE_LEGACY_RENEGOTIATION
    /* Clear the flag if not supported */
    if (opt & 0x00040000)
        opt &= ~0x00040000;
//#endif
    ssl_methods.SSL_ctrl(c,SSL_CTRL_OPTIONS,(opt),NULL);
}

WF_OPENSSL(void, clearSSLOptions)(JNIEnv *e, jobject o, jlong ssl,
                                                   jint opt)
{
    SSL *c = J2P(ssl, SSL *);

    UNREFERENCED_STDARGS;
    ssl_methods.SSL_ctrl(c,SSL_CTRL_CLEAR_OPTIONS,(opt),NULL);
}

WF_OPENSSL(jboolean, setCipherSuite)(JNIEnv *e, jobject o, jlong ctx,
                                                         jstring ciphers)
{
    tcn_ssl_ctxt_t *c = J2P(ctx, tcn_ssl_ctxt_t *);
    TCN_ALLOC_CSTRING(ciphers);
    jboolean rv = JNI_TRUE;
#ifndef HAVE_EXPORT_CIPHERS
    size_t len;
    char *buf;
#endif

    UNREFERENCED(o);
    TCN_ASSERT(ctx != 0);
    if (!J2S(ciphers))
        return JNI_FALSE;

#ifndef HAVE_EXPORT_CIPHERS
    /*
     *  Always disable NULL and export ciphers,
     *  no matter what was given in the config.
     */
    len = strlen(J2S(ciphers)) + strlen(SSL_CIPHERS_ALWAYS_DISABLED) + 1;
    buf = malloc(len * sizeof(char *));
    if (buf == NULL)
        return JNI_FALSE;
    memcpy(buf, SSL_CIPHERS_ALWAYS_DISABLED, strlen(SSL_CIPHERS_ALWAYS_DISABLED));
    memcpy(buf + strlen(SSL_CIPHERS_ALWAYS_DISABLED), J2S(ciphers), strlen(J2S(ciphers)));
    buf[len - 1] = '\0';
    if (!ssl_methods.SSL_CTX_set_cipher_list(c->ctx, buf)) {
#else
    if (!ssl_methods.SSL_CTX_set_cipher_list(c->ctx, J2S(ciphers))) {
#endif
        char err[256];
        crypto_methods.ERR_error_string(crypto_methods.ERR_get_error(), err);
        tcn_Throw(e, "Unable to configure permitted SSL ciphers (%s)", err);
        rv = JNI_FALSE;
    }
#ifndef HAVE_EXPORT_CIPHERS
    free(buf);
#endif
    TCN_FREE_CSTRING(ciphers);
    return rv;
}

WF_OPENSSL(jboolean, setCARevocation)(JNIEnv *e, jobject o, jlong ctx,
                                                          jstring file,
                                                          jstring path)
{
    tcn_ssl_ctxt_t *c = J2P(ctx, tcn_ssl_ctxt_t *);
    TCN_ALLOC_CSTRING(file);
    TCN_ALLOC_CSTRING(path);
    jboolean rv = JNI_FALSE;
    X509_LOOKUP *lookup;
    char err[256];

    UNREFERENCED(o);
    TCN_ASSERT(ctx != 0);
    if (J2S(file) == NULL && J2S(path) == NULL)
        return JNI_FALSE;

    if (!c->crl) {
        if ((c->crl = crypto_methods.X509_STORE_new()) == NULL)
            goto cleanup;
    }
    if (J2S(file)) {
        lookup = crypto_methods.X509_STORE_add_lookup(c->crl, crypto_methods.X509_LOOKUP_file());
        if (lookup == NULL) {
            crypto_methods.ERR_error_string(crypto_methods.ERR_get_error(), err);
            crypto_methods.X509_STORE_free(c->crl);
            c->crl = NULL;
            tcn_Throw(e, "Lookup failed for file %s (%s)", J2S(file), err);
            goto cleanup;
        }
        crypto_methods.X509_LOOKUP_ctrl((lookup),X509_L_FILE_LOAD,(J2S(file)),(long)(X509_FILETYPE_PEM),NULL);
    }
    if (J2S(path)) {
        lookup = crypto_methods.X509_STORE_add_lookup(c->crl, crypto_methods.X509_LOOKUP_hash_dir());
        if (lookup == NULL) {
            crypto_methods.ERR_error_string(crypto_methods.ERR_get_error(), err);
            crypto_methods.X509_STORE_free(c->crl);
            c->crl = NULL;
            tcn_Throw(e, "Lookup failed for path %s (%s)", J2S(file), err);
            goto cleanup;
        }
        crypto_methods.X509_LOOKUP_ctrl((lookup),X509_L_ADD_DIR,(J2S(path)),(long)(X509_FILETYPE_PEM),NULL);
    }
    rv = JNI_TRUE;
cleanup:
    TCN_FREE_CSTRING(file);
    TCN_FREE_CSTRING(path);
    return rv;
}

WF_OPENSSL(jboolean, setCertificate)(JNIEnv *e, jobject o, jlong ctx,
                                                         jbyteArray javaCert, jbyteArray javaKey, jint idx)
{
    /* we get the key contents into a byte array */
    jbyte* bufferPtr = (*e)->GetByteArrayElements(e, javaKey, NULL);
    jsize lengthOfKey = (*e)->GetArrayLength(e, javaKey);
    unsigned char* key = malloc(lengthOfKey);
    memcpy(key, bufferPtr, lengthOfKey);
    (*e)->ReleaseByteArrayElements(e, javaKey, bufferPtr, 0);

    bufferPtr = (*e)->GetByteArrayElements(e, javaCert, NULL);
    jsize lengthOfCert = (*e)->GetArrayLength(e, javaCert);
    unsigned char* cert = malloc(lengthOfCert);
    memcpy(cert, bufferPtr, lengthOfCert);
    (*e)->ReleaseByteArrayElements(e, javaCert, bufferPtr, 0);

    tcn_ssl_ctxt_t *c = J2P(ctx, tcn_ssl_ctxt_t *);
    jboolean rv = JNI_TRUE;
    char err[256];

    UNREFERENCED(o);
    TCN_ASSERT(ctx != 0);

    if (idx < 0 || idx >= SSL_AIDX_MAX) {
        throwIllegalStateException(e, "Invalid key type");
        rv = JNI_FALSE;
        goto cleanup;
    }
    const unsigned char *tmp = (const unsigned char *)cert;
    if ((c->certs[idx] = crypto_methods.d2i_X509(NULL, &tmp, lengthOfCert)) == NULL) {
        crypto_methods.ERR_error_string(crypto_methods.ERR_get_error(), err);
        throwIllegalStateException(e, err);
        rv = JNI_FALSE;
        goto cleanup;
    }

    EVP_PKEY * evp = malloc(sizeof(EVP_PKEY));
    memset(evp, 0, sizeof(EVP_PKEY));
    if(c->keys[idx] != NULL) {
        free(c->keys[idx]);
    }
    c->keys[idx] = evp;

    BIO * bio = crypto_methods.BIO_new(crypto_methods.BIO_s_mem());
    crypto_methods.BIO_write(bio, key, lengthOfKey);

    c->keys[idx] = crypto_methods.PEM_read_bio_PrivateKey(bio, NULL, 0, NULL);
    crypto_methods.BIO_free(bio);
    if (c->keys[idx] == NULL) {
        crypto_methods.ERR_error_string(crypto_methods.ERR_get_error(), err);
        throwIllegalStateException(e, err);
        rv = JNI_FALSE;
        goto cleanup;
    }

    if (ssl_methods.SSL_CTX_use_certificate(c->ctx, c->certs[idx]) <= 0) {
        crypto_methods.ERR_error_string(crypto_methods.ERR_get_error(), err);
        tcn_Throw(e, "Error setting certificate (%s)", err);
        rv = JNI_FALSE;
        goto cleanup;
    }
    if (ssl_methods.SSL_CTX_use_PrivateKey(c->ctx, c->keys[idx]) <= 0) {
        crypto_methods.ERR_error_string(crypto_methods.ERR_get_error(), err);
        tcn_Throw(e, "Error setting private key (%s)", err);
        rv = JNI_FALSE;
        goto cleanup;
    }
    if (ssl_methods.SSL_CTX_check_private_key(c->ctx) <= 0) {
        crypto_methods.ERR_error_string(crypto_methods.ERR_get_error(), err);
        tcn_Throw(e, "Private key does not match the certificate public key (%s)",
                  err);
        rv = JNI_FALSE;
        goto cleanup;
    }
    //TODO: read DH and ECC params?

cleanup:
    free(key);
    free(cert);
    return rv;
}

/*
 * Adapted from Android:
 * https://android.googlesource.com/platform/external/openssl/+/master/patches/0003-jsse.patch
 */
const char* SSL_CIPHER_authentication_method(const SSL_CIPHER* cipher){
    switch (cipher->algorithm_mkey)
        {
    case SSL_kRSA:
        return SSL_TXT_RSA;
    case SSL_kDHr:
        return SSL_TXT_DH "_" SSL_TXT_RSA;
    case SSL_kDHd:
        return SSL_TXT_DH "_" SSL_TXT_DSS;
    case SSL_kEDH:
        switch (cipher->algorithm_auth)
            {
        case SSL_aDSS:
            return "DHE_" SSL_TXT_DSS;
        case SSL_aRSA:
            return "DHE_" SSL_TXT_RSA;
        case SSL_aNULL:
            return SSL_TXT_DH "_anon";
        default:
            return "UNKNOWN";
            }
    case SSL_kKRB5:
        return SSL_TXT_KRB5;
    case SSL_kECDHr:
        return SSL_TXT_ECDH "_" SSL_TXT_RSA;
    case SSL_kECDHe:
        return SSL_TXT_ECDH "_" SSL_TXT_ECDSA;
    case SSL_kEECDH:
        switch (cipher->algorithm_auth)
            {
        case SSL_aECDSA:
            return "ECDHE_" SSL_TXT_ECDSA;
        case SSL_aRSA:
            return "ECDHE_" SSL_TXT_RSA;
        case SSL_aNULL:
            return SSL_TXT_ECDH "_anon";
        default:
            return "UNKNOWN";
            }
    default:
        return "UNKNOWN";
    }
}

static const char* SSL_authentication_method(const SSL* ssl) {
{
    switch (ssl->version)
        {
        case SSL2_VERSION:
            return SSL_TXT_RSA;
        default:
            return SSL_CIPHER_authentication_method(ssl->s3->tmp.new_cipher);
        }
    }
}
/* Android end */

static int SSL_cert_verify(X509_STORE_CTX *ctx, void *arg) {
    /* Get Apache context back through OpenSSL context */
    SSL *ssl = crypto_methods.X509_STORE_CTX_get_ex_data(ctx, ssl_methods.SSL_get_ex_data_X509_STORE_CTX_idx());
    tcn_ssl_ctxt_t *c = SSL_get_app_data2(ssl);


    // Get a stack of all certs in the chain
    STACK_OF(X509) *sk = ctx->untrusted;

    int len = crypto_methods.sk_num(sk);;
    unsigned i;
    X509 *cert;
    int length;
    unsigned char *buf;
    JNIEnv *e;
    jbyteArray array;
    jbyteArray bArray;
    const char *authMethod;
    jstring authMethodString;
    jboolean result;
    int r;
    tcn_get_java_env(&e);

    // Create the byte[][] array that holds all the certs
    array = (*e)->NewObjectArray(e, len, byteArrayClass, NULL);

    for(i = 0; i < len; i++) {
        cert = (X509*) crypto_methods.sk_value(sk, i);

        buf = NULL;
        length = crypto_methods.i2d_X509(cert, &buf);
        if (length < 0) {
            // In case of error just return an empty byte[][]
            array = (*e)->NewObjectArray(e, 0, byteArrayClass, NULL);
            // We need to delete the local references so we not leak memory as this method is called via callback.
            crypto_methods.CRYPTO_free(buf);
            break;
        }
        bArray = (*e)->NewByteArray(e, length);
        (*e)->SetByteArrayRegion(e, bArray, 0, length, (jbyte*) buf);
        (*e)->SetObjectArrayElement(e, array, i, bArray);

        // Delete the local reference as we not know how long the chain is and local references are otherwise
        // only freed once jni method returns.
        (*e)->DeleteLocalRef(e, bArray);
        crypto_methods.CRYPTO_free(buf);
    }

    authMethod = SSL_authentication_method(ssl);
    authMethodString = (*e)->NewStringUTF(e, authMethod);

    result = (*e)->CallBooleanMethod(e, c->verifier, c->verifier_method, P2J(ssl), array,
            authMethodString);

    r = result == JNI_TRUE ? 1 : 0;

    // We need to delete the local references so we not leak memory as this method is called via callback.
    (*e)->DeleteLocalRef(e, authMethodString);
    (*e)->DeleteLocalRef(e, array);
    return r;
}


WF_OPENSSL(void, setCertVerifyCallback)(JNIEnv *e, jobject o, jlong ctx, jobject verifier)
{
    tcn_ssl_ctxt_t *c = J2P(ctx, tcn_ssl_ctxt_t *);

    UNREFERENCED(o);
    TCN_ASSERT(ctx != 0);

    if (verifier == NULL) {
        ssl_methods.SSL_CTX_set_cert_verify_callback(c->ctx, NULL, NULL);
    } else {
        jclass verifier_class = (*e)->GetObjectClass(e, verifier);
        jmethodID method = (*e)->GetMethodID(e, verifier_class, "verify", "(J[[BLjava/lang/String;)Z");

        if (method == NULL) {
            return;
        }
        // Delete the reference to the previous specified verifier if needed.
        if (c->verifier != NULL) {
            (*e)->DeleteLocalRef(e, c->verifier);
        }
        c->verifier = (*e)->NewGlobalRef(e, verifier);
        c->verifier_method = method;

        ssl_methods.SSL_CTX_set_cert_verify_callback(c->ctx, SSL_cert_verify, NULL);
    }
}

WF_OPENSSL(jboolean, setSessionIdContext)(JNIEnv *e, jobject o, jlong ctx, jbyteArray sidCtx)
{
    tcn_ssl_ctxt_t *c = J2P(ctx, tcn_ssl_ctxt_t *);
    int len = (*e)->GetArrayLength(e, sidCtx);
    unsigned char *buf;
    int res;

    UNREFERENCED(o);
    TCN_ASSERT(ctx != 0);

    buf = malloc(len);

    (*e)->GetByteArrayRegion(e, sidCtx, 0, len, (jbyte*) buf);

    res = ssl_methods.SSL_CTX_set_session_id_context(c->ctx, buf, len);
    free(buf);

    if (res == 1) {
        return JNI_TRUE;
    }
    return JNI_FALSE;
}


static void ssl_info_callback(const SSL *ssl, int where, int ret) {
    int *handshakeCount = NULL;
    if (0 != (where & SSL_CB_HANDSHAKE_START)) {
        handshakeCount = (int*) SSL_get_app_data3(ssl);
        if (handshakeCount != NULL) {
            ++(*handshakeCount);
        }
    }
    if (0 != (where & SSL_CB_HANDSHAKE_DONE)) {
        tcn_ssl_conn_t *con = SSL_get_app_data1(ssl);
        con->handshake_done = 1;
    }
}

WF_OPENSSL(jlong, newSSL)(JNIEnv *e, jobject o, jlong ctx /* tcn_ssl_ctxt_t * */,
                                                   jboolean server) {
    tcn_ssl_ctxt_t *c = J2P(ctx, tcn_ssl_ctxt_t *);
    int *handshakeCount = malloc(sizeof(int));
    SSL *ssl;
    tcn_ssl_conn_t *con;

    UNREFERENCED_STDARGS;

    TCN_ASSERT(ctx != 0);
    ssl = ssl_methods.SSL_new(c->ctx);
    if (ssl == NULL) {
        throwIllegalStateException(e, "cannot create new ssl");
        return 0;
    }
    if ((con = malloc(sizeof(tcn_ssl_conn_t))) == NULL) {
        throwIllegalStateException(e, "Failed to allocate memory");
        return 0;
    }
    memset(con, 0, sizeof(*con));
    con->ctx  = c;
    con->ssl  = ssl;
    con->shutdown_type = c->shutdown_type;

    /* Store the handshakeCount in the SSL instance. */
    *handshakeCount = 0;
    SSL_set_app_data3(ssl, handshakeCount);

    if (server) {
        ssl_methods.SSL_set_accept_state(ssl);
    } else {
        ssl_methods.SSL_set_connect_state(ssl);
    }
    ssl_methods.SSL_set_read_ahead(ssl, 0);

    /* Setup verify and seed */
    ssl_methods.SSL_set_verify_result(ssl, X509_V_OK);
    //TODO: do we need our seed? It seems the default seed should be more secure
    //SSL_rand_seed(c->rand_file);
    /* Add callback to keep track of handshakes and the handshake state */
    ssl_methods.SSL_set_info_callback(ssl, &ssl_info_callback);

    /* Store for later usage in SSL_callback_SSL_verify */
    SSL_set_app_data1(ssl, con);
    SSL_set_app_data2(ssl, c);
    return P2J(ssl);
}


/* Free the SSL * and its associated internal BIO */
WF_OPENSSL(void, freeSSL)(JNIEnv *e, jobject o, jlong ssl /* SSL * */) {
    SSL *ssl_ = J2P(ssl, SSL *);
    int *handshakeCount = SSL_get_app_data3(ssl_);

    if (handshakeCount != NULL) {
        free(handshakeCount);
    }

    tcn_ssl_conn_t *con = SSL_get_app_data1(ssl_);
    if(con->alpn_selection_callback != NULL) {
        (*e)->DeleteGlobalRef(e, con->alpn_selection_callback);
    }
    free(con);
    ssl_methods.SSL_free(ssl_);
}


WF_OPENSSL(jlong, bufferAddress)(JNIEnv *e, jobject o, jobject bb)
{
    UNREFERENCED(o);
    if(bb == NULL) {
        throwIllegalArgumentException(e, "Buffer was null");
    }
    return P2J((*e)->GetDirectBufferAddress(e, bb));
}


/* Make a BIO pair (network and internal) for the provided SSL * and return the network BIO */
WF_OPENSSL(jlong, makeNetworkBIO)(JNIEnv *e, jobject o, jlong ssl /* SSL * */) {
    SSL *ssl_ = J2P(ssl, SSL *);
    BIO *internal_bio;
    BIO *network_bio;

    UNREFERENCED(o);

    if (ssl_ == NULL) {
        throwIllegalStateException(e, "ssl is null");
        goto fail;
    }

    if (crypto_methods.BIO_new_bio_pair(&internal_bio, 0, &network_bio, 0) != 1) {
        throwIllegalStateException(e, "BIO_new_bio_pair failed");
        goto fail;
    }

    ssl_methods.SSL_set_bio(ssl_, internal_bio, internal_bio);

    return P2J(network_bio);
 fail:
    return 0;
}


WF_OPENSSL(jint, doHandshake)(JNIEnv *e, jobject o, jlong ssl /* SSL * */) {
    SSL *ssl_ = J2P(ssl, SSL *);
    if (ssl_ == NULL) {
        throwIllegalStateException(e, "ssl is null");
        return 0;
    }

    UNREFERENCED(o);

    return ssl_methods.SSL_do_handshake(ssl_);
}


WF_OPENSSL(jint, getSSLError)(JNIEnv *e, jobject o, jlong ssl, jlong code)
{
    const SSL *ssl_ = J2P(ssl, SSL *);
    UNREFERENCED(o);
    return ssl_methods.SSL_get_error(ssl_, code);
}

WF_OPENSSL(jint, renegotiate)(JNIEnv *e, jobject o, jlong ssl /* SSL * */) {
    SSL *ssl_ = J2P(ssl, SSL *);
    if (ssl_ == NULL) {
        throwIllegalStateException(e, "ssl is null");
        return 0;
    }

    UNREFERENCED(o);

    return ssl_methods.SSL_renegotiate(ssl_);
}


WF_OPENSSL(jint, getLastErrorNumber)(JNIEnv *e, jobject o) {
    return crypto_methods.ERR_get_error();
}

WF_OPENSSL(jint /* nbytes */, pendingWrittenBytesInBIO)(JNIEnv *e, jobject o,
                                                                     jlong bio /* BIO * */) {
    UNREFERENCED_STDARGS;

    return crypto_methods.BIO_ctrl_pending(J2P(bio, BIO *));
}

/* How much is available for reading in the given SSL struct? */
WF_OPENSSL(jint, pendingReadableBytesInSSL)(JNIEnv *e, jobject o, jlong ssl /* SSL * */) {
    UNREFERENCED_STDARGS;

    return ssl_methods.SSL_pending(J2P(ssl, SSL *));
}

/* Write wlen bytes from wbuf into bio */
WF_OPENSSL(jint /* status */, writeToBIO)(JNIEnv *e, jobject o,
                                                       jlong bio /* BIO * */,
                                                       jlong wbuf /* char* */,
                                                       jint wlen /* sizeof(wbuf) */) {
    UNREFERENCED_STDARGS;

    return crypto_methods.BIO_write(J2P(bio, BIO *), J2P(wbuf, void *), wlen);

}

/* Read up to rlen bytes from bio into rbuf */
WF_OPENSSL(jint /* status */, readFromBIO)(JNIEnv *e, jobject o,
                                                        jlong bio /* BIO * */,
                                                        jlong rbuf /* char * */,
                                                        jint rlen /* sizeof(rbuf) - 1 */) {
    UNREFERENCED_STDARGS;

    return crypto_methods.BIO_read(J2P(bio, BIO *), J2P(rbuf, void *), rlen);
}

/* Write up to wlen bytes of application data to the ssl BIO (encrypt) */
WF_OPENSSL(jint /* status */, writeToSSL)(JNIEnv *e, jobject o,
                                                       jlong ssl /* SSL * */,
                                                       jlong wbuf /* char * */,
                                                       jint wlen /* sizeof(wbuf) */) {
    UNREFERENCED_STDARGS;

    return ssl_methods.SSL_write(J2P(ssl, SSL *), J2P(wbuf, void *), wlen);
}

/* Read up to rlen bytes of application data from the given SSL BIO (decrypt) */
WF_OPENSSL(jint /* status */, readFromSSL)(JNIEnv *e, jobject o,
                                                        jlong ssl /* SSL * */,
                                                        jlong rbuf /* char * */,
                                                        jint rlen /* sizeof(rbuf) - 1 */) {
    UNREFERENCED_STDARGS;

    return ssl_methods.SSL_read(J2P(ssl, SSL *), J2P(rbuf, void *), rlen);
}

/* Get the shutdown status of the engine */
WF_OPENSSL(jint /* status */, getShutdown)(JNIEnv *e, jobject o,
                                                        jlong ssl /* SSL * */) {
    UNREFERENCED_STDARGS;

    return ssl_methods.SSL_get_shutdown(J2P(ssl, SSL *));
}

WF_OPENSSL(jint, isInInit)(JNIEnv *e, jobject o,
                                        jlong ssl /* SSL * */) {
    SSL *ssl_ = J2P(ssl, SSL *);

    UNREFERENCED(o);

    if (ssl_ == NULL) {
        throwIllegalStateException(e, "ssl is null");
        return 0;
    } else {
        tcn_ssl_conn_t *con = SSL_get_app_data1(ssl_);
        return con->handshake_done == 0 ? 1 : 0;
    }
}

/* Free a BIO * (typically, the network BIO) */
WF_OPENSSL(void, freeBIO)(JNIEnv *e, jobject o,
                                       jlong bio /* BIO * */) {
    BIO *bio_;
    UNREFERENCED_STDARGS;

    bio_ = J2P(bio, BIO *);
    crypto_methods.BIO_free(bio_);
}


WF_OPENSSL(jstring, getErrorString)(JNIEnv *e, jobject o, jlong number)
{
    char buf[256];
    UNREFERENCED(o);
    crypto_methods.ERR_error_string(number, buf);
    return tcn_new_string(e, buf);
}

/* Read which cipher was negotiated for the given SSL *. */
WF_OPENSSL(jstring, getCipherForSSL)(JNIEnv *e, jobject o, jlong ssl /* SSL * */)
{
    return AJP_TO_JSTRING(ssl_methods.SSL_CIPHER_get_name(ssl_methods.SSL_get_current_cipher(J2P(ssl, SSL*))));
}


/* Read which protocol was negotiated for the given SSL *. */
WF_OPENSSL(jstring, getVersion)(JNIEnv *e, jobject o, jlong ssl /* SSL * */)
{
    return AJP_TO_JSTRING(ssl_methods.SSL_get_version(J2P(ssl, SSL*)));
}


WF_OPENSSL(jobjectArray, getPeerCertChain)(JNIEnv *e, jobject o,
                                                  jlong ssl /* SSL * */)
{
    STACK_OF(X509) *sk;
    int len;
    int i;
    X509 *cert;
    int length;
    unsigned char *buf;
    jobjectArray array;
    jbyteArray bArray;

    SSL *ssl_ = J2P(ssl, SSL *);

    if (ssl_ == NULL) {
        throwIllegalStateException(e, "ssl is null");
        return NULL;
    }

    UNREFERENCED(o);

    // Get a stack of all certs in the chain.
    sk = ssl_methods.SSL_get_peer_cert_chain(ssl_);

    len = crypto_methods.sk_num(sk);
    if (len <= 0) {

        /* No peer certificate chain as no auth took place yet, or the auth was not successful. */
        return NULL;
    }
    /* Create the byte[][] array that holds all the certs */
    array = (*e)->NewObjectArray(e, len, byteArrayClass, NULL);

    for(i = 0; i < len; i++) {
        cert = (X509*) crypto_methods.sk_value(sk, i);

        buf = NULL;
        length = crypto_methods.i2d_X509(cert, &buf);
        if (length < 0) {
            crypto_methods.CRYPTO_free(buf);
            /* In case of error just return an empty byte[][] */
            return (*e)->NewObjectArray(e, 0, byteArrayClass, NULL);
        }
        bArray = (*e)->NewByteArray(e, length);
        (*e)->SetByteArrayRegion(e, bArray, 0, length, (jbyte*) buf);
        (*e)->SetObjectArrayElement(e, array, i, bArray);

        /*
         * Delete the local reference as we not know how long the chain is and local references are otherwise
         * only freed once jni method returns.
         */
        (*e)->DeleteLocalRef(e, bArray);

        crypto_methods.CRYPTO_free(buf);
    }
    return array;
}


/* Send CLOSE_NOTIFY to peer */
WF_OPENSSL(jint , shutdownSSL)(JNIEnv *e, jobject o, jlong ssl) {
    return ssl_methods.SSL_shutdown(J2P(ssl, SSL *));
}


WF_OPENSSL(jbyteArray, getPeerCertificate)(JNIEnv *e, jobject o,
                                                  jlong ssl /* SSL * */)
{
    X509 *cert;
    int length;
    unsigned char *buf = NULL;
    jbyteArray bArray;

    SSL *ssl_ = J2P(ssl, SSL *);

    if (ssl_ == NULL) {
        throwIllegalStateException(e, "ssl is null");
        return NULL;
    }

    UNREFERENCED(o);

    /* Get a stack of all certs in the chain */
    cert = ssl_methods.SSL_get_peer_certificate(ssl_);
    if (cert == NULL) {
        return NULL;
    }

    length = crypto_methods.i2d_X509(cert, &buf);

    bArray = (*e)->NewByteArray(e, length);
    (*e)->SetByteArrayRegion(e, bArray, 0, length, (jbyte*) buf);

    /*
     * We need to free the cert as the reference count is incremented by one and it is not destroyed when the
     * session is freed.
     * See https://www.openssl.org/docs/ssl/SSL_get_peer_certificate.html
     */
    crypto_methods.X509_free(cert);

    crypto_methods.CRYPTO_free(buf);

    return bArray;
}


WF_OPENSSL(jlong, version)(JNIEnv *e)
{
    return ssl_methods.SSLeay();
}
