#ifndef _trans_layer_h_
#define _trans_layer_h_

#include "AmApi.h"

struct sip_msg;
//class AmCtrlInterface;

class trans_layer
{
    /** 
     * Singleton pointer.
     * @see instance()
     */
    static trans_layer* _instance;

    //AmCtrlInterface* ctrl;
    
    
    /** Avoid external instantiation. @see instance(). */
    trans_layer();

    /** Avoid external instantiation. @see instance(). */
    ~trans_layer();

    int update_uac_trans(sip_trans* t, sip_msg* msg);
    int update_uas_trans(sip_trans* t, sip_msg* msg);

 public:

    static trans_layer* instance();


    /**
     * From Control Interface.
     */
    void send_msg(sip_msg* msg);

    /**
     * From Transport Layer
     */
    void received_msg(sip_msg* msg);
};



#endif // _trans_layer_h_
