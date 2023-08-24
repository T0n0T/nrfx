/**
 * @file ccm3310_type.h
 * @author your name (you@domain.com)
 * @brief
 * @version 0.1
 * @date 2023-08-24
 *
 * @copyright Copyright (c) 2023
 *
 */
#include <stdio.h>

typedef struct {
    int len;
    uint8_t *data;
} pdata;

typedef pdata plaintext;
typedef pdata ciphertext;
typedef struct {
    uint8_t version;
    uint8_t key_id;   /*新增写0x00*/
    uint8_t algo;     /*算法标识，0x81(DES)、0x82(AES)、 0x83(SM1)、0x84(SM4)*/
    uint8_t len;      /*密钥长度*/
    uint8_t key_data; /*密钥，使用时作为内存空间中密钥数组的*/
} ccm3310_key_data;

/* P1=00：已导入密钥 */
typedef struct {
    uint8_t version;
    uint8_t key_id;     /*导入密钥命令返回的密钥id*/
    uint8_t mode;       /*算法模式，0x00(ECB)、0x01(CBC)、0x02(CTR)、0x03(CFB)、0x04(OFB)*/
    uint8_t sk_sel;     /*sk导入方式，0x00(外部导入)，0x01(内部导入)，只有sm1使用，其他填0*/
    uint8_t keybitlen;  /*DES 模式下，0x00（64bit）、0x01（128bit）、0x02（192bit）；AES 模式下，0x00（128bit）、0x01（192bit）、0x02（256bit）；其他模式不使用，填 0。*/
    uint8_t feedbitlen; /*AES、SM1、SM4 的 CFB/OFB 模式下，0x00(1bit)、0x01（128bit）；其他模式不使用，填 0*/
    uint8_t lvlen;      /*起始加密向量长度*/
    uint8_t reserved;   /*保留*/
    uint8_t text;       /*包含两个字段，使用时作为内存空间中密钥数组的头*/
                        /*1. 16bits  lv;     起始向量，sm4 须为 16 bits，lv_len=0则没有此段*/
                        /*2. 16nbits data;   明文，sm4 须为 16n bits*/
} ccm3310_sym_crypt_data;