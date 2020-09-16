#ifndef SIM7020_H
#define SIM7020_H

#ifndef AT_RADIO_MAX_RECV_LEN
#define AT_RADIO_MAX_RECV_LEN 1024
#endif

#define SIM7020_MAX_SEND_LEN 768

#define SIM7020_MAX_SOCKETS 5

typedef void (* sim7020_recv_callback_t)(void *,
                            const uint8_t *data, uint16_t datalen);

int sim7020_init(uint8_t uart, uint32_t baudrate);
int sim7020_register(void);
int sim7020_activate(void);
int sim7020_status(void);
int sim7020_udp_socket(const sim7020_recv_callback_t recv_callback, void *recv_callback_arg);
int sim7020_close(uint8_t sockid);
int sim7020_connect(const uint8_t sockid, const sock_udp_ep_t *remote);
int sim7020_bind(const uint8_t sockid, const sock_udp_ep_t *remote);
int sim7020_send(uint8_t sockid, uint8_t *data, size_t datalen);
void *sim7020_recv_thread(void *arg);
int sim7020_test(uint8_t sockid, int count);
#endif /* SIM7020_H */
