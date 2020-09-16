/*
 * Copyright (C) Peter Sjödin, KTH
 *
 * This file is subject to the terms and conditions of the GNU Lesser
 * General Public License v2.1. See the file LICENSE in the top level
 * directory for more details.
 */

#include <assert.h>
#include <errno.h>
#include <stdbool.h>

#include "net/ipv4/addr.h"
#include "net/ipv6/addr.h"
#include "net/sock/udp.h"
#include "timex.h"

#include "byteorder.h"
#include "msg.h"
#include "mutex.h"
#include "net/af.h"
#include "net/sock/udp.h"
#include "net/ipv6/hdr.h"
#include "sched.h"
#include "xtimer.h"

#include "sim7020.h"

#define SIM7020_MAX_SOCKETS 5

#define _MSG_TYPE_CLOSE     (0x4123)
#define _MSG_TYPE_TIMEOUT   (0x4124)
#define _MSG_TYPE_RCV       (0x4125)

sock_udp_t *sim7020_udp_sockets[SIM7020_MAX_SOCKETS];

static void _timeout_callback(void *arg);
static void _input_callback(void *p,
                            const uint8_t *data, uint16_t datalen);

typedef void (* udp_socket_input_callback_t)(sock_udp_t *c,
                            const uint8_t *data, uint16_t datalen);

static void _ep_print(const sock_udp_ep_t *ep) {
    if (ep == NULL) {
        printf("<none>");
    }
    else {
        ipv6_addr_print((ipv6_addr_t *) &ep->addr.ipv6);
        printf(":%d", ep->port);
    }
}
                      
static int _reg(sock_udp_t *sock,
                const sock_udp_ep_t *local, const sock_udp_ep_t *remote)
{
    if (((local != NULL) && (local->family != AF_INET6)) ||
        ((remote != NULL) && (remote->family != AF_INET6))) {
        return -EAFNOSUPPORT;
    }
    int res = sim7020_udp_socket(_input_callback, sock);
    if (res < 0) {
        printf("_reg failed %d\n", res);
        return res;
    }
    sock->sim7020_socket_id = res;

    if (local != NULL) {
        if ((res = sim7020_bind(sock->sim7020_socket_id, local)) < 0) {
            sock_udp_close(sock);
            return res;
        }
    }
    if (remote != NULL) {
        if ((res = sim7020_connect(sock->sim7020_socket_id, remote)) < 0) {
            sock_udp_close(sock);
            return res;
        }
    }
    return 0;
}


int sock_udp_create(sock_udp_t *sock, const sock_udp_ep_t *local,
                    const sock_udp_ep_t *remote, uint16_t flags)
{
    printf("sock_udp_create 0x%x: ", sock); _ep_print(local); printf(" -> "); _ep_print(remote); printf("\n");
    (void)flags;
    assert((sock != NULL));
    assert((remote == NULL) || (remote->port != 0));
    mutex_init(&sock->mutex);
    mutex_lock(&sock->mutex);
    mbox_init(&sock->mbox, sock->mbox_queue, SOCK_MBOX_SIZE);
    
    atomic_init(&sock->receivers, 0);
    int res = _reg(sock, local, remote);
    mutex_unlock(&sock->mutex);
    return res;
}

void sock_udp_close(sock_udp_t *sock)
{
    assert(sock != NULL);
    (void) sim7020_close(sock->sim7020_socket_id);
}

int sock_udp_get_local(sock_udp_t *sock, sock_udp_ep_t *ep)
{
    assert((sock != NULL) && (ep != NULL));
    (void) sock; (void) ep;
#if 0
    if ((sock->sock.input_callback != NULL) &&
        (sock->sock.udp_conn->lport != 0)) {
        mutex_lock(&sock->mutex);
        /* local UDP endpoints do not have addresses in emb6 */
        memset(&ep->addr, 0, sizeof(ipv6_addr_t));
        ep->port = ntohs(sock->sock.udp_conn->lport);
        mutex_unlock(&sock->mutex);
        return sizeof(ipv6_addr_t);
    }
#endif
    return -EADDRNOTAVAIL;
}

int sock_udp_get_remote(sock_udp_t *sock, sock_udp_ep_t *ep)
{
    assert((sock != NULL) && (ep != NULL));
    (void) sock; (void) ep;
#if 0
    if ((sock->sock.input_callback != NULL) &&
        (sock->sock.udp_conn->rport != 0)) {
        mutex_lock(&sock->mutex);
        memcpy(&ep->addr, &sock->sock.udp_conn->ripaddr, sizeof(ipv6_addr_t));
        ep->port = ntohs(sock->sock.udp_conn->rport);
        mutex_unlock(&sock->mutex);
        return sizeof(ipv6_addr_t);
    }
#endif
    return -ENOTCONN;
}

int sock_udp_recv(sock_udp_t *sock, void *data, size_t max_len,
                  uint32_t timeout, sock_udp_ep_t *remote)
{
    xtimer_t timeout_timer;
    int blocking = BLOCKING;
    int res = -EIO;
    msg_t msg;

    assert((sock != NULL) && (data != NULL) && (max_len > 0));
    if (0 && sock->recv_callback == NULL) {
        return -EADDRNOTAVAIL;
    }
    if (timeout == 0) {
        blocking = NON_BLOCKING;
    }
    else if (timeout != SOCK_NO_TIMEOUT) {
        timeout_timer.callback = _timeout_callback;
        timeout_timer.arg = &sock->mbox;
        xtimer_set(&timeout_timer, timeout);
    }

    atomic_fetch_add(&sock->receivers, 1);
    if (_mbox_get(&sock->mbox, &msg, blocking) == 0) {
        /* do not need to remove xtimer, since we only get here in non-blocking
         * mode (timeout > 0) */
        return -EAGAIN;
    }
    switch (msg.type) {
    case _MSG_TYPE_CLOSE:
        res = -EADDRNOTAVAIL;
        break;
    case _MSG_TYPE_TIMEOUT:
        res = -ETIMEDOUT;
        break;
    case _MSG_TYPE_RCV:
        mutex_lock(&sock->mutex);
        if (max_len < sock->recv_info.datalen) {
            res = -ENOBUFS;
            mutex_unlock(&sock->mutex);
            break;
        }
        memcpy(data, sock->recv_info.data, sock->recv_info.datalen);
        if (remote != NULL) {
            remote->family = AF_INET6;
            remote->netif = SOCK_ADDR_ANY_NETIF;
            memcpy(&remote->addr, sock->recv_info.src, sizeof(ipv6_addr_t));
            remote->port = sock->recv_info.src_port;
        }
        res = (int)sock->recv_info.datalen;
        mutex_unlock(&sock->mutex);
        break;
    default:
        printf("Bad mst type %d\n", msg.type);
    }
    atomic_fetch_sub(&sock->receivers, 1);
    printf("sock_udp_recv -> %d\n", res);
    return res;
}

int sock_udp_send(sock_udp_t *sock, const void *data, size_t len,
                  const sock_udp_ep_t *remote)
{
    assert((sock != NULL) || (remote != NULL));
    assert((len == 0) || (data != NULL));   /* (len != 0) => (data != NULL) */

    int sockid = sock->sim7020_socket_id;
    if (remote != NULL) {
        sim7020_connect(sockid, remote);
    }
    int res = sim7020_send(sockid, (uint8_t *) data, len);
    return res;
}

static void _timeout_callback(void *arg)
{
    msg_t msg = { .type = _MSG_TYPE_TIMEOUT };
    mbox_t *mbox = arg;

    /* should be safe, because otherwise if mbox were filled this callback is
     * senseless */
    mbox_try_put(mbox, &msg);
}

static void _input_callback(void *p,
                            const uint8_t *data, uint16_t datalen)
{
    msg_t msg = { .type = _MSG_TYPE_RCV };
    sock_udp_t *sock = (sock_udp_t *) p;
    
    mutex_lock(&sock->mutex);
#if 0
    sock->recv_info.src_port = src_port;
    sock->recv_info.src = (const ipv6_addr_t *)src_addr;
#endif
    sock->recv_info.data = data;
    sock->recv_info.datalen = datalen;
    mutex_unlock(&sock->mutex);
    mbox_put(&sock->mbox, &msg);
}

/** @} */
