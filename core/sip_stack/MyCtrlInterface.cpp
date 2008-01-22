#include "MyCtrlInterface.h"
#include "../AmSipMsg.h"

#include "log.h"

MyCtrlInterface* MyCtrlInterface::_instance = NULL;


MyCtrlInterface* MyCtrlInterface::instance()
{
    if(!_instance)
	_instance = new MyCtrlInterface();

    return _instance;
}


int MyCtrlInterface::send(const AmSipRequest &req, string &serKey)
{
    
}

int MyCtrlInterface::send(const AmSipReply &rep)
{
    
}

#define DBG_PARAM(p)\
    DBG("%s = <%s>\n",#p,p.c_str());

void MyCtrlInterface::handleSipMsg(AmSipRequest &req)
{
    DBG("Received new request:\n");

    DBG_PARAM(req.cmd);
    DBG_PARAM(req.method);
    DBG_PARAM(req.user);
    DBG_PARAM(req.domain);
    DBG_PARAM(req.dstip);
    DBG_PARAM(req.port);
    DBG_PARAM(req.r_uri);
    DBG_PARAM(req.from_uri);
    DBG_PARAM(req.from);
    DBG_PARAM(req.to);
    DBG_PARAM(req.callid);
    DBG_PARAM(req.from_tag);
    DBG_PARAM(req.to_tag);
    DBG("cseq = <%i>\n",req.cseq);
    DBG_PARAM(req.serKey);
    DBG_PARAM(req.route);
    DBG_PARAM(req.next_hop);
    DBG("hdrs = <%s>\n",req.hdrs.c_str());
    DBG("body = <%s>\n",req.body.c_str());
}

void MyCtrlInterface::handleSipMsg(AmSipReply &rep)
{
    
}
