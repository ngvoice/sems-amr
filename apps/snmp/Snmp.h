#ifndef _SNMP_h_
#define _SNMP_h_

#include "AmApi.h"
#include "AmThread.h"

class SnmpFactory 
  : public AmPluginFactory,
    public AmThread
{
  AmSharedVar<bool> agent_running;

  SnmpFactory(const string& name);

protected:
  void run();
  void on_stop();

public:
  DECLARE_MODULE_INSTANCE(SnmpFactory);

  int onLoad();
  void onUnload();
};


#endif
