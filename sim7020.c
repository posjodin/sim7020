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

#include "at.h"
#include "xtimer.h"
#include "periph/uart.h"

static at_dev_t at_dev;
static char buf[256];
static char resp[1024];

int sim7020_init(uint8_t uart, uint32_t baudrate) {

    int res = at_dev_init(&at_dev, UART_DEV(uart), baudrate, buf, sizeof(buf));

    if (res != UART_OK) {
      printf("Error initialising AT dev %d speed %d\n", uart, baudrate);
      return 1;
    }

    res = at_send_cmd_wait_ok(&at_dev, "AT+RESET", 5000000);
    /* Ignore */
    res = at_send_cmd_wait_ok(&at_dev, "AT", 5000000);
    if (res < 0)
      printf("AT fail\n");
    res = at_send_cmd_wait_ok(&at_dev, "AT+CPSMS=0", 5000000);
    if (res < 0)
      printf("CPSMS fail\n");      

    /* Limit bands to speed up roaming */
    /* WIP needs a generic solution */
    //res = at_send_cmd_wait_ok(&at_dev, "AT+CBAND=20", 5000000);

    /* Receive data as hex string */
    res = at_send_cmd_wait_ok(&at_dev, "AT+CSORCVFLAG=0", 5000000);

    //Telia is 24001
    //res = at_send_cmd_wait_ok(&at_dev, "AT+COPS=1,2,\"24002\"", 5000000);

    /* Request International Mobile Subscriber Identity */
    res = at_send_cmd_get_resp(&at_dev, "AT+CIMI", resp, sizeof(resp), 10*1000000);

    /* Request TA Serial Number Identification (IMEI) */
    res = at_send_cmd_get_resp(&at_dev, "AT+GSN", resp, sizeof(resp), 10*1000000);
    return res;
}

int sim7020_register(void) {
  int res;

  while (0) {
    /* Request list of network operators */
    res = at_send_cmd_get_resp(&at_dev, "AT+COPS=?", resp, sizeof(resp), 120*1000000);
    if (res > 0) {
      if (strncmp(resp, "+COPS", strlen("+COPS")) == 0)
        break;
    }
    xtimer_sleep(5);
  }

  res =at_send_cmd_wait_ok(&at_dev, "AT+COPS=1,2,\"24001\"", 120*1000000);

  int count = 0;
  while (1) {

    if (++count % 20 == 0) {
      res = at_send_cmd_get_resp(&at_dev, "AT+COPS=?", resp, sizeof(resp), 120*1000000);
      res =at_send_cmd_wait_ok(&at_dev, "AT+COPS=1,2,\"24001\"", 5*1000000);
    }
      
    res = at_send_cmd_get_resp(&at_dev, "AT+CREG?", resp, sizeof(resp), 120*1000000);
    if (res > 0) {
      uint8_t creg;

      if (1 == (sscanf(resp, "%*[^:]: %*d,%hhd", &creg))) {
        /* Wait for 1 (Registered, home network) or 5 (Registered, roaming) */
        if (creg == 1 || creg == 5)
          break;
      }
    }
    xtimer_sleep(5);

  }
  /* Report Network State */

  res = at_send_cmd_get_resp(&at_dev,"AT+CSTT?", resp, sizeof(resp), 120*1000000);
res = at_send_cmd_get_resp(&at_dev,"AT+CSTT=\"lpwa.telia.iot\",\"\",\"\"\r", resp, sizeof(resp), 120*1000000); /* Start task and set APN */
  res = at_send_cmd_get_resp(&at_dev,"AT+CSTT?", resp, sizeof(resp), 120*1000000);
  //"lpwa.telia.iot"

  return 1;
}

int sim7020_activate(void) {
  int res;
  uint8_t attempts = 8;
  
  res = at_send_cmd_get_resp(&at_dev,"AT+CSTT?", resp, sizeof(resp), 120*1000000);
  /* Start Task and Set APN, USER NAME, PASSWORD */
  res = at_send_cmd_get_resp(&at_dev,"AT+CSTT=\"lpwa.telia.iot\",\"\",\"\"\r", resp, sizeof(resp), 120*1000000);
  /* Start task and set APN */
  res = at_send_cmd_get_resp(&at_dev,"AT+CSTT?", resp, sizeof(resp), 120*1000000);
  //"lpwa.telia.iot"
  
  while (0 && attempts--) {
    /* Bring Up Wireless Connection with GPRS or CSD */
    res = at_send_cmd_wait_ok(&at_dev, "AT+CIICR", 600*1000000);
    if (res == 0) {
      printf("activated\n");
      break;
    }
    xtimer_sleep(8);
  }
  return res == 0;
}

int sim7020_status(void) {
  int res;
  res = at_send_cmd_get_resp(&at_dev, "AT+CREG?", resp, sizeof(resp), 120*1000000);
  /* Request International Mobile Subscriber Identity */
  res = at_send_cmd_get_resp(&at_dev, "AT+CIMI", resp, sizeof(resp), 10*1000000);

    /* Request TA Serial Number Identification (IMEI) */
  res = at_send_cmd_get_resp(&at_dev, "AT+GSN", resp, sizeof(resp), 10*1000000);

  /* Mode 0: Radio information for serving and neighbor cells */
  res = at_send_cmd_wait_ok(&at_dev,"AT+CENG=0", 60*1000000);
  /* Report Network State */
  res = at_send_cmd_get_resp(&at_dev,"AT+CENG?", resp, sizeof(resp), 60*1000000);

  /* Task status, APN */
  res = at_send_cmd_get_resp(&at_dev,"AT+CSTT?", resp, sizeof(resp), 60*1000000);

  /* Get Local IP Address */
  res = at_send_cmd_get_resp(&at_dev,"AT+CIFSR", resp, sizeof(resp), 60*1000000);
  return res;
}

int sim7020_udp_socket(void) {
  int res;
  /* Create a socket: IPv4, UDP, 1 */
    res = at_send_cmd_get_resp(&at_dev, "AT+CSOC=1,2,1", resp, sizeof(resp), 120*1000000);
    if (res > 0) {
      uint8_t sockid;

      if (1 == (sscanf(resp, "+CSOC: %hhd", &sockid))) {
        printf("Socket no %d\n", sockid);
        return sockid;
      }
      else
        printf("Parse error: '%s'\n", resp);
    }
    return res;
}

int sim7020_connect(uint8_t sockid, char *ipaddr, uint16_t port) {

  int res;
  char cmd[64];

  sprintf(cmd, "AT+CSOCON=%d,%d,%s",
          sockid, port, ipaddr);

  /* Create a socket: IPv4, UDP, 1 */
  res = at_send_cmd_get_resp(&at_dev, cmd, resp, sizeof(resp), 120*1000000);
  return res;
}


int sim7020_send(uint8_t sockid, char *data, size_t datalen);
