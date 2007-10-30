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


#include "parse_request.h"
#include "parse_header.h"
#include "parse_common.h"
#include "log.h"


char* INVITEm = "INVITE";
#define INVITE_len 6

char* ACKm = "ACK";
#define ACK_len 3

char* OPTIONSm = "OPTIONS";
#define OPTIONS_len 7

char* BYEm = "BYE";
#define BYE_len 3

char* CANCELm = "CANCEL";
#define CANCEL_len 6

char* REGISTERm = "REGISTER";
#define REGISTER_len 8


int parse_method(int* method, char* beg, int len)
{
    char* c = beg;
    char* end = c+len;

    *method = sip_request::OTHER_METHOD;

    switch(len){
    case INVITE_len:
    //case CANCEL_len:
	switch(*c){
	case 'I':
	    if(!memcmp(c+1,INVITEm+1,INVITE_len-1)){
		DBG("Found INVITE\n");
		*method = sip_request::INVITE;
	    }
	    break;
	case 'C':
	    if(!memcmp(c+1,CANCELm+1,CANCEL_len-1)){
		DBG("Found CANCEL\n");
		*method = sip_request::CANCEL;
	    }
	    break;
	}
	break;

    case ACK_len:
    //case BYE_len:
	switch(*c){
	case 'A':
	    if(!memcmp(c+1,ACKm-1,ACK_len-1)){
		DBG("Found ACK\n");
		*method = sip_request::ACK;
	    }
	    break;
	case 'B':
	    if(!memcmp(c+1,BYEm-1,BYE_len-1)){
		DBG("Found BYE\n");
		*method = sip_request::BYE;
	    }
	    break;
	}

    case OPTIONS_len:
	if(!memcmp(c+1,OPTIONSm-1,OPTIONS_len-1)){
	    DBG("Found OPTIONS\n");
	    *method = sip_request::OPTIONS;
	}
	break;

    case REGISTER_len:
	if(!memcmp(c+1,REGISTERm-1,REGISTER_len-1)){
	    DBG("Found REGISTER\n");
	    *method = sip_request::REGISTER;
	}
	break;
    }
    
    // other method
    for(;c!=end;c++){
	if(!IS_TOKEN(*c)){
	    DBG("!IS_TOKEN(%c): MALFORMED_SIP_MSG\n",*c);
	    return MALFORMED_SIP_MSG;
	}
    }

    if(*method == sip_request::OTHER_METHOD){
	DBG("Found other method\n");
    }

    return 0;
}

static int parse_request_line(sip_request* req, char** c)
{
    //
    // Request-line states:
    //
    enum {
	RL_METHOD=0,
	RL_RURI,
	RL_SIPVER
    };

    int   ret   = 0;
    char* begin = *c;
    int   st    = RL_METHOD;
    bool  cr    = false;

    for(;;(*c)++){
	switch(**c){

	case '\0':
	    DBG("Unexpected EoT with st=%i\n",st);
	    return UNEXPECTED_EOT;

	case CR:
	    if(*((*c)+1) != LF){
		DBG("CR without LF\n");
		return MALFORMED_SIP_MSG;
	    }
	    
	    cr = true;
	    break;

	case LF:
	    if(st == RL_SIPVER){
		ret = parse_sip_version(begin,(*c - (cr?1:0))-begin);

		// End of Request-line
		(*c)++;
		return ret;
	    }

	    return UNEXPECTED_EOL;

	case SP:
	    switch(st){
		
	    case RL_METHOD:
		ret = parse_method(&(req->method),begin,*c-begin);
		if(ret!=0) return ret;
		begin = ++(*c);
		st = RL_RURI;
		continue;
		
	    case RL_RURI: // check if '@' occurs in URI
		          // before calling parse_uri
		ret = parse_uri(&(req->ruri),begin,*c-begin);
		if(ret!=0) return ret;
		begin = ++(*c);
		st = RL_SIPVER;
		continue;

	    case RL_SIPVER:
		// no space allowed after SIP-Version
		DBG("no space allowed after SIP-Version\n");
		return MALFORMED_SIP_MSG;

	    default:
		ERROR("parse_request_line: Unknown state %i\n",st);
		return UNDEFINED_ERR;
	    }
	    break;

	default:
	    continue;
	}
    }

    // should never reach this point...
    return UNDEFINED_ERR;
}

int parse_request(sip_request* req)
{
    char* c = req->msg_buf;

    int ret = parse_request_line(req,&c);
    if(ret != 0){
	return ret;
    }

    ret = parse_headers(&(req->hdrs),&c);
    if(!ret){
	DBG("body length: %i bytes\n",strlen(c));
	DBG("body: \"%s\"\n",c);
    }

    //TODO: parse_body()

    return ret;
}
