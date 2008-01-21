#ifndef _transport_h_
#define _transport_h_

#include "AmThread.h"
#include <string>

using std::string;

class trans_layer;
struct sockaddr_storage;

class transport: public AmThread
{
 protected:
    /**
     * Transaction layer pointer.
     * This is used for received messages.
     */
    trans_layer* tl;

 public:
    transport(trans_layer* tl);

    virtual ~transport();
    
    /**
     * Binds the transport server to an address
     * @return -1 if error(s) occured.
     */
    virtual int bind(const string& address, int port)=0;

    /**
     * Sends a message.
     * @return -1 if error(s) occured.
     */
    virtual int send(const sockaddr_storage* sa, const char* msg, const int msg_len)=0;
};

#endif
