
#include "trans_layer.h"
#include "sip_parser.h"
#include "hash_table.h"
#include "parse_cseq.h"
#include "sip_trans.h"
#include "log.h"

#include "AmUtils.h"
#include "../AmSipMsg.h"

/** 
 * Singleton pointer.
 * @see instance()
 */
trans_layer* trans_layer::_instance = NULL;


trans_layer::trans_layer()
{}

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
    
    if(!msg->callid || !get_cseq(msg)){
	
	DBG("Call-ID or CSeq header missing: dropping message\n");
	DROP_MSG;
    }

    trans_bucket& bucket = get_trans_bucket(msg->callid->value, get_cseq(msg)->number);
    sip_trans* t = NULL;

    bucket.lock();

    switch(msg->type){
    case SIP_REQUEST: 
	
	if(t = bucket.match_request(msg)){
	    if(msg->u.request->method != t->msg->u.request->method){

		// ACK matched INVITE transaction

		// TODO: - update transaction state
		//       - absorb ACK
	    }
	    else {
		DBG("Found retransmission\n");
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
	    
	    // TODO: create AmSipRequest and pass it upstream.

	    AmSipRequest req;

	    req.serKey = int2hex(hash(msg->callid->value, get_cseq(msg)->number)) 
		+ ":" + long2hex((unsigned long)t);

	    // TODO: other fields

	    bucket.unlock();

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


int trans_layer::update_uac_trans(sip_trans* t, sip_msg* msg)
{
    return -1;
}

int trans_layer::update_uas_trans(sip_trans* t, sip_msg* msg)
{
    return -1;
}
