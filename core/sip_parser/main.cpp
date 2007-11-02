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
#include "log.h"



//
// Parser functions:
//


static int parse_reply(sip_reply* rep)
{
    DBG("**parse_reply**\n");
    DBG("Not yet implemented !!!\n");
    return 0;
}


void debug_headers()
{
    
}

int main()
{
    log_level  = 3;
    log_stderr = 1;

    char* msg = 
	"INVITE sip:bob@biloxi.com;user=phone;tti=13;ttl=12?abc=def SIP/2.0\r\n"
 	"Via: SIP/2.0/UDP bigbox3.site3.atlanta.com  \r\n"
	"  \t  ; branch=z9hG4bK77ef4c2312983.1\r\n"
 	"Via: SIP/2.0/UDP pc33.atlanta.com;branch=z9hG4bKnashds8\r\n"
 	" ;received=192.0.2.1,\r\n"
	" SIP/2.0/UDP bigbox3.site3.atlanta.com\r\n"
	" ;branch=z9hG4bK77ef4c2312983.1\r\n"
	"Max-Forwards: 69\r\n"
	"To: Bob <sip:bob@biloxi.com>\r\n"
	"From: Alice <sip:alice@atlanta.com>;tag=1928301774\r\n"
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

    int err = 0;
    int msg_type = parse_msg_type(msg);

    switch(msg_type){

    case SIP_REQUEST:{
	
	sip_request req;
	req.msg_buf = msg;
	
	err = parse_request(&req);
	if(!err && !req.hdrs.vias.empty()){

	    for(list<sip_header*>::iterator it = req.hdrs.vias.begin();
		it != req.hdrs.vias.end(); ++it) {

		sip_via via;
		err = parse_via(&via,(*it)->value.s,
				(*it)->value.len);

		if(err)
		    break;

// 		DBG("via: proto=%i, val=%.*s\n", via.trans.type,
// 		    via.trans.val.len, via.trans.val.s);
	    }
	}
    }
	break;
	
    case SIP_REPLY:{
    
	sip_reply rep;
	rep.msg_buf = msg;

	err = parse_reply(&rep); 
    }

    default:
	err = msg_type;
    }
    
    INFO("parse_sip_msg returned %i\n",err);

    return 0;
}
