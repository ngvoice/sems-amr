#include "SipCtrlInterface.h"

#include "AmUtils.h"
#include "../../AmSipMsg.h"

#include "trans_layer.h"
#include "sip_parser.h"
#include "parse_header.h"
#include "parse_from_to.h"
#include "parse_cseq.h"
#include "hash_table.h"
#include "sip_trans.h"

#include "udp_trsp.h"

#include "log.h"

#include <assert.h>

#include <stack>
using std::stack;

#ifndef _STANDALONE

#include "AmApi.h"
#include "AmConfigReader.h"

#ifndef MOD_NAME
#define MOD_NAME  "sipctrl"
#endif

EXPORT_CONTROL_INTERFACE_FACTORY(SipCtrlInterfaceFactory,MOD_NAME);

#endif


#ifndef _STANDALONE


AmCtrlInterface* SipCtrlInterfaceFactory::instance()
{
    SipCtrlInterface* ctrl = new SipCtrlInterface(bind_addr,bind_port);
    trans_layer::instance()->register_ua(ctrl);

    return ctrl;
}

int SipCtrlInterfaceFactory::onLoad()
{
    
    AmConfigReader cfg;
  
    if (cfg.loadFile(AmConfig::ModConfigPath + string(MOD_NAME ".conf"))) {
	
	WARN("failed to read/parse config file `%s' - assuming defaults\n",
	     (AmConfig::ModConfigPath + string(MOD_NAME ".conf")).c_str());
	
	bind_addr = AmConfig::LocalIP;
	bind_port = 5060;
    } 
    else {

	bind_addr = cfg.getParameter("bind_addr", AmConfig::LocalIP);
	bind_port = cfg.getParameterInt("bind_port", 5060);
    }

    INFO("bind_addr: `%s'.\n", bind_addr.c_str());
    INFO("bind_port: `%i'.\n", bind_port);

    return 0;
    
}
#endif

SipCtrlInterface::SipCtrlInterface(const string& bind_addr, unsigned short bind_port)
    : bind_addr(bind_addr), bind_port(bind_port)
{
    tl = trans_layer::instance();
}


int SipCtrlInterface::send(const AmSipRequest &req, string &serKey)
{
    sip_msg* msg = new sip_msg();
    
    msg->type = SIP_REQUEST;
    msg->u.request = new sip_request();

    msg->u.request->method_str = stl2cstr(req.method);
    msg->u.request->ruri_str = stl2cstr(req.r_uri);

    // To
    // From
    // Call-ID
    // CSeq
    // Contact
    // Max-Forwards
    
    string from = req.from;
    if(!req.from_tag.empty())
	from += ";tag=" + req.from_tag;

    msg->from = new sip_header(0,"From",stl2cstr(from));
    msg->hdrs.push_back(msg->from);

    msg->to = new sip_header(0,"To",stl2cstr(req.to));
    msg->hdrs.push_back(msg->to);

    msg->callid = new sip_header(0,"Call-ID",stl2cstr(req.callid));
    msg->hdrs.push_back(msg->callid);

    string cseq = int2str(req.cseq)
	+ " " + req.method;

    msg->cseq = new sip_header(0,"CSeq",stl2cstr(cseq));
    msg->hdrs.push_back(msg->cseq);

    msg->contact = new sip_header(0,"Contact",stl2cstr(req.contact));
    msg->hdrs.push_back(msg->contact);

    msg->hdrs.push_back(new sip_header(0,"Max-Forwards","10")); // FIXME

    if(!req.route.empty()){
	
 	char *c = (char*)req.route.c_str();
	
 	int err = parse_headers(msg,&c);
	
	if(err){
	    ERROR("Route headers parsing failed\n");
	    return -1;
	}

	//
	// parse_headers() appends our route headers 
	// to msg->hdrs and msg->route. But we want
	// only msg->route(), so we just remove the
	// route headers at the end of msg->hdrs.
	//
 	for(sip_header* h_rr = msg->hdrs.back();
 	    !msg->hdrs.empty(); h_rr = msg->hdrs.back()) {
	    
 	    if(h_rr->type != sip_header::H_ROUTE){
 		break;
 	    }
	    
	    msg->hdrs.pop_back();
 	}
    }
    
    tl->send_request(msg);
    delete msg;

    return 0;
}

void SipCtrlInterface::run()
{
    INFO("Starting SIP control interface\n");
    udp_trsp* udp_server = new udp_trsp(tl);

    udp_server->start();
    udp_server->join();
}

int SipCtrlInterface::send(const AmSipReply &rep)
{
    unsigned int  h=0;
    unsigned long t=0;

    if((sscanf(rep.serKey.c_str(),"%x:%lx",&h,&t) != 2) ||
       (h >= H_TABLE_ENTRIES)){
	ERROR("Invalid transaction key: invalid bucket ID\n");
	return -1;
    }
    
    return tl->send_reply(get_trans_bucket(h),(sip_trans*)t,
			  rep.code,stl2cstr(rep.reason),
			  stl2cstr(rep.local_tag), stl2cstr(rep.contact),
			  stl2cstr(rep.hdrs), stl2cstr(rep.body));
}

#define DBG_PARAM(p)\
    DBG("%s = <%s>\n",#p,p.c_str());

void SipCtrlInterface::handleSipMsg(AmSipRequest &req)
{
    DBG("Received new request:\n");

//     DBG_PARAM(req.cmd);
    DBG_PARAM(req.method);
//     DBG_PARAM(req.user);
//     DBG_PARAM(req.domain);
//     DBG_PARAM(req.dstip);
//     DBG_PARAM(req.port);
    DBG_PARAM(req.r_uri);
//     DBG_PARAM(req.from_uri);
    DBG_PARAM(req.from);
    DBG_PARAM(req.to);
    DBG_PARAM(req.callid);
//     DBG_PARAM(req.from_tag);
//     DBG_PARAM(req.to_tag);
    DBG("cseq = <%i>\n",req.cseq);
    DBG_PARAM(req.serKey);
    DBG_PARAM(req.route);
    DBG_PARAM(req.next_hop);
    DBG("hdrs = <%s>\n",req.hdrs.c_str());
    DBG("body = <%s>\n",req.body.c_str());

    if(req.method == "ACK")
	return;
    
    // Debug code - begin
    AmSipReply reply;
    
    reply.method    = req.method;
    reply.code      = 200;
    reply.reason    = "OK";
    reply.serKey    = req.serKey;
    reply.local_tag = "12345";
    reply.contact   = "sip:" + req.dstip + ":" + req.port;
    
    int err = send(reply);
    if(err < 0){
	DBG("send failed with err code %i\n",err);
    }
    // Debug code - end
}

void SipCtrlInterface::handleSipMsg(AmSipReply &rep)
{
    DBG("Received reply: %i %s\n",rep.code,rep.reason.c_str());
    DBG_PARAM(rep.callid);
    DBG_PARAM(rep.local_tag);
    DBG_PARAM(rep.remote_tag);
}

void SipCtrlInterface::handle_sip_request(const char* tid, sip_msg* msg)
{
    assert(msg->from && msg->from->p);
    assert(msg->to && msg->to->p);
    
    AmSipRequest req;
    
    req.cmd      = "sems";
    req.method   = c2stlstr(msg->u.request->method_str);
    req.user     = c2stlstr(msg->u.request->ruri.user);
    req.domain   = c2stlstr(msg->u.request->ruri.host);
    req.dstip    = get_addr_str(((sockaddr_in*)(&msg->local_ip))->sin_addr); //FIXME: IPv6
    req.port     = int2str(ntohs(((sockaddr_in*)(&msg->local_ip))->sin_port));
    req.r_uri    = c2stlstr(msg->u.request->ruri_str);
    req.from_uri = c2stlstr(((sip_from_to*)msg->from->p)->nameaddr.addr);
    req.from     = c2stlstr(msg->from->value);
    req.to       = c2stlstr(msg->to->value);
    req.callid   = c2stlstr(msg->callid->value);
    req.from_tag = c2stlstr(((sip_from_to*)msg->from->p)->tag);
    req.to_tag   = c2stlstr(((sip_from_to*)msg->to->p)->tag);
    req.cseq     = get_cseq(msg)->num;
    req.body     = c2stlstr(msg->body);
    req.serKey   = tid;

    prepare_routes(msg->record_route, req.route);
	
    handleSipMsg(req);
}

void SipCtrlInterface::handle_sip_reply(sip_msg* msg)
{
    assert(msg->from && msg->from->p);
    assert(msg->to && msg->to->p);
    
    AmSipReply   reply;

    reply.content_type = msg->content_type ? c2stlstr(msg->content_type->value): "";

    reply.body = msg->body.len ? c2stlstr(msg->body) : "";
    reply.cseq = get_cseq(msg)->num;

    reply.code   = msg->u.reply->code;
    reply.reason = c2stlstr(msg->u.reply->reason);

    if(msg->contact){

	char* c = msg->contact->value.s;
	sip_nameaddr na;
	
	int err = parse_nameaddr(&na,&c,msg->contact->value.len);
	if(err < 0) {
	    
	    ERROR("Contact nameaddr parsing failed\n");
	    return;
	}
	
	// 'Contact' header?
	reply.next_request_uri = c2stlstr(na.addr); 
    }

    reply.callid = c2stlstr(msg->callid->value);
    
    reply.remote_tag = c2stlstr(((sip_from_to*)msg->to->p)->tag);
    reply.local_tag  = c2stlstr(((sip_from_to*)msg->from->p)->tag);

    // Should i fill this one with anything
    // i do not understand??? -> H_OTHER ?
    //
    // reply.hdrs;
    
    if( (msg->u.reply->code >= 200 ) &&
	(msg->u.reply->code < 300 )){

	tl->send_200_ack(msg);
    }

    //
    // Will be computed in send_request()
    //
    // reply.next_hop;

    prepare_routes(msg->record_route, reply.route);
    
    handleSipMsg(reply);
}

void SipCtrlInterface::prepare_routes(const list<sip_header*>& routes, string& route_field)
{
    if(!routes.empty()){
	
	list<sip_header*>::const_iterator it = routes.begin();

	route_field = c2stlstr((*it)->value);
	++it;

	for(; it != routes.end(); ++it) {
		
	    route_field += ", " + c2stlstr((*it)->value);
	}
    }
}
