/*
 * Copyright (C) 2020 Peter Sjödin, KTH
 *
 * This file is subject to the terms and conditions of the GNU Lesser
 * General Public License v2.1. See the file LICENSE in the top level
 * directory for more details.
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "shell.h"
#include "msg.h"
#include "net/emcute.h"
#include "net/ipv6/addr.h"
#include "xtimer.h"

#include "at24mac.h"

#include "mqttsn_publisher.h"
#include "report.h"

#ifndef EMCUTE_ID
#define EMCUTE_ID           ("sim7020-14b0")
#endif
#define EMCUTE_PORT         (1883U)
#define EMCUTE_PRIO         (THREAD_PRIORITY_MAIN - 1)

#define MQPUB_PRIO         (THREAD_PRIORITY_MAIN - 1)

#define NUMOFSUBS           (16U)
#define TOPIC_MAXLEN        (64U)

#define MQPUB_TOPIC_LENGTH  64

/* State machine interval in secs */
#define MQPUB_STATE_INTERVAL 2

typedef enum {
    MQTTSN_NOT_CONNECTED,
    MQTTSN_CONNECTED,
    MQTTSN_PUBLISHING,
} mqttsn_state_t;

typedef struct mqttsn_stats {
  uint16_t connect_ok;
  uint16_t register_ok;
  uint16_t publish_ok;
  uint16_t connect_fail;
  uint16_t register_fail;
  uint16_t publish_fail;
  uint16_t reset;
} mqttsn_stats_t;

mqttsn_stats_t mqttsn_stats;

static char mqpub_stack[THREAD_STACKSIZE_DEFAULT];
static char emcute_stack[THREAD_STACKSIZE_DEFAULT];

static char topicstr[MQPUB_TOPIC_LENGTH];
emcute_topic_t emcute_topic;

static void *emcute_thread(void *arg)
{
    (void)arg;
    emcute_run(EMCUTE_PORT, EMCUTE_ID);
    return NULL;    /* should never be reached */
}

int get_nodeid(char *buf, size_t size) {
    int n = 0;
    eui64_t e64;

    if (at24mac_get_eui64(0, &e64) != 0) {
        printf("Can't get nodeid\n");
        return 0;
    }
    for (unsigned i = 0; i < sizeof(e64.uint8); i++) {
      n += snprintf(buf + n, size - n, "%02x", e64.uint8[i]);
    }
    return n;
}

static void _init_topic(void) {
  
    //static char nodeid[sizeof(e64.uint8)*2+1];
    int n; 

    n = snprintf(topicstr, sizeof(topicstr), "%s/", MQTT_TOPIC_BASE);
    n += get_nodeid(topicstr + n, sizeof(topicstr) - n);
    n += snprintf(topicstr + n, sizeof(topicstr) - n, "/sensors");
    printf("_init topicstr is %s\n", topicstr);
}


uint8_t publish_buffer[MQTTSN_BUFFER_SIZE];

static int mqpub_pub(void) {
    unsigned flags = EMCUTE_QOS_1;
    size_t publen;
    int errno;
    
    publen = makereport(publish_buffer, sizeof(publish_buffer));
    printf("Publish %d: \"%s\"\n", publen, publish_buffer);
    if ((errno = emcute_pub(&emcute_topic, publish_buffer, publen, flags)) != EMCUTE_OK) {
        printf("error: unable to publish data to topic '%s [%i]' (error %d)\n",
               emcute_topic.name, (int)emcute_topic.id, errno);
        mqttsn_stats.publish_fail += 1;
        if (errno == EMCUTE_OVERFLOW)
             return 0;
        return errno;
    }
    mqttsn_stats.publish_ok += 1;
    return 0;
    
}
static int mqpub_con(void) {
    sock_udp_ep_t gw = { .family = AF_INET6, .port = MQTTSN_GATEWAY_PORT };
    int errno;

    /* parse address */
    if (ipv6_addr_from_str((ipv6_addr_t *)&gw.addr.ipv6, MQTTSN_GATEWAY_HOST) == NULL) {
         printf("Bad address %s\n", MQTTSN_GATEWAY_HOST);
        return 1;
    }
    if ((errno = emcute_con(&gw, true, NULL, NULL, 0, 0)) != EMCUTE_OK) {
         printf("error: unable to connect to gateway [%s]:%d (error %d)\n", MQTTSN_GATEWAY_HOST, MQTTSN_GATEWAY_PORT, errno);
        mqttsn_stats.connect_fail += 1;
        return 1;
    }
    printf("MQTT-SN: Connect to gateway [%s]:%d\n", MQTTSN_GATEWAY_HOST, MQTTSN_GATEWAY_PORT);
    mqttsn_stats.connect_ok += 1;
    return 0;
}

static int mqpub_reg(void) {
    int errno;
    emcute_topic.name = topicstr;
    if ((errno = emcute_reg(&emcute_topic)) != EMCUTE_OK) {
        mqttsn_stats.register_fail += 1;
        printf("error: unable to obtain topic ID for \"%s\" (error %d)\n", topicstr, errno);
        return 1;
    }
    printf("Register topic %d for \"%s\"\n", (int)emcute_topic.id, topicstr);
    mqttsn_stats.register_ok += 1;
    return 0;
}

static int mqpub_reset(void) {
    int errno = emcute_discon();
    mqttsn_stats.reset += 1;
    if (errno != EMCUTE_OK) {
        printf("MQTT-SN: disconnect failed %d\n", errno);
        return errno;
    }
    return 0;
}

static mqttsn_state_t state;

static void *mqpub_thread(void *arg)
{
    uint32_t sleepsecs;
    (void)arg;
    
 again:
    state = MQTTSN_NOT_CONNECTED;
    sleepsecs = MQPUB_STATE_INTERVAL;
    while (1) {
      switch (state) {
      case MQTTSN_NOT_CONNECTED:
        if (mqpub_con() == 0)
          state = MQTTSN_CONNECTED;
        break;
      case MQTTSN_CONNECTED:
        if (mqpub_reg() == 0) {
          state = MQTTSN_PUBLISHING;
        }
        else {
          mqpub_reset();
          state = MQTTSN_NOT_CONNECTED;
        }
        break;
      case MQTTSN_PUBLISHING:
        if (mqpub_pub() != 0) {
          mqpub_reset();
          state = MQTTSN_NOT_CONNECTED;
          goto again;
        }
        sleepsecs = MQTTSN_PUBLISH_INTERVAL;
        break;
      default:
        break;
      }
      xtimer_sleep(sleepsecs);
    }
    return NULL;    /* shouldn't happen */
}

void mqttsn_publisher_init(void) {

    _init_topic();

    /* start emcute thread */
    thread_create(emcute_stack, sizeof(emcute_stack), EMCUTE_PRIO, 0,
                  emcute_thread, NULL, "emcute");
    /* start publisher thread */
    thread_create(mqpub_stack, sizeof(mqpub_stack), MQPUB_PRIO, 0,
                  mqpub_thread, NULL, "mqttsn_publisher");
}

typedef enum {
    s_gateway, s_connect, s_register, s_publish, s_reset} mqttsn_report_state_t;

int mqttsn_report(uint8_t *buf, size_t len, uint8_t *finished) {
     char *s = (char *) buf;
     size_t l = len;
     static mqttsn_report_state_t state = s_gateway;
     int nread = 0;
     
     *finished = 0;
     
     switch (state) {
     case s_gateway:
          RECORD_START(s + nread, l - nread);
          PUTFMT(",{\"n\": \"mqtt_sn;gateway\",\"vs\":\"[%s]:%d\"}",MQTTSN_GATEWAY_HOST, MQTTSN_GATEWAY_PORT);
          RECORD_END(nread);
          state = s_connect;

     case s_connect:
          RECORD_START(s + nread, l - nread);
          PUTFMT(",{\"n\":\"mqtt_sn;stats;connect\",\"vj\":[");
          PUTFMT("{\"n\":\"ok\",\"u\":\"count\",\"v\":%d},", mqttsn_stats.connect_ok);
          PUTFMT("{\"n\":\"fail\",\"u\":\"count\",\"v\":%d}", mqttsn_stats.connect_fail);
          PUTFMT("]}");
          RECORD_END(nread);
          state = s_register;

     case s_register:
          RECORD_START(s + nread, l - nread);
          PUTFMT(",{\"n\":\"mqtt_sn;stats;register\",\"vj\":[");
          PUTFMT("{\"n\":\"ok\",\"u\":\"count\",\"v\":%d},", mqttsn_stats.register_ok);
          PUTFMT("{\"n\":\"fail\",\"u\":\"count\",\"v\":%d}", mqttsn_stats.register_fail);
          PUTFMT("]}");
          RECORD_END(nread);
          state = s_publish;
     
     case s_publish:
          RECORD_START(s + nread, l - nread);
          PUTFMT(",{\"n\":\"mqtt_sn;stats;publish\",\"vj\":[");
          PUTFMT("{\"n\":\"ok\",\"u\":\"count\",\"v\":%d},", mqttsn_stats.publish_ok);
          PUTFMT("{\"n\":\"fail\",\"u\":\"count\",\"v\":%d}", mqttsn_stats.publish_fail);
          PUTFMT("]}");
          RECORD_END(nread);
          state = s_reset;
          
     case s_reset:
          RECORD_START(s + nread, l - nread);
          PUTFMT(",{\"n\":\"mqtt_sn;stats;reset\",\"u\":\"count\",\"v\":%d}", mqttsn_stats.reset);
          RECORD_END(nread);

          state = s_gateway;
     }
     *finished = 1;

     return nread;
}

