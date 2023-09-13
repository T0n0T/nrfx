/**
 * @file ccm3310_sm4.c
 * @author your name (you@domain.com)
 * @brief
 * @version 0.1
 * @date 2023-08-24
 *
 * @copyright Copyright (c) 2023
 *
 */

#include <ccm3310.h>
#include <stdlib.h>
#define RECV_PRE_LEN 30

uint8_t ccm3310_sm4_setkey(uint8_t *key)
{
    uint8_t *pack          = (uint8_t *)rt_malloc(100);
    ccm3310_key_data *data = (ccm3310_key_data *)rt_malloc(sizeof(ccm3310_key_data) + 16 - 1);
    data->version          = 0x00;
    data->key_id           = 0x00;
    data->algo             = 0x84;
    data->len              = 0x10;
    for (size_t i = 0; i < 16; i++) {
        *(&data->key_data + i) = *(key + i);
    }

    uint8_t *id  = 0;
    int send_len = 0;
    send_len     = encode(0x80, 0x42, 0x00, 0x00, pack, (uint8_t *)data, sizeof(ccm3310_key_data) + 16 - 1);
    ccm3310_transfer(pack, send_len, &id, 21);
    rt_free((void *)pack);
    rt_free((void *)data);
    return *id;
}

uint8_t ccm3310_sm4_updatekey(uint8_t update_id, uint8_t *key)
{
    uint8_t *pack          = (uint8_t *)rt_malloc(100);
    ccm3310_key_data *data = (ccm3310_key_data *)rt_malloc(sizeof(ccm3310_key_data) + 16 - 1);
    data->version          = 0x00;
    data->key_id           = update_id;
    data->algo             = 0x84;
    data->len              = 0x10;
    for (size_t i = 0; i < 16; i++) {
        *(&data->key_data + i) = *(key + i);
    }

    uint8_t *id  = 0;
    int send_len = 0;
    send_len     = encode(0x80, 0x42, 0x01, 0x00, pack, (uint8_t *)data, sizeof(ccm3310_key_data) + 16 - 1);
    ccm3310_transfer(pack, send_len, &id, 21);
    rt_free((void *)pack);
    rt_free((void *)data);
    return *id;
}
/**
 * @brief 加密sm4
 *
 * @param key_id 对称密钥id
 * @param p 待加密数据
 * @return ciphertext 密文
 */
ciphertext ccm3310_sm4_encrypt(uint8_t key_id, pdata p)
{
    int plen                                   = numalgin(p.len, 16);
    int sm4_crypt_struct_len                   = sizeof(ccm3310_sym_crypt_data) + plen - 1;
    ccm3310_sym_crypt_data *sm4_encrypt_struct = (ccm3310_sym_crypt_data *)rt_calloc(1, sm4_crypt_struct_len);
    uint8_t *pack                              = (uint8_t *)rt_calloc(1, sm4_crypt_struct_len + RECV_PRE_LEN);
    sm4_encrypt_struct->version                = 0;
    sm4_encrypt_struct->key_id                 = key_id;
    sm4_encrypt_struct->mode                   = 0x00;
    sm4_encrypt_struct->sk_sel                 = 0x00;
    sm4_encrypt_struct->keybitlen              = 0x00;
    sm4_encrypt_struct->feedbitlen             = 0x00;
    sm4_encrypt_struct->lvlen                  = 0;

    for (size_t i = 0; i < plen; i++) {
        if (i < p.len) {
            *(&sm4_encrypt_struct->text + i) = *(p.data + i);
        } else {
            *(&sm4_encrypt_struct->text + i) = 0;
        }
    }

    ciphertext encrypt_data;
    int send_len     = encode(0x80, 0x44, 0x00, 0x00, pack, (uint8_t *)sm4_encrypt_struct, sm4_crypt_struct_len);
    encrypt_data.len = ccm3310_transfer(pack, send_len, &encrypt_data.data, plen + RECV_PRE_LEN);
    rt_free((void *)pack);
    rt_free((void *)sm4_encrypt_struct);
    return encrypt_data;
}

/**
 * @brief 解密sm4数据
 *
 * @param key_id 对称密钥id
 * @param p 待加密数据
 * @return plaintext 明文
 */
plaintext ccm3310_sm4_decrypt(uint8_t key_id, pdata p)
{
    int plen                                   = numalgin(p.len, 16);
    int sm4_crypt_struct_len                   = sizeof(ccm3310_sym_crypt_data) + plen - 1;
    ccm3310_sym_crypt_data *sm4_decrypt_struct = (ccm3310_sym_crypt_data *)rt_calloc(1, sm4_crypt_struct_len);
    uint8_t *pack                              = (uint8_t *)rt_calloc(1, sm4_crypt_struct_len + RECV_PRE_LEN);
    sm4_decrypt_struct->version                = 0;
    sm4_decrypt_struct->key_id                 = key_id;
    sm4_decrypt_struct->mode                   = 0x00;
    sm4_decrypt_struct->sk_sel                 = 0x00;
    sm4_decrypt_struct->keybitlen              = 0x00;
    sm4_decrypt_struct->feedbitlen             = 0x00;
    sm4_decrypt_struct->lvlen                  = 0;

    for (size_t i = 0; i < plen; i++) {
        if (i < p.len) {
            *(&sm4_decrypt_struct->text + i) = *(p.data + i);
        } else {
            *(&sm4_decrypt_struct->text + i) = 0;
        }
    }

    plaintext encrypt_data;
    int send_len     = encode(0x80, 0x46, 0x00, 0x00, pack, (uint8_t *)sm4_decrypt_struct, sm4_crypt_struct_len);
    encrypt_data.len = ccm3310_transfer(pack, send_len, &encrypt_data.data, plen + RECV_PRE_LEN);
    rt_free((void *)pack);
    rt_free((void *)sm4_decrypt_struct);
    return encrypt_data;
}
