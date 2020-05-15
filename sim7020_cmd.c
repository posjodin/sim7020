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

static char recvstack[THREAD_STACKSIZE_DEFAULT];

#define SIM7020_PRIO         (THREAD_PRIORITY_MAIN + 1)

int sim7020cmd_recv(int argc, char **argv) {
  unsigned int runsecs;
  
  if (argc < 2) {
    printf("Usage: %s runsecs\n", argv[0]);
    return 1;
  }
  runsecs = atoi(argv[1]);
  /* start the emcute thread */
  //kernel_pid_t thread_create(char *stack, int stacksize, uint8_t priority, int flags, thread_task_func_t function, void *arg, const char *name)

  thread_create(recvstack, sizeof(recvstack), SIM7020_PRIO, 0,
                sim7020_recv_thread, (void *) runsecs, "sim7020");
  printf("Receive thread started\n");
  return 0;
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

