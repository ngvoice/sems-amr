#ifndef _MyCtrlInterface_h_
#define _MyCtrlInterface_h_

#include <string>
using std::string;

class AmSipRequest;
class AmSipReply;

class MyCtrlInterface
{
    static MyCtrlInterface* _instance;

    MyCtrlInterface(){}
    ~MyCtrlInterface(){}

  public:

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


    static MyCtrlInterface* instance();
};


#endif
