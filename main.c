/*
 * Copyright (C) 2020 Peter Sjödin, KTH
 *
 * This file is subject to the terms and conditions of the GNU Lesser
 * General Public License v2.1. See the file LICENSE in the top level
 * directory for more details.
 */

/**
 * @ingroup tests
 * @{
 *
 * @file
 * @brief    AT module test application
 *
 * @author   Vincent Dupont <vincent@otakeys.com>
 * @author   Alexandre Abadie <alexandre.abadie@inria.fr>
 *
 * @}
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "at.h"
#include "shell.h"
#include "timex.h"

#include "periph/uart.h"

#include "mqttsn_publisher.h"

int run_mqttsn(int argc, char **argv)
{
    if (argc != 1) {
        printf("Usage: %s\n", argv[0]);
        return 1;
    }
    mqttsn_publisher_init();
    return 0;
}

#define SIM7020
#ifdef SIM7020
int sim7020cmd_init(int argc, char **argv);
int sim7020cmd_register(int argc, char **argv);
int sim7020cmd_activate(int argc, char **argv);
int sim7020cmd_status(int argc, char **argv);
int sim7020cmd_udp_socket(int argc, char **argv);
int sim7020cmd_close(int argc, char **argv);
int sim7020cmd_connect(int argc, char **argv);
int sim7020cmd_send(int argc, char **argv);
int sim7020cmd_test(int argc, char **argv);
int sim7020cmd_recv(int arg, char **argv);
#endif /* SIM7020 */
static const shell_command_t shell_commands[] = {
#ifdef SIM7020
    { "init", "Init SIM7020", sim7020cmd_init },
    { "register", "Register SIM7020", sim7020cmd_register },
    { "reg", "Register SIM7020", sim7020cmd_register },
    { "act", "Activate SIM7020", sim7020cmd_activate },    
    { "status", "Report SIM7020 status", sim7020cmd_status },
    { "usock", "Create SIM7020 UDP socket", sim7020cmd_udp_socket },        
    { "ucon", "Connect SIM7020 socket", sim7020cmd_connect },
    { "usend", "Send on SIM7020 socket", sim7020cmd_send },
    { "uclose", "Close SIM7020 socket", sim7020cmd_close },
    { "utest", "repeat usend", sim7020cmd_test },                
    { "mqttsn", "Run MQTT-SN client", run_mqttsn },                
#endif /* SIM7020 */

    { NULL, NULL, NULL },
};

extern int sim7020_active(void);

int main(void)
{
    puts("AT command test app");

    /* run the shell */
    char line_buf[SHELL_DEFAULT_BUFSIZE];


    shell_run(shell_commands, line_buf, SHELL_DEFAULT_BUFSIZE);

    return 0;
}
