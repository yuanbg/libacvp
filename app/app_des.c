/*
 * Copyright (c) 2019, Cisco Systems, Inc.
 *
 * Licensed under the Apache License 2.0 (the "License").  You may not use
 * this file except in compliance with the License.  You can obtain a copy
 * in the file LICENSE in the source distribution or at
 * https://github.com/cisco/libacvp/LICENSE
 */


#include <openssl/evp.h>

#include "acvp/acvp.h"
#include "app_lcl.h"
#include "safe_lib.h"
#ifdef ACVP_NO_RUNTIME
# include "app_fips_lcl.h"
#endif

static EVP_CIPHER_CTX *glb_cipher_ctx = NULL; /* need to maintain across calls for MCT */

void app_des_cleanup(void) {
    if (glb_cipher_ctx) EVP_CIPHER_CTX_free(glb_cipher_ctx);
    glb_cipher_ctx = NULL;
}

int app_des_handler(ACVP_TEST_CASE *test_case) {
    ACVP_SYM_CIPHER_TC      *tc;
    EVP_CIPHER_CTX *cipher_ctx;
    const EVP_CIPHER        *cipher;
    unsigned char *iv = 0;

    if (!test_case) {
        return 1;
    }

    tc = test_case->tc.symmetric;

    /*
     * We only support 3 key DES
     */
    if (tc->key_len != 192) {
        printf("Unsupported DES key length\n");
        return 1;
    }

    if (glb_cipher_ctx == NULL) {
        glb_cipher_ctx = EVP_CIPHER_CTX_new();
        if (glb_cipher_ctx == NULL) {
            printf("Failed to allocate global cipher_ctx");
            return 1;
        }
    }

    /* Begin encrypt code section */
    cipher_ctx = glb_cipher_ctx;

    switch (tc->cipher) {
    case ACVP_TDES_ECB:
        cipher = EVP_des_ede3_ecb();
        break;
    case ACVP_TDES_CBC:
        iv = tc->iv;
        cipher = EVP_des_ede3_cbc();
        break;
    case ACVP_TDES_OFB:
        iv = tc->iv;
        cipher = EVP_des_ede3_ofb();
        break;
    case ACVP_TDES_CFB64:
        iv = tc->iv;
        cipher = EVP_des_ede3_cfb64();
        break;
    case ACVP_TDES_CFB8:
        iv = tc->iv;
        cipher = EVP_des_ede3_cfb8();
        break;
    case ACVP_TDES_CFB1:
        iv = tc->iv;
        cipher = EVP_des_ede3_cfb1();
        break;
    case ACVP_TDES_CTR:
    /*
     * IMPORTANT: if this mode is supported in your crypto module,
     * you will need to fill that out here. It is set to fall
     * through as an unsupported mode.
     */
    default:
        printf("Error: Unsupported DES mode requested by ACVP server\n");
        return 1;

        break;
    }

    /* If Monte Carlo we need to be able to init and then update
     * one thousand times before we complete each iteration.
     */
    if (tc->test_type == ACVP_SYM_TEST_TYPE_MCT) {
        const unsigned char *ctx_iv = NULL;


#if OPENSSL_VERSION_NUMBER <= 0x10100000L
        ctx_iv = cipher_ctx->iv;
#else
        ctx_iv = EVP_CIPHER_CTX_iv(cipher_ctx);
#endif

#define SYM_IV_BYTE_MAX 128
        if (tc->direction == ACVP_SYM_CIPH_DIR_ENCRYPT) {
            if (tc->mct_index == 0) {
                EVP_CipherInit_ex(cipher_ctx, cipher, NULL, tc->key, iv, 1);
                EVP_CIPHER_CTX_set_padding(cipher_ctx, 0);
            } else {
                /* TDES needs the pre-operation IV returned */
                memcpy_s(tc->iv_ret, SYM_IV_BYTE_MAX, ctx_iv, 8);
            }
            if (tc->cipher == ACVP_TDES_CFB1) {
                EVP_CIPHER_CTX_set_flags(cipher_ctx, EVP_CIPH_FLAG_LENGTH_BITS);
            }

            EVP_Cipher(cipher_ctx, tc->ct, tc->pt, tc->pt_len);
            tc->ct_len = tc->pt_len;
            /* TDES needs the post-operation IV returned */
            memcpy_s(tc->iv_ret_after, SYM_IV_BYTE_MAX, ctx_iv, 8);
        } else if (tc->direction == ACVP_SYM_CIPH_DIR_DECRYPT) {
            if (tc->mct_index == 0) {
                EVP_CipherInit_ex(cipher_ctx, cipher, NULL, tc->key, iv, 0);
                EVP_CIPHER_CTX_set_padding(cipher_ctx, 0);
            } else {
                /* TDES needs the pre-operation IV returned */
                memcpy_s(tc->iv_ret, SYM_IV_BYTE_MAX, ctx_iv, 8);
            }
            if (tc->cipher == ACVP_TDES_CFB1) {
                EVP_CIPHER_CTX_set_flags(cipher_ctx, EVP_CIPH_FLAG_LENGTH_BITS);
            }
            EVP_Cipher(cipher_ctx, tc->pt, tc->ct, tc->ct_len);
            tc->pt_len = tc->ct_len;
            /* TDES needs the post-operation IV returned */
            memcpy_s(tc->iv_ret_after, SYM_IV_BYTE_MAX, ctx_iv, 8);
        } else {
            printf("Unsupported direction\n");
            return 1;
        }
        if (tc->mct_index == 9999) {
            EVP_CIPHER_CTX_cleanup(cipher_ctx);
        }
    } else {
        if (tc->direction == ACVP_SYM_CIPH_DIR_ENCRYPT) {
            EVP_CipherInit_ex(cipher_ctx, cipher, NULL, tc->key, iv, 1);
            EVP_CIPHER_CTX_set_padding(cipher_ctx, 0);
            if (tc->cipher == ACVP_TDES_CFB1) {
                EVP_CIPHER_CTX_set_flags(cipher_ctx, EVP_CIPH_FLAG_LENGTH_BITS);
            }
            EVP_Cipher(cipher_ctx, tc->ct, tc->pt, tc->pt_len);
            tc->ct_len = tc->pt_len;
        } else if (tc->direction == ACVP_SYM_CIPH_DIR_DECRYPT) {
            EVP_CipherInit_ex(cipher_ctx, cipher, NULL, tc->key, iv, 0);
            EVP_CIPHER_CTX_set_padding(cipher_ctx, 0);
            if (tc->cipher == ACVP_TDES_CFB1) {
                EVP_CIPHER_CTX_set_flags(cipher_ctx, EVP_CIPH_FLAG_LENGTH_BITS);
            }
            EVP_Cipher(cipher_ctx, tc->pt, tc->ct, tc->ct_len);
            tc->pt_len = tc->ct_len;
        } else {
            printf("Unsupported direction\n");
            return 1;
        }

        EVP_CIPHER_CTX_cleanup(cipher_ctx);
    }

    return 0;
}

