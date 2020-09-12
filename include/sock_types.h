/*
 * Copyright (C) 2020 KTH
 *
 * This file is subject to the terms and conditions of the GNU Lesser
 * General Public License v2.1. See the file LICENSE in the top level
 * directory for more details.
 */

#ifndef SOCK_TYPES_H
#define SOCK_TYPES_H

#include <stdatomic.h>

#include "mbox.h"
#include "mutex.h"
#include "net/af.h"
#include "net/ipv6/addr.h"
#include "errno.h"

/*
#include "uip.h"
#include "udp-socket.h"
*/

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief   Size for struct sock_udp::mbox_queue
 */
#ifndef SOCK_MBOX_SIZE
#define SOCK_MBOX_SIZE  (2)
#endif

/**
 * @brief @ref net_sock_udp definition for emb6
 */
struct sock_udp {
    uint8_t sim7020_socket_id;          /**< sim7020 socket id */
    uint8_t sim7020_connected;          /**< Connect command issued? */
  void (*recv_callback)(sock_udp_t *sock, const uint8_t *data, uint16_t datalen);
    mutex_t mutex;                      /**< mutex for the connection */
    mbox_t mbox;                        /**< mbox for receiving */
    msg_t mbox_queue[SOCK_MBOX_SIZE];   /**< queue for mbox */
    atomic_int receivers;               /**< current number of recv calls */
    struct {
        const ipv6_addr_t *src;         /**< source address */
        const void *data;               /**< data of received packet */
        size_t datalen;                 /**< length of received packet data */
        uint16_t src_port;              /**< source port */
    } recv_info;                        /**< info on received packet */
};

#ifdef __cplusplus
}
#endif

#endif /* SOCK_TYPES_H */
/** @} */
