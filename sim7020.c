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
#include <errno.h>

#include "net/af.h"
#include "net/ipv4/addr.h"
#include "net/ipv6/addr.h"
#include "net/sock/udp.h"

#include "at.h"
#include "xtimer.h"
#include "periph/uart.h"

#include "sim7020.h"

#define SIM7020_RECVHEX

static at_dev_t at_dev;
static char buf[256];
static char resp[1024];

static struct at_radio_status {
    enum {
        AT_RADIO_STATE_NONE,
        AT_RADIO_STATE_IDLE,
        AT_RADIO_STATE_REGISTERED,
        AT_RADIO_STATE_ACTIVE
    } state;
} status = { .state = AT_RADIO_STATE_NONE}; 

struct sock_sim7020 {
    uint8_t sockid;
    sim7020_recv_callback_t recv_callback;
    void *recv_callback_arg;
};
typedef struct sock_sim7020 sim7020_socket_t;

static sim7020_socket_t sim7020_sockets[SIM7020_MAX_SOCKETS];

mutex_t sim7020_lock = MUTEX_INIT;

static char sim7020_stack[THREAD_STACKSIZE_DEFAULT];
#define SIM7020_PRIO         (THREAD_PRIORITY_MAIN + 1)
static kernel_pid_t sim7020_pid = KERNEL_PID_UNDEF;

static void *sim7020_thread(void *);

int sim7020_init(uint8_t uart, uint32_t baudrate) {

    int res = at_dev_init(&at_dev, UART_DEV(uart), baudrate, buf, sizeof(buf));

    if (res != UART_OK) {
        printf("Error initialising AT dev %d speed %d\n", uart, baudrate);
        return res;
    }
    if (pid_is_valid(sim7020_pid)) {
        printf("sim7020 thread already running\n");
    }
    else {
        sim7020_pid = thread_create(sim7020_stack, sizeof(sim7020_stack), SIM7020_PRIO, 0,
                                    sim7020_thread, NULL, "sim7020");
        if (!pid_is_valid(sim7020_pid)) {
            printf("Could not launch sim7020: %d\n", sim7020_pid);
            return sim7020_pid;
        }
    }
    return 0;
}

static int _module_init(void) {
    int res;
    res = at_send_cmd_wait_ok(&at_dev, "AT+RESET", 5000000);
    /* Ignore */
    res = at_send_cmd_wait_ok(&at_dev, "AT", 5000000);
    if (res < 0) {
        printf("AT fail\n");
        return res;
    }
    res = at_send_cmd_wait_ok(&at_dev, "AT+CPSMS=0", 5000000);
    if (res < 0)
        printf("CPSMS fail\n");      

    /* Limit bands to speed up roaming */
    /* WIP needs a generic solution */
    //res = at_send_cmd_wait_ok(&at_dev, "AT+CBAND=20", 5000000);

    /* Receive data as hex string */
    res = at_send_cmd_wait_ok(&at_dev, "AT+CSORCVFLAG=0", 5000000);

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

    uint8_t i;
    for (i = 0; i < SIM7020_MAX_SOCKETS; i++) {
        sim7020_close(i);
    }
    status.state = AT_RADIO_STATE_IDLE;
    return res;
}

/* Operator MCCMNC (mobile country code and mobile network code) */
/* Telia */
#define OPERATOR "24001"
#define APN "lpwa.telia.iot"
/* Tre  */
//#define OPERATOR "24002"
//#define APN "internet"

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
            res =at_send_cmd_wait_ok(&at_dev, "AT+COPS=1,2,\"" OPERATOR "\"", 240*1000000);
        }
      
        res = at_send_cmd_get_resp(&at_dev, "AT+CREG?", resp, sizeof(resp), 120*1000000);
        if (res > 0) {
            uint8_t creg;

            if (1 == (sscanf(resp, "%*[^:]: %*d,%hhd", &creg))) {
                /* Wait for 1 (Registered, home network) or 5 (Registered, roaming) */
                if (creg == 1 || creg == 5) {
                    status.state = AT_RADIO_STATE_REGISTERED;
                    break;
                }
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
        /* Already activated? */
        if (strncmp("+CSTT: \"\"", resp, sizeof("+CSTT: \"\"")-1) != 0) {
            status.state = AT_RADIO_STATE_ACTIVE;
            return 0;
        }
    }
    /* Start Task and Set APN, USER NAME, PASSWORD */
    //res = at_send_cmd_get_resp(&at_dev,"AT+CSTT=\"lpwa.telia.iot\",\"\",\"\"", resp, sizeof(resp), 120*1000000);
    res = at_send_cmd_get_resp(&at_dev,"AT+CSTT=\"" APN "\",\"\",\"\"", resp, sizeof(resp), 120*1000000);  
    while (attempts--) {
        /* Bring Up Wireless Connection with GPRS or CSD. This may take a while. */
        printf("Bringing up wireless, be patient\n");
        res = at_send_cmd_wait_ok(&at_dev, "AT+CIICR", 600*1000000);
        if (res == 0) {
            status.state = AT_RADIO_STATE_ACTIVE;
            printf("activated\n");
            break;
        }
        xtimer_sleep(8);
    }
    return res;
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

int sim7020_udp_socket(const sim7020_recv_callback_t recv_callback, void *recv_callback_arg) {
    int res;
    /* Create a socket: IPv4, UDP, 1 */
    mutex_lock(&sim7020_lock);
    res = at_send_cmd_get_resp(&at_dev, "AT+CSOC=1,2,1", resp, sizeof(resp), 120*1000000);    
    mutex_unlock(&sim7020_lock);

    if (res > 0) {
        uint8_t sockid;

        if (1 == (sscanf(resp, "+CSOC: %hhd", &sockid))) {
            assert(sockid < SIM7020_MAX_SOCKETS);
            sim7020_socket_t *sock = &sim7020_sockets[sockid];
            sock->recv_callback = recv_callback;
            sock->recv_callback_arg = recv_callback_arg;
            printf("CSOC = %d\n", sockid);
            return sockid;
        }
        else
            printf("Parse error: '%s'\n", resp);
    }
    else {
        printf("CSOC failed: %d\n", res);
        at_drain(&at_dev);
    }        
    return res;
}

int sim7020_close(uint8_t sockid) {

    int res;
    char cmd[64];

    assert(sockid < SIM7020_MAX_SOCKETS);
    sim7020_socket_t *sock = &sim7020_sockets[sockid];
    sock->recv_callback = NULL;

    sprintf(cmd, "AT+CSOCL=%d", sockid);

    res = at_send_cmd_wait_ok(&at_dev, cmd, 120*1000000);
    return res;
}


int sim7020_connect(uint8_t sockid, const sock_udp_ep_t *remote) {

    int res;
    char cmd[64];

    assert(sockid < SIM7020_MAX_SOCKETS);

    if (remote->family != AF_INET6) {
        return -EAFNOSUPPORT;
    }
    if (!ipv6_addr_is_ipv4_mapped((ipv6_addr_t *) &remote->addr.ipv6)) {
        printf("sim7020_connect: not ipv6 mapped ipv4: ");
        ipv6_addr_print((ipv6_addr_t *) &remote->addr.ipv6);
        printf("\n");
        return -1;
    }
    if (remote->port == 0) {
        return -EINVAL;
    }

    printf("Connectiing to ipv6 mapped ");
    ipv6_addr_print((ipv6_addr_t *) &remote->addr.ipv6);
    printf(":%d\n", remote->port);

    char *c = cmd;
    int len = sizeof(cmd);
    int n = snprintf(c, len, "AT+CSOCON=%d,%d,", sockid, remote->port);
    c += n; len -= n;
    ipv6_addr_t *v6addr = (ipv6_addr_t *) &remote->addr.ipv6;
    ipv4_addr_t *v4addr = (ipv4_addr_t *) &v6addr->u32[3];
    if (NULL == ipv4_addr_to_str(c, v4addr, len)) {
        printf("connect: bad IPv4 mapped address: ");
        ipv6_addr_print((ipv6_addr_t *) &remote->addr.ipv6);
        printf("\n");
        return -1;
    }

    /* Create a socket: IPv4, UDP, 1 */
    mutex_lock(&sim7020_lock);
    res = at_send_cmd_wait_ok(&at_dev, cmd, 120*1000000);
    mutex_unlock(&sim7020_lock);
    printf("socket connected: %d\n", res);
    return res;
}

int sim7020_send(uint8_t sockid, uint8_t *data, size_t datalen) {
    int res;

    char cmd[32];
    size_t len = datalen;
    if (len > SIM7020_MAX_SEND_LEN)
        return -EINVAL;

    assert(sockid < SIM7020_MAX_SOCKETS);
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

        if (sockid >= SIM7020_MAX_SOCKETS) {
            printf("Illegal sim socket %d\n", sockid);
            return;
        }
        sim7020_socket_t *sock = &sim7020_sockets[sockid];
        if (sock->recv_callback != NULL) {
            sock->recv_callback(sock->recv_callback_arg, recv_buf, rcvlen);
        }
        else {
            printf("sockid %d: no callback\n", sockid);
        }
    }
    else
        printf("recv_cb res %d\n", res);
}

static void _recv_loop(void) {
    at_urc_t urc;

    urc.cb = _recv_cb;
    urc.code = "+CSONMI:";
    urc.arg = NULL;
    at_add_urc(&at_dev, &urc);
    while (status.state == AT_RADIO_STATE_ACTIVE) {
        mutex_lock(&sim7020_lock);
        at_process_urc(&at_dev, 1000*(uint32_t) 1000);
        mutex_unlock(&sim7020_lock);
    }
    at_remove_urc(&at_dev, &urc);
}


static void *sim7020_thread(void *arg) {
    (void) arg;
again:
    switch (status.state) {
        
    case AT_RADIO_STATE_NONE:
        printf("***module init:\n");
        _module_init();
        goto again;
    case AT_RADIO_STATE_IDLE:
        printf("***register:\n");
        sim7020_register();
        goto again;
    case AT_RADIO_STATE_REGISTERED:
        printf("***activate:\n");
        sim7020_activate();
        goto again;
    case AT_RADIO_STATE_ACTIVE:
        printf("***recv loop:\n");
        _recv_loop();
        goto again;
    }
    return NULL;
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
