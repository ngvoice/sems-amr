#include "MyCtrlInterface.h"

#include "AmUtils.h"
#include "../../AmSipMsg.h"

#include "trans_layer.h"
#include "sip_parser.h"
#include "parse_header.h"
#include "parse_from_to.h"
#include "parse_cseq.h"
#include "hash_table.h"
#include "sip_trans.h"

#include "log.h"

#include <assert.h>

#include <stack>
using std::stack;

MyCtrlInterface* MyCtrlInterface::_instance = NULL;


MyCtrlInterface* MyCtrlInterface::instance()
{
    if(!_instance)
	_instance = new MyCtrlInterface();
    
    return _instance;
}

MyCtrlInterface::MyCtrlInterface()
{
    tl = trans_layer::instance();
    tl->register_ua(this);
}


int MyCtrlInterface::send(const AmSipRequest &req, string &serKey)
{
    sip_msg* msg = new sip_msg();
    
    msg->type = SIP_REQUEST;
    msg->u.request = new sip_request();

    msg->u.request->method_str = stl2cstr(req.method);
    // TODO: parse method and set msg->u.request.method
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
	
 	stack<sip_header*> route_hdrs;
 	for(list<sip_header*>::reverse_iterator it = msg->hdrs.rbegin();
 	    it != msg->hdrs.rend(); it--) {
	    
 	    if((*it)->type != sip_header::H_ROUTE){
 		break;
 	    }
	    
 	    route_hdrs.push(*it);
 	}
	
 	for(;!route_hdrs.empty(); route_hdrs.pop()) {
 	    msg->route.push_back(route_hdrs.top());
 	}
    }

//     if(!req.route.empty()){
// 	msg->route.push_back(new sip_header(0,"Route",stl2cstr(req.route)));
// 	msg->hdrs.push_back(msg->route.back());
//     }
    
    msg->content_length = new sip_header(0,"Content-Length","0");
    msg->hdrs.push_back(msg->content_length); // FIXME
    
    tl->send_request(msg);
    delete msg;
}

int MyCtrlInterface::send(const AmSipReply &rep)
{
    unsigned int h=0;
    sip_trans*   t=0;

    if((sscanf(rep.serKey.c_str(),"%x:%x",&h,(unsigned long)&t) != 2) ||
       (h >= H_TABLE_ENTRIES)){
	ERROR("Invalid transaction key: invalid bucket ID\n");
	return -1;
    }
    
    return tl->send_reply(get_trans_bucket(h),t,
			  rep.code,stl2cstr(rep.reason),
			  stl2cstr(rep.local_tag), stl2cstr(rep.contact),
			  stl2cstr(rep.hdrs), stl2cstr(rep.body));
}

#define DBG_PARAM(p)\
    DBG("%s = <%s>\n",#p,p.c_str());

void MyCtrlInterface::handleSipMsg(AmSipRequest &req)
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

void MyCtrlInterface::handleSipMsg(AmSipReply &rep)
{
    DBG("Received reply: %i %s\n",rep.code,rep.reason.c_str());
    DBG_PARAM(rep.callid);
    DBG_PARAM(rep.local_tag);
    DBG_PARAM(rep.remote_tag);
}

void MyCtrlInterface::handle_sip_request(const char* tid, sip_msg* msg)
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

void MyCtrlInterface::handle_sip_reply(sip_msg* msg)
{
    assert(msg->from && msg->from->p);
    assert(msg->to && msg->to->p);
    
    AmSipReply   reply;

    //reply.next_hop;
    //reply.route;

    reply.content_type = msg->content_type ? c2stlstr(msg->content_type->value): "";

    //reply.hdrs;
    reply.body = msg->body.len ? c2stlstr(msg->body) : "";
    reply.cseq = get_cseq(msg)->num;

    reply.code   = msg->u.reply->code;
    reply.reason = c2stlstr(msg->u.reply->reason);

    //reply.next_request_uri
    
    reply.callid = c2stlstr(msg->callid->value);
    
    reply.remote_tag = c2stlstr(((sip_from_to*)msg->to->p)->tag);
    reply.local_tag  = c2stlstr(((sip_from_to*)msg->from->p)->tag);

    if(msg->u.reply->code >= 200)
	tl->send_200_ack(msg);

    prepare_routes(msg->record_route, reply.route);
    
    handleSipMsg(reply);
}

void MyCtrlInterface::prepare_routes(const list<sip_header*>& routes, string& route_field)
{
    if(!routes.empty()){
	
	list<sip_header*>::const_iterator it = routes.begin();

	route_field = c2stlstr((*it)->value);
	++it;

	// TODO: req.route
	for(; it != routes.end(); ++it) {
		
	    route_field += ", " + c2stlstr((*it)->value);
	}
    }
}
