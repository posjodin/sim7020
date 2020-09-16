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

#include "msg.h"
#include "net/ipv6/addr.h"
#include "xtimer.h"

#include "mqttsn_publisher.h"
#include "report.h"

static int seq_nr_value = 0;

int rpl_report(uint8_t *buf, size_t len, uint8_t *finished);
int mqttsn_report(uint8_t *buf, size_t len, uint8_t *finished);
int boot_report(uint8_t *buf, size_t len, uint8_t *finished);

static size_t preamble(uint8_t *buf, size_t len) {
     char *s = (char *) buf;
     size_t l = len;
     size_t n;
     int nread = 0;
     
     RECORD_START(s + nread, l - nread);
     PUTFMT("{\"bn\":\"urn:dev:mac:");
     n = get_nodeid(RECORD_STR(), RECORD_LEN());
     RECORD_ADD(n);
     PUTFMT(";\"");
     PUTFMT(",\"bu\":\"count\",\"bt\":%lu}", (uint32_t) (xtimer_now_usec()/1000000));
     PUTFMT(",{\"n\":\"seq_no\",\"u\":\"count\",\"v\":%d}", 9000+seq_nr_value++);
     RECORD_END(nread);

     return (nread);
}

/*
 * Report scheduler -- return report generator function to use next
 */

typedef enum {s_rpl_report, s_mqttsn_report} report_state_t;

report_gen_t next_report_gen(void) {
     static unsigned int reportno = 0;
     static report_state_t state = s_rpl_report;

     reportno += 1;
     if (reportno == 1)
       return(boot_report);

     switch (state) {
     case s_rpl_report:
          //state = s_mqttsn_report;
          //return(rpl_report);
     case s_mqttsn_report:
          state = s_rpl_report;
          return(mqttsn_report);
     }
     return NULL;
}

/*
 * Reports -- build report by writing records to buffer 
 */
static size_t reports(uint8_t *buf, size_t len) {
     char *s = (char *) buf;
     size_t l = len;
     size_t nread = 0;
     static report_gen_t reportfun = NULL;
     static uint8_t finished;

     if (reportfun == NULL) {
          reportfun = next_report_gen();
     }
     do {
          int n = reportfun((uint8_t *) s + nread, l - nread, &finished);
          if (n == 0)
               return (nread);
          else
               nread += n;
     } while (!finished);
     reportfun = NULL;
     return (nread);
}

size_t makereport(uint8_t *buffer, size_t len) {
     char *s = (char *) buffer;
     size_t l = len;
     size_t n;
     int nread = 0;
     
     RECORD_START(s + nread, l - nread);
     PUTFMT("[");
     n = preamble((uint8_t *) RECORD_STR(), RECORD_LEN()-1); /* Save one for last bracket */
     RECORD_ADD(n);
     n = reports((uint8_t *) RECORD_STR(), RECORD_LEN()-1); /* Save one for last bracket */
     RECORD_ADD(n);
     PUTFMT("]");
     RECORD_END(nread);

     return (nread);
}

