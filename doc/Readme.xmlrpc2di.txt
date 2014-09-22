xmlrpc2di: DI call via XMLRPC

This module makes the "Dynamic Invocation (DI)" Interfaces (internal module APIs)
exported by component modules accessible from XMLRPC. Additionaly the builtin 
methods "calls", "get_loglevel"/"set_loglevel",  
"get_shutdownmode"/"set_shutdownmode", and access to the statistics counters
(get_callsavg/get_callsmax/get_cpsmax/get_cpsavg) are implemented.

Additionally, it can be used as client to access XMLRPC servers. Applications
can use the DI function newConnection to add a new server entry, and sendRequest
to send a request. If sendRequest is executed, an active server is selected from 
the list and used to send the request. If sending the request failes, the server
is put inactive for a (configurable) while, and only then retried.

This module uses the XmlRpc++ library (http://xmlrpcpp.sourceforge.net/), which
is included, as it is currently not any more maintained by the major distributions.
Some patches have been applied to that library (multi threading, SSL etc).

The XMLRPC server can be configured to run as single threaded server (if
one request is executed, the next ones have to wait), or a multi-threaded server 
with a thread pool, in which case the callers only have to wait if there are 
as many requests served in parallel as threads. Note that it is a good idea to 
implement DI functions thread-safe, as they may be called from different threads
in SEMS anyway.

Configuration parameters
------------------------

  function         default    description
 +---------------------------------------
  server_ip        ANY        if set, bind only to the specific interface

  xmlrpc_port      8090       port to bind XMLRPC server to

  export_di        yes        enable 'di' function (see below)

  direct_export    none       search these interfaces for methods 
                              to export. load xmlrpc module after other
                              modules (i.e. in sems.conf: load_plugins=mymod;xmlrpc2di)
                              if you want to direct_export them.

  run_server       yes        start an XMLRPC server

  server_retry_after 10       retry a failed server after n seconds

  multithreaded    yes        MT or default server
 
  threads          5          only in case of multithreaded

Using XMLRPC2DI client over DI
------------------------------

DI method "newConnection"
 arguments: 
    "ssis":   app_name, server, port, uri
 example: 
 -- snip --
  AmDynInvoke* xmlrpc2di;
  AmDynInvokeFactory* xfFactory = AmPlugIn::instance()->getFactory4Di("xmlrpc2di");
  if (NULL == xfFactory || 
      NULL == (xmlrpc2di = xfFactory->getInstance())){
    ERROR("could not get xmlrpc2di. please load the xmlrpc2di module\n");
    return;
  }

  AmArg cargs, cret;    
  cargs.push("conf_auth");
  cargs.push("192.168.0.1");
  cargs.push(8102);
  cargs.push("");
  xmlrpc2di->invoke("newConnection", cargs, cret);
 -- snip --
             
DI method "sendRequest"
 arguments: 
    "ssa":    app_name, method, args
 example: 
 -- snip --
    AmArg args, ret;    
    args.push("conf_auth");
    args.push("authPin");  // method name
    args.assertArray(3);
    args[2].push("fancy_parameter"); // some parameters
    args[2].push("1234");
    xmlrpc2di->invoke("sendRequest", args, ret);
    if (ret[0].asInt() == 0) {
     DBG("status %d, description %s, uri %s", 
	ret[2][0].asInt(), ret[2][1].asCStr(), ret[2][3].asCStr());
    }
 -- snip --


Exporting functions from a DI interface to XMLRPC
-------------------------------------------------
The xmlrpc2di module searches the interfaces configured by the 
'direct_export' configuration variable for functions to export
via XMLRPC. The interface must provide a function '_list' which 
should return a list of methods to export. These methods are 
exported as XMLRPC functions in two ways:
 <method name>
and
 <interface name>.<method name>

If the function <method name> already exists, only 
<interface name>.<method name> is exported.

As an example, lets set 
 direct_export=di_dial
in xmlrpc2di.conf, and have a look at 
examples/di_dial/DIDial.cpp:
...
    } else if(method == "_list"){ 
      ret.push(AmArg("dial"));
      ret.push(AmArg("dial_auth"));
      ret.push(AmArg("dial_pin"));
      ret.push(AmArg("help"));
    } else 
...

When SEMS starts and loads the xmlrpc2di module, the _list 
method of the di_dial interface is called, and the functions 
are exported: 
(23386) DEBUG: onLoad (XMLRPC2DI.cpp:67): direct_export interfaces: di_dial
(23386) DEBUG: onLoad (XMLRPC2DI.cpp:77): XMLRPC Server: Enabling builtin method 'di'.
(23386) DEBUG: XMLRPC2DIServer (XMLRPC2DI.cpp:99):  XMLRPC Server: enabled builtin method 'calls'
(23386) DEBUG: XMLRPC2DIServer (XMLRPC2DI.cpp:100): XMLRPC Server: enabled builtin method 'get_loglevel'
(23386) DEBUG: XMLRPC2DIServer (XMLRPC2DI.cpp:101): XMLRPC Server: enabled builtin method 'set_loglevel'
(23386) DEBUG: registerMethods (XMLRPC2DI.cpp:154): XMLRPC Server: adding method 'dial'
(23386) DEBUG: registerMethods (XMLRPC2DI.cpp:160): XMLRPC Server: adding method 'di_dial.dial'
(23386) DEBUG: registerMethods (XMLRPC2DI.cpp:154): XMLRPC Server: adding method 'dial_auth'
(23386) DEBUG: registerMethods (XMLRPC2DI.cpp:160): XMLRPC Server: adding method 'di_dial.dial_auth'
(23386) DEBUG: registerMethods (XMLRPC2DI.cpp:154): XMLRPC Server: adding method 'dial_pin'
(23386) DEBUG: registerMethods (XMLRPC2DI.cpp:160): XMLRPC Server: adding method 'di_dial.dial_pin'
(23386) DEBUG: registerMethods (XMLRPC2DI.cpp:154): XMLRPC Server: adding method 'help'
(23386) DEBUG: registerMethods (XMLRPC2DI.cpp:160): XMLRPC Server: adding method 'di_dial.help'
(23386) DEBUG: XMLRPC2DIServer (XMLRPC2DI.cpp:115): Initialized XMLRPC2DIServer with:
(23386) DEBUG: XMLRPC2DIServer (XMLRPC2DI.cpp:116):                           port = 8090

Now from e.g. python we can do the following:
 >>> from xmlrpclib import *
 >>> s = ServerProxy("http://127.0.0.1:8090")
 >>> s.help()
 ['dial <application> <user> <from> <to>', 'dial_auth <application> <user> <from> <to> <realm> <auth_user> <auth_pwd>', 'dial_pin <application> <dialout pin> <local_user> <to_user>']
 >>> s.di_dial.help()
 ['dial <application> <user> <from> <to>', 'dial_auth <application> <user> <from> <to> <realm> <auth_user> <auth_pwd>', 'dial_pin <application> <dialout pin> <local_user> <to_user>']

a.s.o.

XMLRPC server function 'di'
--------------------------
Using the function 'di' all methods of all interfaces can be executed 
without the interfaces needing to list the methods in the '_list' call.
The first parameter to 'di' is alway the factory name, the second the
function name. Further parameters to XMLRPC calls are converted to DI 
ArgArray structure, and result of DI calls are converted back to XMLRPC 
parameters. At the moment only string, int and double types are implemented
(no DateTime, struct, binary, ...).

The 'di' function can be disabled on the XMLRPC server by setting 
 export_di=no
in xmlrpc2di.conf. By default it is enabled.


Examples (that hopefully trigger some creativity in the reader ;) 
-----------------------------------------------------------------

Checking load, setting log level:
 from xmlrpclib import *
 server = ServerProxy("http://127.0.0.1:8090")
 print server.calls()
 print "Current log level is ", server.get_loglevel()
 server.set_loglevel(3)

Register fritz at iptel.org using registrar client over XMLRPC 
from python:

 from xmlrpclib import *
 server = ServerProxy("http://127.0.0.1:8090")
 server.di('registrar_client', 'createRegistration', 
     'iptel.org',  'fritz', 'Frotz', 
     'fritz', 'secretpass', '')

or, having set 
 direct_export=registrar_client
directly:
 from xmlrpclib import *
 server = ServerProxy("http://127.0.0.1:8090")
 server.createRegistration('iptel.org',  'fritz', 'Frotz', 
     'fritz', 'secretpass', '')


To call someone into webconference (supports authenticated dial-out) 
over an account at sparvoip.de using di_dial:
from xmlrpclib import *
server = ServerProxy("http://127.0.0.1:8090")
server.di('di_dial', 'dial_auth','webconference', 'roomname',
 'sip:myuser@sparvoip.de', 'sip:0049301234567@sparvoip.de',
 'sparvoip.de','myuser','passwd')


