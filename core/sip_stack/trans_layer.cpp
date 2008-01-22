
#include "trans_layer.h"
#include "sip_parser.h"
#include "hash_table.h"
#include "parse_cseq.h"
#include "parse_from_to.h"
#include "sip_trans.h"
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


trans_layer::trans_layer()
{
    ctrl = MyCtrlInterface::instance();
}

trans_layer::~trans_layer()
{}


trans_layer* trans_layer::instance()
{
    if(!_instance)
	_instance = new trans_layer();

    return _instance;
}


void trans_layer::send_msg(sip_msg* msg)
{
    // NYI
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

    trans_bucket& bucket = get_trans_bucket(msg->callid->value, get_cseq(msg)->str);
    sip_trans* t = NULL;

    bucket.lock();

    switch(msg->type){
    case SIP_REQUEST: 
	
	if(t = bucket.match_request(msg)){
	    if(msg->u.request->method != t->msg->u.request->method){

		// ACK matched INVITE transaction
		DBG("ACK matched INVITE transaction\n");
		
		if(update_uas_trans(bucket,t,msg)<0){
		    DBG("trans_layer::update_uas_trans() failed!\n");
		    // Anyway, there is nothing we can do...
		}
		
		// should we forward the ACK to SEMS-App upstream?
	    }
	    else {
		DBG("Found retransmission\n");
		retransmit_reply(t);
	    }
	}
	else {

	    // It's a new transaction:
	    //  let's create it and pass to
	    //  the UA. 
	    //
	    // Forget the msg: it is now 
	    //  owned by the transaction

	    t = bucket.add_trans(msg, TT_UAS);
	    
	    assert(msg->from && msg->from->p);
	    assert(msg->to && msg->to->p);
	    
	    AmSipRequest req;

	    req.serKey = int2hex(hash(msg->callid->value, get_cseq(msg)->str)) 
		+ ":" + long2hex((unsigned long)t);

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

	    bucket.unlock();

	    ctrl->handleSipMsg(req);
	    return;
	}
	break;
    
    case SIP_REPLY:

	if(t = bucket.match_reply(msg)){

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
    bucket.unlock();
    DROP_MSG;
}


int trans_layer::update_uac_trans(trans_bucket& bucket, sip_trans* t, sip_msg* msg)
{
    return -1;
}

int trans_layer::update_uas_trans(trans_bucket& bucket, sip_trans* t, sip_msg* msg)
{
    assert(t && (t->type == TT_UAS));

    switch(msg->type){
	
    case SIP_REQUEST:
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
	    bucket.remove_trans(t);
	    return TS_REMOVED;
	    
	default:
	    DBG("Bug? Unknown state at this point: %i\n",t->state);
	}
	break;

    case SIP_REPLY:
	if(t->reply_status >= 200){
	    ERROR("Trying to send a reply whereby reply_status >= 300\n");
	    return -1;
	}

	t->reply_status = msg->u.reply->code;
	
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

    default:
	ERROR("Bug? Unknown request type\n");
	break;
    }

    return -1;
}

void trans_layer::retransmit_reply(sip_trans* t)
{
    // NYI
}
