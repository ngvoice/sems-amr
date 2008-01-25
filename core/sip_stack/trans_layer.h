#ifndef _trans_layer_h_
#define _trans_layer_h_

//#include "AmApi.h"

#include "cstring.h"

struct sip_msg;
struct sip_trans;

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
};



#endif // _trans_layer_h_
