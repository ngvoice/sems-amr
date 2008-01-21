
#include "trans_layer.h"
#include "sip_parser.h"
#include "hash_table.h"
#include "parse_cseq.h"
#include "sip_trans.h"
#include "log.h"

/** 
 * Singleton pointer.
 * @see instance()
 */
trans_layer* trans_layer::_instance;


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
    
    bucket.lock();
    sip_trans* t = bucket.match_request(msg);
    if(!t){
	DBG("Found new transaction\n");
	t = bucket.add_trans(msg,TT_UAS);
    }
    else {
	DBG("It's a retransmission\n");
    }
    bucket.unlock();
    
    //FIXME
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
