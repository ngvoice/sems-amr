#ifndef _MyCtrlInterface_h_
#define _MyCtrlInterface_h_

#include "sip_ua.h"

#include <string>
using std::string;

class AmSipRequest;
class AmSipReply;

class trans_layer;
class trans_bucket;
struct sip_msg;

class MyCtrlInterface: public sip_ua
{
    static MyCtrlInterface* _instance;

    trans_layer* tl;

    MyCtrlInterface();
    ~MyCtrlInterface(){}

  public:

    static MyCtrlInterface* instance();

    /**
     * From AmCtrlInterface
     */
    //@param serKey An out parameter
    int send(const AmSipRequest &req, string &serKey);
    int send(const AmSipReply &rep);

//    string localURI(const string &displayName, 
// 		    const string &userName, const string &hostName, 
// 		    const string &uriParams, const string &hdrParams);
//    void registerInterfaceHandler(AmInterfaceHandler *handler);

    void handleSipMsg(AmSipRequest &req);
    void handleSipMsg(AmSipReply &rep);


    /**
     * From sip_ua
     */
    void handle_sip_request(const char* tid, sip_msg* msg);
    void handle_sip_reply(sip_msg* msg);
    
};


#endif
