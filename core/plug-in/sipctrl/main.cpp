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

#include "trans_layer.h"
#include "udp_trsp.h"

#include "log.h"

#include "MyCtrlInterface.h"
#include "../../AmSipMsg.h"
#define SERVER

int main()
{
    log_level  = 3;
    log_stderr = 1;

    trans_layer* tl = trans_layer::instance();
    udp_trsp* udp_server = new udp_trsp(tl);
    MyCtrlInterface* ctrl = MyCtrlInterface::instance();
    
#ifndef SERVER
    char* buf = 
	"INVITE sip:bob@biloxi.com;user=phone;tti=13;ttl=12?abc=def SIP/2.0\r\n"
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
    sip_msg* msg = new sip_msg(buf,buf_len);
    
    tl->received_msg(msg);

#else
    
    udp_server->bind("127.0.0.1",5060);
    udp_server->start();

//     AmSipRequest req;
//     req.method   = "INVITE";
//     req.r_uri    = "sip:sipp@127.0.0.1:5070";
//     req.from     = "SEMS <sip:sems@127.0.0.1:5060>";
//     req.from_tag = "12345";
//     req.to       = "SIPP <sip:sipp@127.0.0.1:5070>";
//     req.cseq     = 10;
//     req.callid   = "12345@127.0.0.1";
//     req.contact  = "sip:127.0.0.1";

//     int send_err = ctrl->send(req, req.serKey);
//     if(send_err < 0) {
//       ERROR("ctrl->send() failed with error code %i\n",send_err);
//     }

    udp_server->join();
    
#endif


#ifdef SERVER

#if 0
    if(msg->type == SIP_REQUEST){

	sip_msg reply;
	reply.type = SIP_REPLY;
	
	int len = status_line_len(cstring("Bad Request"));
	len += copy_hdrs_len(msg->hdrs);

	len += 2; // CRLF

	reply.buf = new char[len];
	reply.len = len;

	char* c = reply.buf;
	status_line_wr(&reply,&c,400,cstring("Bad Request"));
	copy_hdrs_wr(&reply,&c,msg->hdrs);
	
	*(c++) = CR;
	*(c++) = LF;

	DBG("reply msg: \"%.*s\"\n",reply.len,reply.buf);

	sendto(sd,reply.buf,reply.len,0,
	       (sockaddr*)&msg->recved,msg->recved_len);


    }
#endif

    //FIXME: enter udp_server loop
    
#else
    delete msg;

#endif
    

    return 0;
}
