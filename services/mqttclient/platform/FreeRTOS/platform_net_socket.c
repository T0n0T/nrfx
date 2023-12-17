/*
 * @Author: jiejie
 * @Github: https://github.com/jiejieTop
 * @Date: 2020-01-10 23:45:59
 * @LastEditTime: 2020-04-25 17:50:58
 * @Description: the code belongs to jiejie, please keep the author information and source code according to the license.
 */
#include "platform_net_socket.h"

int platform_net_socket_connect(const char* host, const char* port, int proto)
{
    int fd, flag, ret = MQTT_SUCCESS_ERROR;
    flag = proto == PLATFORM_NET_PROTO_TCP ? EC800M_SOCKET_TCP_CLIENT : EC800M_SOCKET_UDP_CLIENT;
    fd   = ec800m_socket_open(host, port, flag);
    if (fd < 0) {
        ret = MQTT_SOCKET_FAILED_ERROR;
    } else {
        ret = fd;
    }
    return ret;
}

int platform_net_socket_recv_timeout(int fd, unsigned char* buf, int len, int timeout)
{
    int            nread;
    int            nleft = len;
    unsigned char* ptr;
    ptr = buf;

    while (nleft > 0) {
        nread = ec800m_socket_read_with_timeout(fd, ptr, nleft, timeout);
        if (nread < 0) {
            return -1;
        } else if (nread == 0) {
            break;
        }

        nleft -= nread;
        ptr += nread;
    }
    return len - nleft;
}

int platform_net_socket_write_timeout(int fd, unsigned char* buf, int len, int timeout)
{
    int            nwrite;
    int            nleft = len;
    unsigned char* ptr;
    ptr = buf;

    while (nleft > 0) {
        nwrite = ec800m_socket_write_with_timeout(fd, ptr, nleft, timeout);
        if (nwrite < 0) {
            return -1;
        } else if (nwrite == 0) {
            break;
        }

        nleft -= nwrite;
        ptr += nwrite;
    }
    return len - nleft;
}

int platform_net_socket_close(int fd)
{
    return ec800m_socket_close(fd);
}

int platform_net_socket_set_block(int fd)
{
}

int platform_net_socket_set_nonblock(int fd)
{
}
