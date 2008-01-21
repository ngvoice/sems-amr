#include "MyCtrlInterface.h"


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

void MyCtrlInterface::handleSipMsg(AmSipRequest &req)
{
    
}

void MyCtrlInterface::handleSipMsg(AmSipReply &rep)
{
    
}
