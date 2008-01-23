#ifndef _udp_trsp_h_
#define _udp_trsp_h_

#include "transport.h"

/**
 * Maximum message length for UDP
 * not including terminating '\0'
 */
#define MAX_UDP_MSGLEN 65535


class udp_trsp: public transport
{
    // socket descriptor
    int sd;

    // bound port number
    int _port;

 protected:
    /** @see AmThread */
    void run();
    /** @see AmThread */
    void on_stop();

 public:
    /** @see transport */
    udp_trsp(trans_layer* tl);
    ~udp_trsp();

    /** @see transport */
    int bind(const string& address, int port);

    /** @see transport */
    int send(const sockaddr_storage* sa, const char* msg, const int msg_len);

};

#endif
