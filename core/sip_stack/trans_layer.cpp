
#include "trans_layer.h"
#include "sip_parser.h"
#include "hash_table.h"
#include "parse_cseq.h"
#include "parse_from_to.h"
#include "sip_trans.h"
#include "msg_fline.h"
#include "msg_hdrs.h"
#include "udp_trsp.h"
#include "log.h"

#include "MyCtrlInterface.h"
#include "AmUtils.h"
#include "../AmSipMsg.h"

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
			    const cstring& to_tag, const cstring& hdrs, 
			    const cstring& body)
{
    //
    // Fields to copy (from RFC 3261):
    //  - From
    //  - Call-ID
    //  - CSeq
    //  - Vias (same order)
    //  - To (+ tag if not yet present in request)    
    //
    
    bucket->lock();
    if(!bucket->exist(t)){
	bucket->unlock();
	ERROR("Invalid transaction key: transaction does not exist\n");
	return -1;
    }

    if(update_uas_reply(bucket,t,reply_code)<0){
      
      ERROR("Invalid state change\n");
      return -1;
    }


    sip_msg* req = t->msg;

    bool have_to_tag = true;
    int reply_len = status_line_len(reason);

    for(list<sip_header*>::iterator it = req->hdrs.begin();
	it != req->hdrs.end(); ++it) {

	switch((*it)->type){

	case sip_header::H_TO:
	    if(! ((sip_from_to*)(*it)->p)->tag.len ) {

		// To-tag not present in request
		have_to_tag = false;

		reply_len += 5/* ';tag=' */
		    + to_tag.len; 
	    }
	    else {
		t->to_tag = ((sip_from_to*)(*it)->p)->tag;
	    }
	    // fall-through-trap
	case sip_header::H_FROM:
	case sip_header::H_CALL_ID:
	case sip_header::H_CSEQ:
	case sip_header::H_VIA:
	    reply_len += copy_hdr_len(*it);
	    break;
	}
    }

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

    assert(transport);
    int err = transport->send(&req->remote_ip,reply_buf,reply_len);

    if((err < 0) || (reply_code < 200)){
	delete [] reply_buf;
    }
    else {
	t->retr_buf = reply_buf;
	t->retr_len = reply_len;
    }
    
    bucket->unlock();

    return err;
}

int trans_layer::send_request(sip_msg* msg)
{
    // NYI
    return -1;
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

    trans_bucket* bucket = get_trans_bucket(msg->callid->value, get_cseq(msg)->str);
    sip_trans* t = NULL;

    bucket->lock();

    switch(msg->type){
    case SIP_REQUEST: 
	
	if(t = bucket->match_request(msg)){
	    if(msg->u.request->method != t->msg->u.request->method){

		// ACK matched INVITE transaction
		DBG("ACK matched INVITE transaction\n");
		
		if(update_uas_request(bucket,t,msg)<0){
		    DBG("trans_layer::update_uas_trans() failed!\n");
		    // Anyway, there is nothing we can do...
		}
		DBG("t->state = %i\n",t->state);

		// should we forward the ACK to SEMS-App upstream?
	    }
	    else {
		DBG("Found retransmission\n");
		retransmit_reply(t);
	    }
	}
	else {

	    // New transaction:
	    //  let's pass the request to
	    //  the UA. 
	    //
	    // Forget the msg: it will 
	    //  owned by the new transaction

	    bucket->unlock();

	    assert(ua);
	    ua->handle_sip_request(bucket,msg);

	    return;
	}
	break;
    
    case SIP_REPLY:

	if(t = bucket->match_reply(msg)){

	    // Reply matched UAC transaction
	    // TODO: - update transaction state
	    //       - maybe retransmit ACK???
	}
	else {
	    // Anything we should do???
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
    return -1;
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
      t->state = TS_TERMINATED;
		
      // Instead of destroying the transaction
      // we handle the 2xx reply retransmission
      // by setting proper timer.
		
      // TODO: set ? timer
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
	    
  case TS_TERMINATED:
    // remove transaction
    bucket->remove_trans(t);
    return TS_REMOVED;
	    
  default:
    DBG("Bug? Unknown state at this point: %i\n",t->state);
  }

  return -1;
}

void trans_layer::retransmit_reply(sip_trans* t)
{
    // NYI
}
