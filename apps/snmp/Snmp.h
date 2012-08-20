#ifndef _SNMP_h_
#define _SNMP_h_

#include "AmApi.h"
#include "AmThread.h"

#include <net-snmp/library/large_fd_set.h>

class SnmpFactory 
  : public AmPluginFactory,
    public AmThread
{
  int               wakeup_pipe[2];
  AmSharedVar<bool> agent_running;

  SnmpFactory(const string& name);

  void interrupt_snmp_agent();
  int check_and_process_snmp(netsnmp_large_fd_set* fdset);

protected:
  void run();
  void on_stop();

public:
  DECLARE_MODULE_INSTANCE(SnmpFactory);

  int onLoad();
  void onUnload();
};


#endif
