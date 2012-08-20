#include "Snmp.h"

#include <net-snmp/net-snmp-config.h>
#include <net-snmp/net-snmp-includes.h>
#include <net-snmp/library/large_fd_set.h>
#include <net-snmp/agent/net-snmp-agent-includes.h>
#include "semsStats.h"
#include "log.h"

#define MOD_NAME "snmp"

EXPORT_MODULE_FUNC(SnmpFactory);
DEFINE_MODULE_INSTANCE(SnmpFactory, MOD_NAME)

SnmpFactory::SnmpFactory(const string& name)
  : AmPluginFactory(name)
{
  bzero(wakeup_pipe,sizeof(int)*2);
}

void SnmpFactory::interrupt_snmp_agent()
{
  unsigned char faked_byte=0;
  if(write(wakeup_pipe[1],&faked_byte,1) != 1) {
    ERROR("while writing to wakeup pipe: %s\n",
	  strerror(errno));
  }
}

int snmp_custom_log_fct(int majorID, int minorID, 
			void* serverarg, void* clientarg)
{
  struct snmp_log_message* slm = (struct snmp_log_message*)serverarg;
  if(!slm) return 0;

  int prio = slm->priority - LOG_ERR/*snmp def*/;
  if(prio < 0) prio = 0;

  _LOG(prio,"snmp: %s",slm->msg);

  return 1;
}

void fake_wakeup_func(int fd)
{
  unsigned char fake_buf;
  read(fd,&fake_buf,1);
}

int SnmpFactory::check_and_process_snmp(netsnmp_large_fd_set* fdset)
{
    struct timeval timeout = { LONG_MAX, 0 }, *tvp = &timeout;
    int            numfds;
    int            count;
    int            fakeblock = 0;

    NETSNMP_LARGE_FD_ZERO(fdset);

    numfds = wakeup_pipe[0]+1;
    NETSNMP_LARGE_FD_SET(wakeup_pipe[0],fdset);

    snmp_select_info2(&numfds, fdset, tvp, &fakeblock);
    if (fakeblock != 0) {
        /*
         * There are no alarms registered, so
         * let select() block forever.  
         */

        tvp = NULL;
    }

    count = select(numfds, fdset->lfs_setptr, 0, 0, tvp);
    if (count > 0) {
        /*
         * packets found, process them 
         */
        snmp_read2(fdset);
	if(NETSNMP_LARGE_FD_ISSET(wakeup_pipe[0],fdset)) {
	  fake_wakeup_func(wakeup_pipe[0]);
	}
    } else
        switch (count) {
        case 0:
            snmp_timeout();
            break;
        case -1:
            if (errno != EINTR) {
                snmp_log_perror("select");
            }
            return -1;
        default:
            snmp_log(LOG_ERR, "select returned %d\n", count);
            return -1;
        }                       /* endif -- count>0 */

    /*
     * Run requested alarms.  
     */
    run_alarms();

    return count;
}

void SnmpFactory::run()
{
  netsnmp_large_fd_set fdset;

  // Start SNMP Agent
  agent_running.set(true);

  // register our own log handler
  snmp_register_callback(SNMP_CALLBACK_LIBRARY,SNMP_CALLBACK_LOGGING,
			 snmp_custom_log_fct,NULL);
  snmp_enable_calllog();

  /* make us a agentx client. */
  netsnmp_ds_set_boolean(NETSNMP_DS_APPLICATION_ID, NETSNMP_DS_AGENT_ROLE, 1);

  /* initialize tcpip, if necessary */
  SOCK_STARTUP;

  /* initialize the agent library */
  init_agent("sems");

  snmp_log(LOG_INFO,"SEMS SNMP-Agent initialized.\n");

  /* initialize mib code here */
  init_semsStats();

  /* example-demon will be used to read example-demon.conf files. */
  init_snmp("sems");
  netsnmp_large_fd_set_init(&fdset, FD_SETSIZE);

  snmp_log(LOG_INFO,"SEMS SNMP-Agent is up and running.\n");

  /* your main loop here... */
  while(agent_running.get()) {
    check_and_process_snmp(&fdset);
  }

  /* at shutdown time */
  snmp_shutdown("sems");
  SOCK_CLEANUP;
}

void SnmpFactory::on_stop()
{
  agent_running.set(false);
  interrupt_snmp_agent();  
  join();
}

int SnmpFactory::onLoad()
{
  // init wakeup pipe
  if(pipe(wakeup_pipe)) {
    ERROR("pipe(): %s\n",strerror(errno));
    return -1;
  }

  start();
  return 0;
}

void SnmpFactory::onUnload()
{
  stop();
};

