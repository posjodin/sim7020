#ifndef SIM7020_H
#define SIM7020_H

#ifndef AT_RADIO_MAX_RECV_LEN
#define AT_RADIO_MAX_RECV_LEN 1024
#endif
int sim7020_init(uint8_t uart, uint32_t baudrate);
int sim7020_register(void);
int sim7020_activate(void);
int sim7020_status(void);
int sim7020_udp_socket(void);
int sim7020_close(uint8_t sockid);
int sim7020_connect(uint8_t sockid, char *ipaddr, uint16_t port);
int sim7020_send(uint8_t sockid, uint8_t *data, size_t datalen);
void *sim7020_recv_thread(void *arg);
int sim7020_test(uint8_t sockid, int count);
#endif /* SIM7020_H */
