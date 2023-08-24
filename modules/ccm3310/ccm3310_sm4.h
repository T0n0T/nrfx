/**
 * @file ccm3310_sm4.h
 * @author your name (you@domain.com)
 * @brief
 * @version 0.1
 * @date 2023-08-24
 *
 * @copyright Copyright (c) 2023
 *
 */

#include <ccm3310_type.h>

extern uint8_t ccm3310_sm4_setkey(uint8_t *key);
extern ciphertext ccm3310_sm4_encrypt(uint8_t key_id, pdata p);
extern plaintext ccm3310_sm4_decrypt(uint8_t key_id, pdata p);
