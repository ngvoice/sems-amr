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


#include "parse_header.h"
#include "parse_common.h"
#include "log.h"

#include <memory>
using std::auto_ptr;


//
// Header length
//

#define TO_len             2
#define VIA_len            3
#define FROM_len           4
#define CSEQ_len           4
#define ROUTE_len          5
#define CALL_ID_len        7
#define CONTACT_len        7
#define CONTENT_TYPE_len   12
#define RECORD_ROUTE_len   12
#define CONTENT_LENGTH_len 14


//
// Low case headers 
//

char* TO_lc = "to";
char* VIA_lc = "via";
char* FROM_lc = "from";
char* CSEQ_lc = "cseq";
char* ROUTE_lc = "route";
char* CALL_ID_lc = "call-id";
char* CONTACT_lc = "contact";
char* CONTENT_TYPE_lc = "content-type";
char* RECORD_ROUTE_lc = "record-route";
char* CONTENT_LENGTH_lc = "content-length";


static int parse_header_type(sip_headers* hdrs, sip_header* h)
{
    h->type = sip_header::H_UNPARSED;

    switch(h->name.len){

    case TO_len:
	if(!lower_cmp(h->name.s,TO_lc,TO_len)){
	    h->type = sip_header::H_TO;
	    hdrs->to = h;
	}
	break;

    case VIA_len:
	if(!lower_cmp(h->name.s,VIA_lc,VIA_len)){
	    h->type = sip_header::H_VIA;
	    hdrs->vias.push_back(h);
	}
	break;

    //case FROM_len:
    case CSEQ_len:
	switch(h->name.s[0]){
	case 'f':
	case 'F':
	    if(!lower_cmp(h->name.s+1,FROM_lc+1,FROM_len-1)){
		h->type = sip_header::H_FROM;
		hdrs->from = h;
	    }
	    break;
	case 'c':
	case 'C':
	    if(!lower_cmp(h->name.s+1,CSEQ_lc+1,CSEQ_len-1)){
		h->type = sip_header::H_CSEQ;
		hdrs->cseq = h;
	    }
	    break;
	default:
	    h->type = sip_header::H_OTHER;
	    break;
	}
	break;

    case ROUTE_len:
	if(!lower_cmp(h->name.s+1,ROUTE_lc+1,ROUTE_len-1)){
	    h->type = sip_header::H_ROUTE;
	    hdrs->route.push_back(h);
	}
	break;

    //case CALL_ID_len:
    case CONTACT_len:
	switch(h->name.s[0]){
	case 'c':
	case 'C':
	    switch(h->name.s[1]){
	    case 'a':
	    case 'A':
		if(!lower_cmp(h->name.s+2,CALL_ID_lc+2,CALL_ID_len-2)){
		    h->type = sip_header::H_CALL_ID;
		    hdrs->call_id = h;
		}
		break;

	    case 'o':
	    case 'O':
		if(!lower_cmp(h->name.s+2,CONTACT_lc+2,CONTACT_len-2)){
		    h->type = sip_header::H_CONTACT;
		    hdrs->contact = h;
		}
		break;

	    default:
		h->type = sip_header::H_OTHER;
		break;
	    }
	    break;

	default:
	    h->type = sip_header::H_OTHER;
	    break;
	}
	break;

    //case RECORD_ROUTE_len:
    case CONTENT_TYPE_len:
	switch(h->name.s[0]){
	case 'c':
	case 'C':
	    if(!lower_cmp(h->name.s,CONTENT_TYPE_lc,CONTENT_TYPE_len)){
		h->type = sip_header::H_CONTENT_TYPE;
		hdrs->content_type = h;
	    }
	    break;
	case 'r':
	case 'R':
	    if(!lower_cmp(h->name.s,RECORD_ROUTE_lc,RECORD_ROUTE_len)){
		h->type = sip_header::H_RECORD_ROUTE;
		hdrs->record_route.push_back(h);
	    }
	    break;
	}
	break;

    case CONTENT_LENGTH_len:
	if(!lower_cmp(h->name.s,CONTENT_LENGTH_lc,CONTENT_LENGTH_len)){
	    h->type = sip_header::H_CONTENT_LENGTH;
	    hdrs->content_length = h;
	}
	break;

    }

    if(h->type == sip_header::H_UNPARSED)
	h->type = sip_header::H_OTHER;

    return h->type;
}

static void add_parsed_header(sip_headers* hdrs, sip_header* hdr)
{
    parse_header_type(hdrs,hdr);
    hdrs->hdrs.push_back(hdr);
}

int parse_headers(sip_headers* hdrs, char** c)
{
    //
    // Header states
    //
    enum {
	H_NAME=0,
	H_HCOLON,
	H_VALUE_SWS,
	H_VALUE,
    };

    int st = H_NAME;
    char* begin = *c;
    bool cr = false;

    auto_ptr<sip_header> hdr(new sip_header());

    for(;;(*c)++){

	switch(**c){
	case '\0':
	    switch(st){

	    case H_VALUE:
		if(*((*c)-1) != LF){
		    hdr->value.set(begin, *c-begin);
		    add_parsed_header(hdrs,hdr.release());
		    hdr.reset(new sip_header());
		}
		return 0;

	    case H_NAME:
		if(*c-begin == 0){
		    return 0;
		}
		//go to default

	    default:
		DBG("Incomplete header\n");
		return UNEXPECTED_EOT;

	    }
	    break;

	case HCOLON:
	    switch(st){
	    case H_NAME:
		st = H_HCOLON;
		hdr->name.set(begin,*c-begin);
		break;
	    case H_HCOLON:
		st = H_VALUE_SWS;
		begin = *c+1;
	    }
	    break;

	case HTAB:
	case SP:
	    switch(st){

	    case H_NAME:
		st = H_HCOLON;
		hdr->name.set(begin,*c-begin);
		break;

	    case H_HCOLON:// skip SP
	    case H_VALUE_SWS:
	    case H_VALUE:
		break;

	    default:
		DBG("Bug!! no idea what to do... (st=%i) <%s>\n",st,*c);
		return UNDEFINED_ERR;
		break;

	    }
	    break;

	case CR:
	    if(*(*c+1) != LF){
		DBG("CR without LF\n");
		return MALFORMED_SIP_MSG;
	    }
	    
	    cr = true;
	    break;

	case LF:

	    if((st == H_NAME) && ((*c-(cr?1:0))-begin == 0)){
		DBG("Detected end of headers\n");
		(*c)++;
		return 0;
	    }

	    if(st<H_VALUE_SWS){
		DBG("Malformed header: <%.*s>\n",(*c-(cr?1:0))-begin,begin);
		begin = ++(*c);
		st = H_NAME;
		continue;
	    }
	    
	    if(IS_WSP(*(*c+1))){
		(*c)+=2;
		cr = false;
		continue;
	    }

	    hdr->value.set(begin,(*c-(cr?1:0))-begin);
	    add_parsed_header(hdrs,hdr.release());
	    hdr.reset(new sip_header());

	    cr = false;
	    begin = *c+1;
	    st = H_NAME;

	    break;

	default:
	    switch(st){
	    case H_VALUE_SWS:
	    case H_HCOLON:
		st = H_VALUE;
		begin = *c;
		break;
	    }
	    break;
	}
    }

    return 0;
}
