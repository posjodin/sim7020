/*
 * Copyright (C) 2020 Peter Sjödin, KTH
 *
 * This file is subject to the terms and conditions of the GNU Lesser
 * General Public License v2.1. See the file LICENSE in the top level
 * directory for more details.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

#include "net/af.h"
#include "net/ipv4/addr.h"
#include "net/ipv6/addr.h"
#include "net/sock/udp.h"

#include "periph/uart.h"

#include "sim7020.h"

int sim7020cmd_init(int argc, char **argv) {
  
  (void) argc; (void) argv;

  int res = sim7020_init(UART_DEV(1), 9600);
  if (res < 0)
    printf("Error %d\n", res);
  else
    printf("OK");
  return res;
}

int sim7020cmd_register(int argc, char **argv) {
  
  (void) argc; (void) argv;

  int res = sim7020_register();
  if (res < 0)
    printf("Error %d\n", res);
  else
    printf("OK");
  return res;
}

int sim7020cmd_activate(int argc, char **argv) {
  
  (void) argc; (void) argv;

  int res = sim7020_activate();
  if (res < 0)
    printf("Error %d\n", res);
  else
    printf("OK");
  return res;
}
  
int sim7020cmd_status(int argc, char **argv) {
  
  (void) argc; (void) argv;

  int res = sim7020_status();
  if (res < 0)
    printf("Error %d\n", res);
  else
    printf("OK");
  return res;
}

static void recv_callback(void *p, const uint8_t *data, uint16_t datalen) {
    (void) p; (void) data;
    printf("recv_callback: got %d bytes\n", datalen);
}

int sim7020cmd_udp_socket(int argc, char **argv) {
  
(void) argc; (void) argv;

  int res = sim7020_udp_socket(recv_callback, NULL);
  if (res < 0)
    printf("Error %d\n", res);
  else
      printf("Socket %d", res);
  return res;
}

int sim7020cmd_close(int argc, char **argv) {
  uint8_t sockid;
  
  if (argc != 2) {
    printf("Usage: %s sockid\n", argv[0]);
    return 1;
  }
  sockid = atoi(argv[1]);
  int res = sim7020_close(sockid);
  if (res < 0)
    printf("Error %d\n", res);
  else
    printf("OK");
  return res;
}

int sim7020cmd_connect(int argc, char **argv) {
    uint8_t sockid;
    sock_udp_ep_t remote;

  
    if (argc < 4) {
        printf("Usage: %s sockid ipaddr port\n", argv[0]);
        return 1;
    }
    sockid = atoi(argv[1]);

    remote.family = AF_INET6;
    ipv6_addr_t *v6addr = (ipv6_addr_t *) remote.addr.ipv6;
    /* Build ipv4-mapped address */
    v6addr->u32[0].u32 = 0; v6addr->u32[1].u32 = 0;
    v6addr->u16[4].u16 = 0; v6addr->u16[5].u16 = 0xffff;
    if (NULL == ipv4_addr_from_str((ipv4_addr_t *) &v6addr->u32[3], argv[2])) {
        printf("Usage: %s sockid ipaddr port\n", argv[0]);
        return 1;
    }
    remote.netif = SOCK_ADDR_ANY_NETIF;
    remote.port = atoi(argv[3]);
    
    int res = sim7020_connect(sockid, &remote);
    if (res < 0)
        printf("Error %d\n", res);
    else
        printf("OK");
    return res;
}

int sim7020cmd_send(int argc, char **argv) {
  uint8_t sockid;
  char *data;
  
  if (argc < 3) {
    printf("Usage: %s sockid data\n", argv[0]);
    return 1;
  }
  sockid = atoi(argv[1]);
  data = argv[2];
  int res = sim7020_send(sockid, (uint8_t *) data, strlen(data));
  if (res < 0)
    printf("Error %d\n", res);
  else
    printf("OK");
  return res;
}

int sim7020cmd_test(int argc, char **argv) {
  uint8_t sockid;
  int count;
  
  if (argc < 2) {
    printf("Usage: %s sockid [count]\n", argv[0]);
    return 1;
  }
  sockid = atoi(argv[1]);
  if (argc == 3)
    count = atoi(argv[2]);
  else
    count = -1;
  int res = sim7020_test(sockid, count);
  if (res < 0)
    printf("Error %d\n", res);
  else
    printf("OK");
  return res;
}

