/**
 * @file ccm3310_test.c
 * @author your name (you@domain.com)
 * @brief
 * @version 0.1
 * @date 2023-07-25
 *
 * @copyright Copyright (c) 2023
 *
 */

#include <rtthread.h>
#include <rtdevice.h>
#include <board.h>
#include <ccm3310.h>

static uint8_t sm4_id;
// char origin_text[] = "{\"test\":\"hello\",\"description\":\"here's a sm4 test.\"}";
char origin_text[] = "hello!here's a test,with utf-8";
ciphertext cipher_text;
plaintext plain_text;

uint8_t Get_Version[] = {
    0x53, 0x02, 0x10, 0x33,
    0x50, 0x00, 0x00, 0x00,
    0x80, 0x30, 0x00, 0x00,
    0x55, 0x55, 0x55, 0x55,
    0x55, 0x02, 0x33, 0x01};

uint8_t SM2_Gen_Key[] = {
    // 1.包头
    0x53, 0x02, 0x10, 0x33,
    // 2.数据区长度
    0x04, 0x00, 0x00, 0x00,
    // 3.0x5a生成sm2密钥，新增，保留在sram，掉电复位
    0x80, 0x5A, 0x00, 0x00,
    // 4.保留
    0x55, 0x55, 0x55, 0x55,
    // 5.数据区
    // 0x00结构体版本，0x00新增密钥不适用，0x00私钥可导出，保留
    0x00, 0x00, 0x00, 0x00,
    // 6.包尾
    0x55, 0x02, 0x33, 0x01};

uint8_t SM2_Export_Key[] = {
    // 1.包头
    0x53, 0x02, 0x10, 0x33,
    // 2.数据区长度
    0x04, 0x00, 0x00, 0x00,
    // 3.0x5e导出sm2密钥，
    0x80, 0x5E, 0x00, 0x00,
    // 4.保留
    0x55, 0x55, 0x55, 0x55,
    // 5.数据区
    // 0x00结构体版本，0x00密钥id，导出sram中密钥，0x01导出公钥，保留
    0x00, 0x00, 0x01, 0x00,
    // 6.包尾
    0x55, 0x02, 0x33, 0x01};

uint8_t Import_Sym_Key[] = {
    // 1.包头
    0x53, 0x02, 0x10, 0x33,
    // 2.数据区长度
    0x14, 0x00, 0x00, 0x00,
    // 3.0x42导入sm4会话密钥，0x00新增，0x00保留在sram，掉电复位
    0x80, 0x42, 0x00, 0x00,
    // 4.保留
    0x55, 0x55, 0x55, 0x55,
    // 5.数据区
    // 0x00结构体版本，0x00新增密钥不适用，0x84算法sm4，sm4密钥长度16=0x10
    0x00, 0x00, 0x84, 0x10,
    // 密钥
    0x77, 0x7f, 0x23, 0xc6,
    0xfe, 0x7b, 0x48, 0x73,
    0xdd, 0x59, 0x5c, 0xff,
    0xf6, 0x5f, 0x58, 0xec,
    // 6.包尾
    0x55, 0x02, 0x33, 0x01};

uint8_t Sym_Encrypt[] = {
    // 1.包头
    0x53, 0x02, 0x10, 0x33,
    // 2.数据区长度
    0x28, 0x00, 0x00, 0x00,
    // 3.0x44导入sm4加密，新增，保留在sram，掉电复位
    0x80, 0x44, 0x00, 0x00,
    // 4.保留
    0x55, 0x55, 0x55, 0x55,
    // 5.数据区
    // 5.1 0x00结构体版本，
    // 5.2 密钥id num.17，
    // 5.3 0x00算法ecb，
    // 5.4 sm1相关，
    // 5.5 des/aes相关，
    // 5.6 0x01分组长度128bit,
    // 5.7不使用初始向量，
    // 5.8保留，
    // (5.9无初始向量置空)
    0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00,
    // 5.10明文
    0x68, 0x65, 0x6c, 0x6c,
    0x6f, 0x21, 0x68, 0x65,
    0x72, 0x65, 0x27, 0x73,
    0x20, 0x61, 0x20, 0x74,
    0x65, 0x73, 0x74, 0x2c,
    0x77, 0x69, 0x74, 0x68,
    0x20, 0x75, 0x74, 0x66,
    0x2d, 0x38, 0x00, 0x00,
    // 6.包尾
    0x55, 0x02, 0x33, 0x01};

uint8_t Sym_Decrypt[] = {
    // 1.包头
    0x53, 0x02, 0x10, 0x33,
    // 2.数据区长度
    0x28, 0x00, 0x00, 0x00,
    // 3.0x44导入sm4加密，新增，保留在sram，掉电复位
    0x80, 0x46, 0x00, 0x00,
    // 4.保留
    0x55, 0x55, 0x55, 0x55,
    // 5.数据区
    // 5.1 0x00结构体版本，
    // 5.2 密钥id num.17，
    // 5.3 0x00算法ecb，
    // 5.4 sm1相关，
    // 5.5 des/aes相关，
    // 5.6 0x01分组长度128bit,
    // 5.7不使用初始向量，
    // 5.8保留，
    // (5.9无初始向量置空)
    0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00,
    // 5.10密文
    0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00,
    // 6.包尾
    0x55, 0x02, 0x33, 0x01};

static void ccm3310_version(void)
{
    int len          = 0;
    uint8_t *version = 0;

    len = ccm3310_transfer(Get_Version, sizeof(Get_Version), &version, 100);
    printf("\n========== print version ==========\n");
    // if (len >= 0) {
    //     printf("\n%s\n", version);
    // }
    if (len >= 0) {
        for (size_t i = 0; i < len; i++) {
            printf("%c", *(version + i));
        }
    }
    printf("\n===================================\n");
}

static void ccm3310_sm2_getkey_test(void)
{
    int len     = 0;
    uint8_t *id = 0, *sm2_key = 0;

    len = ccm3310_transfer(SM2_Gen_Key, sizeof(SM2_Gen_Key), &id, 21);
    printf("\n========= print key id ============\n");
    if (len >= 0) {
        printf("\n0x%X\n", *id);
        SM2_Export_Key[17] = *id;
    }

    printf("\n===================================\n");

    len = ccm3310_transfer(SM2_Export_Key, sizeof(SM2_Export_Key), &sm2_key, 100);
    printf("\n========= print public key ========\n");
    if (len >= 0) {
        for (size_t i = 0; i < len; i++) {
            printf("%c", *(sm2_key + i));
        }
    }
    printf("\n===================================\n");
}

static void ccm3310_sm4_setkey_test(void)
{
    uint8_t *sm4_pack = 0;
    uint8_t key[]     = {
        0x77, 0x7f, 0x23, 0xc6,
        0xfe, 0x7b, 0x48, 0x73,
        0xdd, 0x59, 0x5c, 0xff,
        0xf6, 0x5f, 0x58, 0xec};
    sm4_id = ccm3310_sm4_setkey(key);
    printf("\n========= print key id ============\n");
    printf("\n0x%X\n", sm4_id);
    printf("\n===================================\n");
}

static void ccm3310_sm4_encrypt_test(void)
{
    pdata origin_data;
    origin_data.data = (uint8_t *)origin_text;
    origin_data.len  = sizeof(origin_text);
    printf("sm4_id: 0x%02X\n", sm4_id);
    cipher_text = ccm3310_sm4_encrypt(sm4_id, origin_data);

    for (size_t i = 0; i < sizeof(origin_text); i++) {
        printf("%02x", origin_text[i]);
    }
    printf("\n========= print encrypt ===========\n");
    if (cipher_text.len >= 0) {
        for (size_t i = 0; i < cipher_text.len; i++) {
            printf("%02x", *(cipher_text.data + i));
        }
    }
    printf("\n===================================\n");
}

static void ccm3310_sm4_decrypt_test(void)
{
    printf("sm4_id: 0x%02X\n", sm4_id);
    plain_text = ccm3310_sm4_decrypt(sm4_id, cipher_text);
    printf("\n========= print decrypt ===========\n");
    if (plain_text.len >= 0) {
        for (size_t i = 0; i < plain_text.len; i++) {
            printf("%c", *(plain_text.data + i));
        }
    }
    printf("\n===================================\n");
}

MSH_CMD_EXPORT(ccm3310_version, get version);
MSH_CMD_EXPORT(ccm3310_sm2_getkey_test, get sm2key);
MSH_CMD_EXPORT(ccm3310_sm4_setkey_test, set sm4key);
MSH_CMD_EXPORT(ccm3310_sm4_encrypt_test, encode sm4);
MSH_CMD_EXPORT(ccm3310_sm4_decrypt_test, decode sm4);