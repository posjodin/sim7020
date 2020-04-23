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

int sim7020cmd_udp_socket(int argc, char **argv) {
  
  (void) argc; (void) argv;

  int res = sim7020_udp_socket();
  if (res < 0)
    printf("Error %d\n", res);
  else
    printf("Socket %d");
  return res;
}

int sim7020cmd_connect(int argc, char **argv) {
  uint8_t sockid;
  char *ipaddr;
  uint16_t port;
  
  if (argc < 4) {
    printf("Usage: %s sockid ipaddr port\n", argv[0]);
    return 1;
  }
  sockid = atoi(argv[1]);
  ipaddr = argv[2];
  port = atoi(argv[3]);
  int res = sim7020_connect(sockid, ipaddr, port);
  if (res < 0)
    printf("Error %d\n", res);
  else
    printf("OK");
  return res;
}
