#ifndef _trans_layer_h_
#define _trans_layer_h_

//#include "AmApi.h"

#include "cstring.h"

#include <list>
using std::list;

struct sip_msg;
struct sip_uri;
struct sip_trans;
struct sip_header;
struct sockaddr_storage;

class MyCtrlInterface;
class trans_bucket;
class udp_trsp;
class sip_ua;

class trans_layer
{
    /** 
     * Singleton pointer.
     * @see instance()
     */
    static trans_layer* _instance;

    sip_ua*   ua;
    udp_trsp* transport;
    
    
    /** Avoid external instantiation. @see instance(). */
    trans_layer();

    /** Avoid external instantiation. @see instance(). */
    ~trans_layer();

    /**
     * Implements the state changes for the UAC state machine
     * @return -1 if errors
     * @return transaction state if successfull
     */
    int update_uac_trans(trans_bucket* bucket, sip_trans* t, sip_msg* msg);

    /**
     * Implements the state changes for the UAS state machine
     */
    int update_uas_request(trans_bucket* bucket, sip_trans* t, sip_msg* msg);
    int update_uas_reply(trans_bucket* bucket, sip_trans* t, int reply_code);

    /**
     * Retransmits reply / ACK (if possible).
     */
    void retransmit(sip_trans* t);

    /**
     * Send ACK to error replies
     */
    void send_non_200_ack(sip_trans* t, sip_msg* reply);

    int set_next_hop(list<sip_header*>& route_hdrs, sip_uri& r_uri, 
		     sockaddr_storage* remote_ip);

 public:

    static trans_layer* instance();

    /**
     * Register a SIP UA.
     * This method MUST be called ONCE.
     */
    void register_ua(sip_ua* ua);

    /**
     * Register a transport instance.
     * This method MUST be called ONCE.
     */
    void register_transport(udp_trsp* trsp);

    /**
     * From Control Interface.
     */
    int send_reply(trans_bucket* bucket, sip_trans* t,
		   int reply_code, const cstring& reason,
		   const cstring& to_tag, const cstring& contact,
		   const cstring& hdrs, const cstring& body);

    int send_request(sip_msg* msg);

    /**
     * From Transport Layer
     */
    void received_msg(sip_msg* msg);

    void send_200_ack(sip_msg* reply);
};



#endif // _trans_layer_h_
