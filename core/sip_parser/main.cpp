/*
 * $Id:$
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

#include "sip_parser.h"

#include "parse_uri.h"
#include "parse_common.h"
#include "parse_header.h"

#include "parse_via.h"
#include "parse_from_to.h"
#include "parse_cseq.h"

#include "log.h"

#include <sys/socket.h>
#include <netinet/in.h>
#include <errno.h>

//
// Parser functions:
//



void debug_headers()
{
    
}

int main()
{
    log_level  = 3;
    log_stderr = 1;

#if 0
    char* buf = 
	"INVITE sip:b\nob@biloxi.com;user=phone;tti=13;ttl=12?abc=def SIP/2.0\r\n"
 	"Via: SIP/2.0/UDP bigbox3.site3.atlanta.com;branch=z9hG4bK77ef4c2312983.1\r\n"
 	"Via: SIP/2.0/UDP pc33.atlanta.com;branch=z9hG4bKnashds8\r\n"
 	" ;received=192.0.2.1\r\n"
	"Max-Forwards: 69\r\n"
	"To: Bob <sip:bob@biloxi.com>\r\n"
	"From: sip:alice@atlanta.com;tag=1928301774\r\n"
	"Call-ID: a84b4c76e66710\r\n"
	"CSeq: 314159 INVITE\r\n"
	"Contact: <sip:alice@pc33.atlanta.com>\r\n"
	"Content-Type: application/sdp\r\n"
	"Content-Length: 148\r\n"
	"\r\n"
	"v=0\r\n"
	"o=alice 53655765 2353687637 IN IP4 pc33.atlanta.com\r\n"
	"s=-\r\n"
	"t=0 0\r\n"
	"c=IN IP4 pc33.atlanta.com\r\n"
	"m=audio 3456 RTP/AVP 0 1 3 99\r\n"
	"a=rtpmap:0 PCMU/8000";

    int buf_len = strlen(buf);

#else
    
    int sd;
    if((sd = socket(PF_INET,SOCK_DGRAM,0)) == -1){
	ERROR("socket: %s\n",strerror(errno));
	return -1;
    } 

    sockaddr_in addr;

    addr.sin_family = AF_INET;
    addr.sin_port = htons(5060);
    addr.sin_addr.s_addr = INADDR_ANY;

    if(bind(sd,(const struct sockaddr*)&addr,
	     sizeof(struct sockaddr_in))) {

	DBG("bind: %s\n",strerror(errno));		
	close(sd);
	return -1;
    }
    
    int true_opt = 1;
    if(setsockopt(sd, SOL_SOCKET, SO_REUSEADDR,
		  (void*)&true_opt, sizeof (true_opt)) == -1) {
	
	ERROR("%s\n",strerror(errno));
	close(sd);
	return -1;
    }

    char buf[65536];
    int buf_len;

    while(true){

	buf_len = recv(sd,buf,65535,MSG_TRUNC);
	if(buf_len > 65535){
	    ERROR("Message was too big (>65535)\n");
	    continue;
	}

#endif





    int err = 0;

    sip_msg msg(buf,buf_len);

    err = parse_sip_msg(&msg);

    if(!err && !msg.vias.empty()){

	for(list<sip_header*>::iterator it = msg.vias.begin();
	    it != msg.vias.end(); ++it) {
	    
	    sip_via via;
	    err = parse_via(&via,(*it)->value.s,
			    (*it)->value.len);
	    
	    if(err)
		break;

	    list<sip_via_parm*>::iterator pit = via.parms.begin();
	    for(;pit!=via.parms.end();++pit){

		DBG("via: proto=%i, val=%.*s\n", (*pit)->trans.type,
		    (*pit)->trans.val.len, (*pit)->trans.val.s);
	    }
	}
    }

    if(!err && msg.from){

	sip_from_to f;
	err = parse_from_to(&f,
			    msg.from->value.s,
			    msg.from->value.len);

	if(err) 
	    goto end;

	DBG("From header: name-addr=\"%.*s <%.*s>\"\n",
	    f.nameaddr.name.len,f.nameaddr.name.s,
	    f.nameaddr.addr.len,f.nameaddr.addr.s);

	sip_uri fu;
	err = parse_uri(&fu,f.nameaddr.addr.s,f.nameaddr.addr.len);
	if(err)
	    goto end;

	list<sip_avp*>::iterator it = f.params.begin();
	for(;it != f.params.end(); ++it) {
		
	    DBG("From header param: \"%.*s=%.*s\"\n",
		(*it)->name.len,(*it)->name.s,
		(*it)->value.len,(*it)->value.s);
	}
    }

    if(!err && msg.to){

	sip_from_to t;
	err = parse_from_to(&t,
			    msg.to->value.s,
			    msg.to->value.len);

	if(err) 
	    goto end;

	DBG("To header: name-addr=\"%.*s <%.*s>\"\n",
	    t.nameaddr.name.len,t.nameaddr.name.s,
	    t.nameaddr.addr.len,t.nameaddr.addr.s);

	list<sip_avp*>::iterator it = t.params.begin();
	for(it = t.params.begin();it != t.params.end(); ++it) {
		
	    DBG("To header param: \"%.*s=%.*s\"\n",
		(*it)->name.len,(*it)->name.s,
		(*it)->value.len,(*it)->value.s);
	}

    }

    if(!err && msg.cseq){

	sip_cseq cseq;
	err = parse_cseq(&cseq,msg.cseq->value.s,msg.cseq->value.len);
	if(err)
	    goto end;

	DBG("Cseq header: '%.*s' '%.*s'\n",
	    cseq.number.len,cseq.number.s,
	    cseq.method.len,cseq.method.s);
    }

 end:
    INFO("parse_sip_msg returned %i\n",err);

#if 1

    }

#endif


    return 0;
}
