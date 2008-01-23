/*
 * $Id$
 *
 * Copyright (C) 2007 Raphael Coeffic
 *
 * This file is part of sems, a free SIP media server.
 *
 * sems is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version
 *
 * For a license to use the ser software under conditions
 * other than those described here, or to purchase support for this
 * software, please contact iptel.org by e-mail at the following addresses:
 *    info@iptel.org
 *
 * sems is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License 
 * along with this program; if not, write to the Free Software 
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#include "hash_table.h"
#include "hash.h"

#include "sip_parser.h"
#include "parse_header.h"
#include "parse_cseq.h"
#include "parse_via.h"
#include "parse_from_to.h"
#include "sip_trans.h"

#include "log.h"

#include <assert.h>

//
// Global transaction table
//
trans_bucket _trans_table[H_TABLE_ENTRIES];



trans_bucket::trans_bucket()
{
    pthread_mutex_init(&m,NULL);
}

trans_bucket::~trans_bucket()
{
    pthread_mutex_destroy(&m);
}

void trans_bucket::lock()
{
    pthread_mutex_lock(&m);
}

void trans_bucket::unlock()
{
    pthread_mutex_unlock(&m);
}


sip_trans* trans_bucket::match_request(sip_msg* msg)
{
    // assert(msg && msg->cseq && msg->callid);
    // sip_cseq* cseq  = dynamic_cast<sip_cseq*>(msg->cseq->p);
    // assert(cseq);

    //this should have been checked before
    assert(msg->via_p1);

    if(elmts.empty())
	return NULL;

    bool do_3261_match = false;
    sip_trans* t = NULL;

    // Try first RFC 3261 matching
    if(msg->via_p1->branch.len > MAGIC_BRANCH_LEN){

	do_3261_match = !memcmp(msg->via_p1->branch.s,
				MAGIC_BRANCH_COOKIE,
				MAGIC_BRANCH_LEN);
    }

    if(do_3261_match){
	
	char* branch = msg->via_p1->branch.s + MAGIC_BRANCH_LEN;
	int   len = msg->via_p1->branch.len - MAGIC_BRANCH_LEN;
	
	trans_list::iterator it = elmts.begin();
	for(;it!=elmts.end();++it) {
	    
	    if( ((*it)->type != TT_UAS) || 
		((*it)->msg->type != SIP_REQUEST)){
		continue;
	    }

	    if( (msg->u.request->method != (*it)->msg->u.request->method) &&
		( (msg->u.request->method != sip_request::ACK) ||
		  ((*it)->msg->u.request->method != sip_request::INVITE) ) )
		continue;

	    if((*it)->msg->via_p1->branch.len != len + MAGIC_BRANCH_LEN)
		continue;

	    if((*it)->msg->via_p1->host.len != 
	       msg->via_p1->host.len)
		continue;

	    if((*it)->msg->via_p1->port.len != 
	       msg->via_p1->port.len)
		continue;

	    if(memcmp((*it)->msg->via_p1->branch.s + MAGIC_BRANCH_LEN,
		      branch,len))
		continue;

	    if(memcmp((*it)->msg->via_p1->branch.s + MAGIC_BRANCH_LEN,
		      branch,len))
		continue;

	    if(memcmp((*it)->msg->via_p1->host.s,
		      msg->via_p1->host.s,msg->via_p1->host.len))
		continue;

	    if(memcmp((*it)->msg->via_p1->port.s,
		      msg->via_p1->port.s,msg->via_p1->port.len))
		continue;

	    // found matching transaction
	    t = *it; 
	    break;
	}
    }
    else {

	// Pre-3261 matching

	sip_from_to* from = dynamic_cast<sip_from_to*>(msg->from->p);
	sip_from_to* to = dynamic_cast<sip_from_to*>(msg->to->p);
	sip_cseq* cseq = dynamic_cast<sip_cseq*>(msg->cseq->p);

	assert(from && to && cseq);

	trans_list::iterator it = elmts.begin();
	for(;it!=elmts.end();++it) {

	    
	    //Request matching:
	    // Request-URI
	    // From-tag
	    // Call-ID
	    // Cseq
	    // top Via
	    // To-tag
	    
	    //ACK matching:
	    // Request-URI
	    // From-tag
	    // Call-ID
	    // Cseq (number only)
	    // top Via
	    // + To-tag of reply
	    
	    if( ((*it)->type != TT_UAS) || 
		((*it)->msg->type != SIP_REQUEST))
		continue;

	    if( (msg->u.request->method != (*it)->msg->u.request->method) &&
		( (msg->u.request->method != sip_request::ACK) ||
		  ((*it)->msg->u.request->method != sip_request::INVITE) ) )
		continue;

	    sip_from_to* it_from = dynamic_cast<sip_from_to*>((*it)->msg->from->p);
	    if(from->tag.len != it_from->tag.len)
		continue;

	    sip_cseq* it_cseq = dynamic_cast<sip_cseq*>((*it)->msg->cseq->p);
	    if(cseq->str.len != it_cseq->str.len)
		continue;

	    if(memcmp(from->tag.s,it_from->tag.s,from->tag.len))
		continue;

	    if(memcmp(cseq->str.s,it_cseq->str.s,cseq->str.len))
		continue;

	    
	    if(msg->u.request->method == sip_request::ACK){
		
		// ACKs must include To-tag from previous reply
		if(to->tag.len != (*it)->to_tag.len)
		    continue;

		if(memcmp(to->tag.s,(*it)->to_tag.s,to->tag.len))
		    continue;

		if((*it)->reply_status < 300){

		    // 2xx ACK matching

		    //TODO: additional work for dialog matching???
		    //      R-URI should match reply Contact ...
		    break;
		}
	    }
	    else { 
		// non-ACK
		sip_from_to* it_to = dynamic_cast<sip_from_to*>((*it)->msg->to->p);
		if(to->tag.len != it_to->tag.len)
		    continue;

		if(memcmp(to->tag.s,it_to->tag.s,to->tag.len))
		    continue;
	    }

	    // non-ACK and non-2xx ACK matching

	    if((*it)->msg->u.request->ruri_str.len != 
	       msg->u.request->ruri_str.len )
		continue;
	    
	    if(memcmp(msg->u.request->ruri_str.s,
		      (*it)->msg->u.request->ruri_str.s,
		      msg->u.request->ruri_str.len))
		continue;
	    
	    //TODO: missing top-Via matching
	    
	    // found matching transaction
	    t = *it;
	    break;
	}
    }

    return t;
}

sip_trans* trans_bucket::match_reply(sip_msg* msg)
{
    ERROR("NYI\n");
    return NULL;
}

sip_trans* trans_bucket::add_trans(sip_msg* msg, int ttype)
{
    sip_trans* t = new sip_trans();

    t->msg  = msg;
    t->type = ttype;

    t->reply_status = 0;

    if(msg->u.request->method == sip_request::INVITE){
	
	if(t->type == TT_UAS)
	    t->state = TS_PROCEEDING;
	else
	    t->state = TS_CALLING;
    }
    else {
	t->state = TS_TRYING;
    }

    elmts.push_back(t);
    
    return t;
}

trans_bucket::trans_list::iterator trans_bucket::find_trans(sip_trans* t)
{
    trans_list::iterator it = elmts.begin();
    for(;it!=elmts.end();++it)
	if(*it == t)
	    break;
    
    return it;
}

bool trans_bucket::exist(sip_trans* t)
{
    return find_trans(t) != elmts.end();
}

void trans_bucket::remove_trans(sip_trans* t)
{
    trans_list::iterator it = find_trans(t);

    if(it != elmts.end()){
	elmts.erase(it);
	delete t;
	DBG("~sip_trans()\n");
    }
}

unsigned int hash(const cstring& ci, const cstring& cs)
{
    unsigned int h=0;

    h = hashlittle(ci.s,ci.len,h);
    h = hashlittle(cs.s,ci.len,h);

    return h & (H_TABLE_ENTRIES-1);
}


trans_bucket* get_trans_bucket(const cstring& callid, const cstring& cseq_num)
{
    return &_trans_table[hash(callid,cseq_num)];
}

trans_bucket* get_trans_bucket(unsigned int h)
{
    assert(h < H_TABLE_ENTRIES);
    return &_trans_table[h];
}
