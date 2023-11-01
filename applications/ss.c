// 777f23c6fe7b4873dd595cfff65f58ec

#include <stdio.h>
#include <stdint.h>
#include <string.h>

char* sm4keyHexString(void)
{
    uint8_t byteArray[16] = {0x77, 0x7f, 0x23, 0xc6, 0xfe, 0x7b, 0x48, 0x73, 0xdd, 0x59, 0x5c, 0xff, 0xf6, 0x5f, 0x58, 0xec};
    char    hexString[33];

    // 遍历数组并将每个字节转换为两个十六进制字符
    for (int i = 0; i < 16; i++) {
        snprintf(hexString + 2 * i, 3, "%02x", byteArray[i]);
    }
    hexString[32] = '\0';
    return hexString;
}

int main()
{
    // 打印转换后的字符串
    printf("%s\n", sm4keyHexString());

    return 0;
}

// int main()
// {
//     const char* hexString = "777f23c6fe7b4873dd595cfff65f58ec";
//     uint8_t     byteArray[16];

//     // 遍历字符串并将每两个字符解析为一个字节
//     for (int i = 0; i < 32; i += 2) {
//         char byteString[3] = {hexString[i], hexString[i + 1], '\0'};
//         byteArray[i / 2]   = (uint8_t)strtol(byteString, NULL, 16);
//     }

//     // 打印转换后的数组
//     for (int i = 0; i < 16; i++) {
//         printf("%02x ", byteArray[i]);
//     }

//     return 0;
// }
