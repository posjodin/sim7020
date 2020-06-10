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
#include <ctype.h>

#include "at.h"
#include "xtimer.h"
#include "periph/uart.h"

static at_dev_t at_dev;
static char buf[256];
static char resp[1024];

 mutex_t sim7020_lock = MUTEX_INIT;

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

#define SIM7020_RECVHEX
#ifdef SIM7020_RECVHEX
    /* Receive data as hex string */
    res = at_send_cmd_wait_ok(&at_dev, "AT+CSORCVFLAG=0", 5000000);
#else  
    /* Receive binary data */
    res = at_send_cmd_wait_ok(&at_dev, "AT+CSORCVFLAG=1", 5000000);
#endif /* SIM7020_RECVHEX */

    //Telia is 24001
    //res = at_send_cmd_wait_ok(&at_dev, "AT+COPS=1,2,\"24002\"", 5000000);

    /* Signal Quality Report */
    res = at_send_cmd_get_resp(&at_dev, "AT+CSQ", resp, sizeof(resp), 10*1000000);

    return res;
}
/* Operator MCCMNC (mobile country code and mobile network code) */
/* Telia */
//#define OPERATOR "24001"
//#define APN "lpwa.telia.iot"
/* 3  */
#define OPERATOR "24002"
#define APN "internet"

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

  //res =at_send_cmd_wait_ok(&at_dev, "AT+COPS=1,2,\"24001\"", 120*1000000);

  int count = 0;
  while (1) {

    if (count++ % 8 == 0) {
      res =at_send_cmd_wait_ok(&at_dev, "AT+COPS=1,2,\"" OPERATOR "\"", 120*1000000);
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

  return 1;
}

int sim7020_activate(void) {
  int res;
  uint8_t attempts = 3;
  
  res = at_send_cmd_get_resp(&at_dev,"AT+CSTT?", resp, sizeof(resp), 120*1000000);
  if (res > 0) {
    if (strncmp("+CSTT: \"\"", resp, sizeof("+CSTT: \"\"")-1) != 0)
      return 0;
  }
  /* Start Task and Set APN, USER NAME, PASSWORD */
  //res = at_send_cmd_get_resp(&at_dev,"AT+CSTT=\"lpwa.telia.iot\",\"\",\"\"", resp, sizeof(resp), 120*1000000);
  res = at_send_cmd_get_resp(&at_dev,"AT+CSTT=\"" APN "\",\"\",\"\"", resp, sizeof(resp), 120*1000000);  
  while (attempts--) {
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

  if (1) {
    printf("Searching for operators, be patient\n");
    res = at_send_cmd_get_resp(&at_dev, "AT+COPS=?", resp, sizeof(resp), 120*1000000);
  }
  res = at_send_cmd_get_resp(&at_dev, "AT+CREG?", resp, sizeof(resp), 120*1000000);
  /* Request International Mobile Subscriber Identity */
  res = at_send_cmd_get_resp(&at_dev, "AT+CIMI", resp, sizeof(resp), 10*1000000);

    /* Request TA Serial Number Identification (IMEI) */
  res = at_send_cmd_get_resp(&at_dev, "AT+GSN", resp, sizeof(resp), 10*1000000);

  /* Mode 0: Radio information for serving and neighbor cells */
  res = at_send_cmd_wait_ok(&at_dev,"AT+CENG=0", 60*1000000);
  /* Report Network State */
  res = at_send_cmd_get_resp(&at_dev,"AT+CENG?", resp, sizeof(resp), 60*1000000);

  /* Signal Quality Report */
  res = at_send_cmd_wait_ok(&at_dev,"AT+CSQ", 60*1000000);
  /* Task status, APN */
  res = at_send_cmd_get_resp(&at_dev,"AT+CSTT?", resp, sizeof(resp), 60*1000000);

  /* Get Local IP Address */
  res = at_send_cmd_get_resp(&at_dev,"AT+CIFSR", resp, sizeof(resp), 60*1000000);
  /* PDP Context Read Dynamic Parameters */
  res = at_send_cmd_get_resp(&at_dev,"AT+CGCONTRDP", resp, sizeof(resp), 60*1000000);
  return res;
}

int sim7020_udp_socket(void) {
  int res;
  /* Create a socket: IPv4, UDP, 1 */
  res = at_send_cmd_get_resp(&at_dev, "AT+CSOC=1,2,1", resp, sizeof(resp), 120*1000000);    
    if (res > 0) {
      uint8_t sockid;

      if (1 == (sscanf(resp, "+CSOC: %hhd", &sockid))) {
        return sockid;
      }
      else
        printf("Parse error: '%s'\n", resp);
    }
    else
      at_drain(&at_dev);
    return res;
}

int sim7020_close(uint8_t sockid) {

  int res;
  char cmd[64];


  sprintf(cmd, "AT+CSOCL=%d", sockid);

  res = at_send_cmd_wait_ok(&at_dev, cmd, 120*1000000);
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


#define AT_RADIO_MAX_SEND_LEN 128
int sim7020_send(uint8_t sockid, uint8_t *data, size_t datalen) {
  int res;

  size_t len = (datalen < AT_RADIO_MAX_SEND_LEN ? datalen : AT_RADIO_MAX_SEND_LEN);
  char cmd[32];


  mutex_lock(&sim7020_lock);
  at_drain(&at_dev);
  snprintf(cmd, sizeof(cmd), "AT+CSODSEND=%d,%d", sockid, len);
  res = at_send_cmd(&at_dev, cmd, 10*1000000);
  res = at_expect_bytes(&at_dev, "> ", 10*1000000);
  if (res != 0) {
    printf("No send prompt\n");
    goto out;
  }
  if (res == 0) {
    at_send_bytes(&at_dev, (char *) data, len);
    while (1) {
      unsigned int nsent;
      res = at_readline(&at_dev, resp, sizeof(resp), 0, 10*1000000);
      if (res < 0) {
        printf("Timeout waiting for DATA ACCEPT confirmation\n");
        goto out;
      }
      if (1 == (sscanf(resp, "DATA ACCEPT: %d", &nsent))) {
        res = nsent;
        goto out;
      }
    }
  }
  res = 0;
 out:
  mutex_unlock(&sim7020_lock);
  return res;
}

static uint8_t recv_buf[AT_RADIO_MAX_RECV_LEN];

static void _recv_cb(void *arg, const char *code) {
  (void) arg;
  int sockid, len;
  printf("recv_cb for code '%s'\n", code);
  int res = sscanf(code, "+CSONMI: %d,%d,", &sockid, &len);
  if (res == 2) {
    printf("GOt %d bytes on sockid %d\n", len, sockid);
    /* Data is encoded as hex string, so
     * data length is half the string length */ 

    int rcvlen = len >> 1;

    if (rcvlen >  AT_RADIO_MAX_RECV_LEN)
      return; /* Too large */
    /* Find first char after second comma */
    char *ptr = strchr(strchr(code, ',')+1, ',')+1;
    
    for (int i = 0; i < rcvlen; i++) {
      char hexstr[3];
      hexstr[0] = *ptr++; hexstr[1] = *ptr++; hexstr[2] = '\0';
      printf("hexstr %d: '%s' ", i, hexstr);
      recv_buf[i] = (uint8_t) strtoul(hexstr, NULL, 16);
      printf("-> %d\n", recv_buf[i]);
    }
    for (int i = 0; i < rcvlen; i++) {
      if (isprint(recv_buf[i]))
        putchar(recv_buf[i]);
      else
        printf("0x%02x", recv_buf[i]);
      putchar(' ');
    }
    putchar('\n');
  }
  else
    printf("recv_cb res %d\n", res);
}

void *sim7020_recv_thread(void *arg) {
  unsigned int runsecs = (unsigned int) arg;
  at_urc_t urc;

  (void) runsecs;
  urc.cb = _recv_cb;
  urc.code = "+CSONMI:";
  urc.arg = NULL;
  at_add_urc(&at_dev, &urc);
  while (1) {
    mutex_lock(&sim7020_lock);
    at_process_urc(&at_dev, 1000*(uint32_t) 1000);
    mutex_unlock(&sim7020_lock);
  }
}

int sim7020_test(uint8_t sockid, int count) {
  static char testbuf[] = "0123456789ABCDEFGHIJKLMNOPQRSTUVXYZ";
  
  while (count > 0) {
  for (unsigned int i = 1; i < sizeof(testbuf) && count > 0; i++) {
      if (sim7020_send(sockid, (uint8_t *) testbuf, i) < 0)
        return -1;
      xtimer_sleep(3);
      if (count > 0)
        count--;
    }
  }
  return 0;
}
