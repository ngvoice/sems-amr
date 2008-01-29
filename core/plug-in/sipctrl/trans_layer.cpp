
#include "trans_layer.h"
#include "sip_parser.h"
#include "hash_table.h"
#include "parse_cseq.h"
#include "parse_from_to.h"
#include "sip_trans.h"
#include "msg_fline.h"
#include "msg_hdrs.h"
#include "udp_trsp.h"
#include "resolver.h"
#include "log.h"

#include "MyCtrlInterface.h"
#include "AmUtils.h"
#include "../../AmSipMsg.h"

#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include <assert.h>

/** 
 * Singleton pointer.
 * @see instance()
 */
trans_layer* trans_layer::_instance = NULL;


trans_layer* trans_layer::instance()
{
    if(!_instance)
	_instance = new trans_layer();

    return _instance;
}


trans_layer::trans_layer()
    : ua(NULL),
      transport(NULL)
{
}

trans_layer::~trans_layer()
{}


void trans_layer::register_ua(sip_ua* ua)
{
    this->ua = ua;
}

void trans_layer::register_transport(udp_trsp* trsp)
{
    transport = trsp;
}



int trans_layer::send_reply(trans_bucket* bucket, sip_trans* t,
			    int reply_code, const cstring& reason,
			    const cstring& to_tag, const cstring& contact,
			    const cstring& hdrs, const cstring& body)
{
    // Ref.: RFC 3261 8.2.6, 12.1.1
    //
    // Fields to copy (from RFC 3261):
    //  - From
    //  - Call-ID
    //  - CSeq
    //  - Vias (same order)
    //  - To (+ tag if not yet present in request)
    //  - (if a dialog is created) Record-Route
    //
    // Fields to generate (if INVITE transaction):
    //    - Contact
    //    - Route: copied from 
    // 
    // SHOULD be contained:
    //  - Allow, Supported
    //
    // MAY be contained:
    //  - Accept


    bucket->lock();
    if(!bucket->exist(t)){
	bucket->unlock();
	ERROR("Invalid transaction key: transaction does not exist\n");
	return -1;
    }

    sip_msg* req = t->msg;

    bool have_to_tag = false;
    bool add_contact = false;
    int  reply_len   = status_line_len(reason);

    for(list<sip_header*>::iterator it = req->hdrs.begin();
	it != req->hdrs.end(); ++it) {

	switch((*it)->type){

	case sip_header::H_TO:
	    if(! ((sip_from_to*)(*it)->p)->tag.len ) {

		reply_len += 5/* ';tag=' */
		    + to_tag.len; 
	    }
	    else {
		// To-tag present in request
		have_to_tag = true;

		t->to_tag = ((sip_from_to*)(*it)->p)->tag;
	    }
	    // fall-through-trap
	case sip_header::H_FROM:
	case sip_header::H_CALL_ID:
	case sip_header::H_CSEQ:
	case sip_header::H_VIA:
	case sip_header::H_RECORD_ROUTE:
	    reply_len += copy_hdr_len(*it);
	    break;
	}
    }

    // We do not send Contact for
    // 100 provisional replies
    //
    if((reply_code > 100) && contact.len){
	
	reply_len += contact_len(contact);
	add_contact = true;
    }
    reply_len += 2/*CRLF*/;
    
    // Allocate buffer for the reply
    //
    char* reply_buf = new char[reply_len];
    char* c = reply_buf;

    status_line_wr(&c,reply_code,reason);

    for(list<sip_header*>::iterator it = req->hdrs.begin();
	it != req->hdrs.end(); ++it) {

	switch((*it)->type){

	case sip_header::H_TO:
	    memcpy(c,(*it)->name.s,(*it)->name.len);
	    c += (*it)->name.len;
	    
	    *(c++) = ':';
	    *(c++) = SP;
	    
	    memcpy(c,(*it)->value.s,(*it)->value.len);
	    c += (*it)->value.len;
	    
	    if(!have_to_tag){

		memcpy(c,";tag=",5);
		c += 5;

		t->to_tag.s = c;
		t->to_tag.len = to_tag.len;

		memcpy(c,to_tag.s,to_tag.len);
		c += to_tag.len;
	    }

	    *(c++) = CR;
	    *(c++) = LF;
	    break;

	case sip_header::H_FROM:
	case sip_header::H_CALL_ID:
	case sip_header::H_CSEQ:
	case sip_header::H_VIA:
	    copy_hdr_wr(&c,*it);
	    break;
	}
    }

    if(add_contact){
	contact_wr(&c,contact);
    }

    *c++ = CR;
    *c++ = LF;

    assert(transport);
    int err = transport->send(&req->remote_ip,reply_buf,reply_len);

    if(err < 0){
	delete [] reply_buf;
	goto end;
    }

    err = update_uas_reply(bucket,t,reply_code);
    if(err < 0){
	
	ERROR("Invalid state change\n");
	delete [] reply_buf;
    }
    else if(err != TS_TERMINATED) {
	
	t->retr_buf = reply_buf;
	t->retr_len = reply_len;

	err = 0;
    }
    else {
	// Transaction has been deleted
	delete [] reply_buf;

	err = 0;
    }
    
 end:
    bucket->unlock();
    return err;
}

int trans_layer::send_request(sip_msg* msg)
{
    // Request-URI
    // To
    // From
    // Call-ID
    // CSeq
    // Max-Forwards
    // Via
    // Contact
    // Supported / Require
    // Content-Length / Content-Type
    
    assert(transport);

    int err = parse_uri(&msg->u.request->ruri,
			msg->u.request->ruri_str.s,
			msg->u.request->ruri_str.len);
    if(err < 0){
	ERROR("Invalid Request URI\n");
	return -1;
    }

    err = resolver::instance()->resolve_name(c2stlstr(msg->u.request->ruri.host).c_str(),
					     &msg->remote_ip,IPv4,UDP);
    if(err < 0){
	ERROR("Unresolvable Request URI\n");
	return -1;
    }

    ((sockaddr_in*)&msg->remote_ip)->sin_port = htons(msg->u.request->ruri.port);
    
    int request_len = request_line_len(msg->u.request->method_str,
				       msg->u.request->ruri_str);

    char branch_buf[BRANCH_BUF_LEN];
    cstring branch(branch_buf,BRANCH_BUF_LEN);
    compute_branch(branch.s,msg->callid->value,msg->cseq->value);

    cstring via((char*)transport->get_local_ip());
    request_len += via_len(via,branch);

    request_len += copy_hdrs_len(msg->hdrs);
    request_len += 2/* CRLF end-of-headers*/;

    // Allocate new message
    sip_msg* p_msg = new sip_msg();
    p_msg->buf = new char[request_len];
    p_msg->len = request_len;

    // generate it
    char* c = p_msg->buf;
    request_line_wr(&c,msg->u.request->method_str,
		    msg->u.request->ruri_str);

    via_wr(&c,via,branch);
    copy_hdrs_wr(&c,msg->hdrs);

    *c++ = CR;
    *c++ = LF;

    // and parse it
    if(parse_sip_msg(p_msg)){
	ERROR("Parser failed on generate request\n");
	delete p_msg;
	return MALFORMED_SIP_MSG;
    }

    memcpy(&p_msg->remote_ip,&msg->remote_ip,sizeof(sockaddr_storage));

    int send_err = transport->send(&p_msg->remote_ip,p_msg->buf,p_msg->len);
    if(send_err < 0){
	ERROR("Error from transport layer\n");
	delete p_msg;
    }
    else {
	trans_bucket* bucket = get_trans_bucket(p_msg->callid->value,
						get_cseq(p_msg)->str);
	bucket->lock();
	sip_trans* t = bucket->add_trans(p_msg,TT_UAC);
	bucket->unlock();
    }
    
    return send_err;
}

void trans_layer::received_msg(sip_msg* msg)
{
#define DROP_MSG \
          delete msg;\
          return

    int err = parse_sip_msg(msg);
    DBG("parse_sip_msg returned %i\n",err);

    if(err){
	DBG("Message was: \"%.*s\"\n",msg->len,msg->buf);
	DBG("dropping message\n");
	DROP_MSG;
    }
    
    assert(msg->callid && get_cseq(msg));
    if(!msg->callid || !get_cseq(msg)){
	
	DBG("Call-ID or CSeq header missing: dropping message\n");
	DROP_MSG;
    }

    unsigned int  h = hash(msg->callid->value, get_cseq(msg)->str);
    trans_bucket* bucket = get_trans_bucket(h);
    sip_trans* t = NULL;

    bucket->lock();

    switch(msg->type){
    case SIP_REQUEST: 
	
	if(t = bucket->match_request(msg)){
	    if(msg->u.request->method != t->msg->u.request->method){
		
		// ACK matched INVITE transaction
		DBG("ACK matched INVITE transaction\n");
		
		err = update_uas_request(bucket,t,msg);
		if(err<0){
		    DBG("trans_layer::update_uas_trans() failed!\n");
		    // Anyway, there is nothing we can do...
		}
		else if(err == TS_TERMINATED){
		    
		}
		
		// do not touch the transaction anymore:
		// it could have been deleted !!!

		// should we forward the ACK to SEMS-App upstream? Yes
	    }
	    else {
		DBG("Found retransmission\n");
		retransmit(t);
	    }
	}
	else {

	    string t_id;
	    sip_trans* t = NULL;
	    if(msg->u.request->method != sip_request::ACK){
		
		// New transaction
		t = bucket->add_trans(msg, TT_UAS);

		t_id = int2hex(h) 
		    + ":" + long2hex((unsigned long)t);
	    }

	    bucket->unlock();
	    
	    //  let's pass the request to
	    //  the UA. 
	    assert(ua);
	    ua->handle_sip_request(t_id.c_str(),msg);

	    if(!t){
		DROP_MSG;
	    }
	    //Else:
	    // forget the msg: it will 
	    // owned by the new transaction
	    return;
	}
	break;
    
    case SIP_REPLY:

	if(t = bucket->match_reply(msg)){

	    // Reply matched UAC transaction
	    
	    DBG("Reply matched an existing transaction\n");
	    if(update_uac_trans(bucket,t,msg) < 0){
		ERROR("update_uac_trans() failed, so what happens now???\n");
		break;
	    }
	    // do not touch the transaction anymore:
	    // it could have been deleted !!!
	}
	else {
	    DBG("Reply did NOT match any existing transaction...\n");
	    DBG("reply code = %i\n",msg->u.reply->code);
	    if( (msg->u.reply->code >= 200) &&
	        (msg->u.reply->code <  300) ) {
		
		bucket->unlock();
		
		// pass to UA
		assert(ua);
		ua->handle_sip_reply(msg);
		
		DROP_MSG;
	    }
	}
	break;

    default:
	ERROR("Got unknown message type: Bug?\n");
	break;
    }

 unlock_drop:
    bucket->unlock();
    DROP_MSG;
}


int trans_layer::update_uac_trans(trans_bucket* bucket, sip_trans* t, sip_msg* msg)
{
    assert(msg->type == SIP_REPLY);

    cstring to_tag;
    int     reply_code = msg->u.reply->code;

    DBG("reply code = %i\n",msg->u.reply->code);

    if(reply_code < 200){

	// Provisional reply
	switch(t->state){

	case TS_TRYING:
	case TS_CALLING:
	    t->state = TS_PROCEEDING;

	case TS_PROCEEDING:
	    goto pass_reply;

	case TS_COMPLETED:
	default:
	    goto end;
	}
    }
    
    to_tag = ((sip_from_to*)msg->to->p)->tag;
    if(!to_tag.len){
	DBG("To-tag missing in final reply\n");
	return -1;
    }
    
    if(t->msg->u.request->method == sip_request::INVITE){
    
	if(reply_code >= 300){
	    
	    // Final error reply
	    switch(t->state){
		
	    case TS_CALLING:
	    case TS_PROCEEDING:
		
		t->state = TS_COMPLETED;
		send_non_200_ack(t,msg);
		
		// TODO: set D timer?
		
		goto pass_reply;
		
	    case TS_COMPLETED:
		// retransmit non-200 ACK
		retransmit(t);
	    default:
		goto end;
	    }
	} 
	else {
	    
	    // Positive final reply to INVITE transaction
	    switch(t->state){
		
	    case TS_CALLING:
	    case TS_PROCEEDING:
		t->state = TS_TERMINATED;
		bucket->remove_trans(t);
		goto pass_reply;
		
	    default:
		goto end;
	    }
	}
    }
    else {

	// Final reply
	switch(t->state){
	    
	case TS_TRYING:
	case TS_CALLING:
	case TS_PROCEEDING:
	    
	    t->state = TS_COMPLETED;
	    
	    // TODO: set K timer?
	    
	    goto pass_reply;
	    
	default:
	    goto end;
	}
    }

 pass_reply:
    assert(ua);
    ua->handle_sip_reply(msg);
 end:
    return 0;
}

int trans_layer::update_uas_reply(trans_bucket* bucket, sip_trans* t, int reply_code)
{
    if(t->reply_status >= 200){
	ERROR("Trying to send a reply whereby reply_status >= 300\n");
	return -1;
    }

    t->reply_status = reply_code;

    if(t->reply_status >= 300){

	// error reply
	t->state = TS_COMPLETED;
	    
	if(t->msg->u.request->method == sip_request::INVITE){
	    //TODO: set G timer ?
	}
	else {
	    //TODO: set J timer ?
	}
    }
    else if(t->reply_status >= 200) {

	if(t->msg->u.request->method == sip_request::INVITE){

	    // final reply
	    //bucket->remove_trans(t);
	    return TS_TERMINATED_200;
	}
	else {
	    t->state = TS_COMPLETED;
	    //TODO: set J timer
	}
    }
    else {
	// provisional reply
	t->state = TS_PROCEEDING;
    }
	
    return t->state;
}

int trans_layer::update_uas_request(trans_bucket* bucket, sip_trans* t, sip_msg* msg)
{
    if(msg->u.request->method != sip_request::ACK){
	ERROR("Bug? Recvd non-ACK for existing UAS transaction\n");
	return -1;
    }
	
    switch(t->state){
	    
    case TS_COMPLETED:
	t->state = TS_CONFIRMED;
	// TODO: remove G and H timer.
	// TODO: set I timer.

	// drop through
    case TS_CONFIRMED:
	return t->state;
	    
    case TS_TERMINATED_200:
	// remove transaction
	bucket->remove_trans(t);
	return TS_REMOVED;
	    
    default:
	DBG("Bug? Unknown state at this point: %i\n",t->state);
    }

    return -1;
}

void trans_layer::send_non_200_ack(sip_trans* t, sip_msg* reply)
{
    sip_msg* inv = t->msg;
    
    cstring method("ACK",3);
    int ack_len = request_line_len(method,inv->u.request->ruri_str);
    
    ack_len += copy_hdr_len(inv->via1)
	+ copy_hdr_len(inv->from)
	+ copy_hdr_len(reply->to)
	+ copy_hdr_len(inv->callid);
    
    ack_len += cseq_len(get_cseq(inv)->str,get_cseq(inv)->method);
    
    if(!inv->route.empty())
	ack_len += copy_hdrs_len(inv->route);
    
    char* ack_buf = new char [ack_len];
    char* c = ack_buf;

    request_line_wr(&c,method,inv->u.request->ruri_str);
    
    copy_hdr_wr(&c,inv->via1);
    copy_hdr_wr(&c,inv->from);
    copy_hdr_wr(&c,reply->to);
    copy_hdr_wr(&c,inv->callid);
    
    cseq_wr(&c,get_cseq(inv)->str,get_cseq(inv)->method);
    
    if(!inv->route.empty())
	copy_hdrs_wr(&c,inv->route);
    
    *c++ = CR;
    *c++ = LF;

    assert(transport);
    int send_err = transport->send(&inv->remote_ip,ack_buf,ack_len);
    if(send_err < 0){
	ERROR("Error from transport layer\n");
	delete ack_buf;
    }

    t->retr_buf = ack_buf;
    t->retr_len = ack_len;
}

void trans_layer::send_200_ack(sip_msg* reply)
{
    // Set request URI
    // TODO: use correct R-URI instead of just 'Contact'
    if(!reply->contact) {
	DBG("Sorry, reply has no Contact header: could not send ACK\n");
	return;
    }
    
    sip_nameaddr na;
    char* c = reply->contact->value.s;
    if(parse_nameaddr(&na,&c,reply->contact->value.len) < 0){
	DBG("Sorry, reply's Contact parsing failed: could not send ACK\n");
	return;
    }
    
    if(parse_uri(&na.uri,na.addr.s,na.addr.len) < 0){
	DBG("Sorry, reply's Contact URI parsing failed: could not send ACK\n");
	return;
    }

    int request_len = request_line_len(cstring("ACK",3),na.addr);
    
    // Set destination address
    // TODO: get correct next hop from RURI and Route
    sockaddr_storage remote_ip;
    int err = resolver::instance()->resolve_name(c2stlstr(na.uri.host).c_str(),
						 &remote_ip,IPv4,UDP);
    if(err != 0){
	ERROR("Invalid IP address in URI: inet_aton failed\n");
	return;
    }

    ((sockaddr_in*)&remote_ip)->sin_port = htons(na.uri.port ? na.uri.port : 5060);
   
    char branch_buf[BRANCH_BUF_LEN];
    cstring branch(branch_buf,BRANCH_BUF_LEN);
    compute_branch(branch.s,reply->callid->value,reply->cseq->value);

    sip_header* max_forward = new sip_header(0,cstring("Max-Forwards"),cstring("10"));

    cstring via((char*)transport->get_local_ip()); 

    request_len += via_len(via,branch);
    
    request_len += copy_hdr_len(reply->to);
    request_len += copy_hdr_len(reply->from);
    request_len += copy_hdr_len(reply->callid);
    request_len += copy_hdr_len(max_forward);
    request_len += cseq_len(get_cseq(reply)->str,cstring("ACK",3));
    request_len += 2/* CRLF end-of-headers*/;

    // Allocate new message
    char* ack_buf = new char[request_len];

    // generate it
    c = ack_buf;

    request_line_wr(&c,cstring("ACK",3),na.addr);
    via_wr(&c,via,branch);
    copy_hdr_wr(&c,reply->from);
    copy_hdr_wr(&c,reply->to);
    copy_hdr_wr(&c,reply->callid);
    copy_hdr_wr(&c,max_forward);
    cseq_wr(&c,get_cseq(reply)->str,cstring("ACK",3));

    *c++ = CR;
    *c++ = LF;

    DBG("About to send ACK: \n<%.*s>\n",request_len,ack_buf);

    assert(transport);
    int send_err = transport->send(&remote_ip,ack_buf,request_len);
    if(send_err < 0){
	ERROR("Error from transport layer\n");
    }

    delete [] ack_buf;
    delete max_forward;
}

void trans_layer::retransmit(sip_trans* t)
{
    DBG("NYI\n");
}
