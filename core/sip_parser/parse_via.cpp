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

#include "parse_via.h"
#include "parse_common.h"

#include "log.h"

#include <memory>
using std::auto_ptr;

sip_via_parm::~sip_via_parm()
{
    list<sip_avp*>::iterator it = params.begin();
    for(;it != params.end(); ++it) {

	delete *it;
    }
}

sip_via::~sip_via()
{
    list<sip_via_parm*>::iterator it = parms.begin();
    for(;it != parms.end(); ++it) {

	delete *it;
    }
}

static int parse_transport(sip_transport* t, char** c, int len)
{
    enum {

	TR_BEG,
	TR_UDP,
	TR_T,
	TR_TCP,
	TR_TLS,
	TR_SCTP,
	TR_OTHER
    };

    if(len < SIPVER_len + 2){ // at least "SIP/2.0/?"
	DBG("Transport protocol is too small\n");
	return MALFORMED_SIP_MSG;
    }

    if (parse_sip_version(*c,SIPVER_len)) {
	DBG("Wrong/unsupported SIP version\n");
	return MALFORMED_SIP_MSG;
    }
    
    *c += SIPVER_len;
    
    if (*(*c)++ != '/') {
	DBG("Missing '/' after SIP version\n");
	return MALFORMED_SIP_MSG;
    }

    int st = TR_BEG;
    t->val.s = *c;

    len -= SIPVER_len+1;
    char* end = *c + len;

    for(;**c && (*c!=end);(*c)++){

	switch(st){

	case TR_BEG:
	    switch(**c){

	    case 'U':
		if ((len >= 3) && !memcmp(*c+1,"DP",2)) {
		    t->type = sip_transport::UDP;
		    st = TR_UDP;
		    *c += 2;
		}
		else
		    st = TR_OTHER;
		break;

	    case 'T':
		st = TR_T;
		break;

	    case 'S':
		if ((len >= 4) && !memcmp(*c+1,"CTP",3)) {
		    t->type = sip_transport::SCTP;
		    st = TR_SCTP;
		    *c += 3;
		}
		else
		    st = TR_OTHER;
		break;
		
	    default:
		st = TR_OTHER;
		break;
	    }
	    break;

	case TR_T:
	    switch(**c){
	    case 'L':
		if((len >= 3) && (*(*c+1) == 'S')){
		    t->type = sip_transport::TLS;
		    st = TR_TLS;
		    (*c)++;
		}
		else
		    st = TR_OTHER;
		break;
		    
	    case 'C':
		if((len >= 3) && (*(*c+1) == 'P')){
		    t->type = sip_transport::TCP;
		    st = TR_TCP;
		    (*c)++;
		}
		else
		    st = TR_OTHER;
		break;

	    default:
		st = TR_OTHER;
		break;
	    }
	    break;

	case TR_UDP:
	case TR_TCP:
	case TR_TLS:
	case TR_SCTP:
	    switch(**c){
	    case '\0':
	    case CR:
	    case LF:
	    case SP:
		t->val.len = *c - t->val.s;
		return 0;

	    default:
		st = TR_OTHER;
		break;
	    }
	    break;

	case TR_OTHER:
	    switch(**c){
	    case '\0':
	    case CR:
	    case LF:
	    case SP:
		t->val.len = *c - t->val.s;
		t->type = sip_transport::OTHER;
		return 0;
	    }
	    break;
	}
    }

    t->val.len = *c - t->val.s;
    t->type = sip_transport::OTHER;
    return 0;
}

static int parse_by(cstring* host, cstring* port, char** c, int len)
{
    enum {
	BY_HOST=0,
	BY_HOST_V6,
	BY_COLON,
	BY_PORT_SWS,
	BY_PORT
    };

    int saved_st, st=BY_HOST;

    char* beg = *c;
    char* end = beg+len;

    for(;*c!=end;(*c)++){

	switch(st){

	case BY_HOST:
	    switch(**c){

	    case_CR_LF;

	    case '[':
		st = BY_HOST_V6;
		break;

	    case ':':
		st = BY_PORT_SWS;
		host->set(beg,*c - beg);
		break;

	    case ';':
		host->set(beg,*c - beg);
		return 0;

	    case SP:
	    case HTAB:
		st = BY_COLON;
		host->set(beg,*c - beg);
		break;
	    }
	    break;

	case BY_HOST_V6:
	    switch(**c){

	    case ']':
		st = BY_COLON;
		host->set(beg,*c+1 - beg);
		break;

	    case SP:
	    case HTAB:
	    case CR:
	    case LF:
		DBG("Bad character in IPv6 address\n");
		return MALFORMED_SIP_MSG;
	    }
	    break;

	case BY_COLON:
	    switch(**c){

	    case_CR_LF;

	    case ':':
		st = BY_PORT_SWS;
		break;

	    case ';':
		return 0;

	    case SP:
	    case HTAB:
		break;

	    default:
		DBG("LWS expected\n");
		return MALFORMED_SIP_MSG;
	    }
	    break;

	case BY_PORT_SWS:

	    switch(**c){

	    case_CR_LF;

	    case SP:
	    case HTAB:
		break;

	    default:
		st = BY_PORT;
		beg = *c;
		break;
	    }
	    break;

	case BY_PORT:

	    switch(**c){

	    case_CR_LF;

	    case SP:
	    case HTAB:
	    case ';':
		port->set(beg,*c-beg);
		return 0;
	    }
	    break;

	case_ST_CR(**c);

	case ST_LF:
	case ST_CRLF:
	    switch(saved_st){
	    case BY_HOST:
		saved_st = BY_COLON;
		host->set(beg,*c - (st==ST_CRLF?2:1) - beg);
		break;
	    case BY_PORT:
		port->set(beg,*c - (st==ST_CRLF?2:1) - beg);
		return 0;
	    }
	    st = saved_st;
	    break;
	}
    }

    DBG("st = %i\n",st);
    return UNDEFINED_ERR;
}

static int parse_via_params(list<sip_avp*>* params, char** c, int len)
{
    enum {
	VP_PARAM_SEP=0,
	VP_PARAM_SEP_SWS,
	VP_PNAME,
	VP_PNAME_EQU,
	VP_PNAME_EQU_SWS,
	VP_PVALUE,
	VP_PVALUE_QUOTED
    };

    char* beg = *c;
    char* end = beg+len;
    int saved_st,st=VP_PARAM_SEP;

    auto_ptr<sip_avp> avp(new sip_avp());

    for(;*c!=end;(*c)++){

	switch(st){

	case VP_PARAM_SEP:
	    switch(**c){

	    case_CR_LF;

	    case SP:
	    case HTAB:
		break;
		
	    case ';':
		st = VP_PARAM_SEP_SWS;
		break;

	    case ',':
		return 0;

	    default:
		DBG("';' expected, found '%c'\n",**c);
		return MALFORMED_SIP_MSG;
	    }
	    break;

	case VP_PARAM_SEP_SWS:
	    switch(**c){

	    case_CR_LF;

	    case SP:
	    case HTAB:
		break;

	    default:
		st = VP_PNAME;
		beg=*c;
		break;
	    }
	    break;
	    
	case VP_PNAME:
	    switch(**c){

	    case_CR_LF;

	    case SP:
	    case HTAB:
		st = VP_PNAME_EQU;
		avp->name.set(beg,*c-beg);
		break;

	    case '=':
		st = VP_PNAME_EQU_SWS;
		avp->name.set(beg,*c-beg);
		break;

	    case ';':
		st = VP_PARAM_SEP_SWS;
		avp->name.set(beg,*c-beg);
		params->push_back(avp.release());
		avp.reset(new sip_avp());
		break;

	    case ',':
		avp->name.set(beg,*c-beg);
		params->push_back(avp.release());
		return 0;

	    }
	    break;

	case VP_PNAME_EQU:
	    switch(**c){

	    case_CR_LF;

	    case SP:
	    case HTAB:
		break;

	    case '=':
		st = VP_PNAME_EQU_SWS;
		break;

	    case ';':
		st = VP_PARAM_SEP_SWS;
		params->push_back(avp.release());
		avp.reset(new sip_avp());
		break;

	    case ',':
		params->push_back(avp.release());
		return 0;

	    default:
		DBG("'=' expected\n");
		return MALFORMED_SIP_MSG;
	    }

	case VP_PNAME_EQU_SWS:
	    switch(**c){

	    case_CR_LF;

	    case SP:
	    case HTAB:
		break;
		
	    case '\"':
		st = VP_PVALUE_QUOTED;
		beg = *c;
		break;

	    default:
		st = VP_PVALUE;
		beg = *c;
		break;
	    }
	    break;

	case VP_PVALUE:
	    switch(**c){

	    case_CR_LF;

	    case '\"':
		st = VP_PVALUE_QUOTED;
		break;

	    case ';':
		st = VP_PARAM_SEP_SWS;
		avp->value.set(beg,*c-beg);
		params->push_back(avp.release());
		avp.reset(new sip_avp());
		break;

	    case ',':
		avp->value.set(beg,*c-beg);
		params->push_back(avp.release());
		return 0;
	    }
	    break;

	case VP_PVALUE_QUOTED:
	    switch(**c){

	    case_CR_LF;

	    case '\"':
		st = VP_PARAM_SEP;
		avp->value.set(beg,*c+1-beg);
		params->push_back(avp.release());
		avp.reset(new sip_avp());
		break;
		
	    case '\\':
		if(!*(++(*c))){
		    DBG("Escape char in quoted str at EoT!!!\n");
		    return MALFORMED_SIP_MSG;
		}
		break;
	    }
	    break;

	case_ST_CR(**c);
	
	case ST_LF:
	case ST_CRLF:
	    switch(saved_st){

	    case VP_PNAME:
		saved_st = VP_PNAME_EQU;
		avp->name.set(beg,*c-(st==ST_CRLF?2:1)-beg);
		break;

	    case VP_PVALUE:
		saved_st = VP_PARAM_SEP;
		avp->value.set(beg,*c-(st==ST_CRLF?2:1)-beg);
		params->push_back(avp.release());
		avp.reset(new sip_avp());
		break;
	    }
	    st = saved_st;
	}
    }
    
    switch(st){

    case VP_PNAME:
	avp->name.set(beg,*c-beg);
	params->push_back(avp.release());
	break;

    case VP_PVALUE:
	avp->value.set(beg,*c-beg);
	params->push_back(avp.release());
	break;

    default:
	DBG("Incomplete via header (st=%i)\n",st);
	return MALFORMED_SIP_MSG;
    }

    return 0;
}

int parse_via(sip_via* via, char* beg, int len)
{
    enum {
	
	V_TRANS=0,
	V_URI,
	V_PARM_SEP,
	V_PARM_SEP_SWS
    };


    char* c   = beg;
    char* end = beg+len;

    int saved_st, st=V_TRANS;

    auto_ptr<sip_via_parm> parm(new sip_via_parm());

    int ret = 0;
    for(;c<end;c++){

 	switch(st){

	case V_TRANS:
 	    ret = parse_transport(&parm->trans, &c, end-c);
	    if(ret) return ret;

	    st = V_URI;
 	    break;
	    
 	case V_URI:
	    switch(*c){

	    case_CR_LF;
		
	    case SP:
	    case HTAB:
		break;

	    default:
		ret = parse_by(&parm->host,&parm->port, &c, end-c);
		if(ret) return ret;

		ret = parse_via_params(&parm->params,&c,end-c);
		if(ret) return ret;

		via->parms.push_back(parm.release());
		parm.reset(new sip_via_parm());

		st = V_PARM_SEP;
		c--;
		break;
	    }
	    break;

	case V_PARM_SEP:
	    switch(*c){

	    case_CR_LF;

	    case SP:
	    case HTAB:
		break;
		
	    case ',':
		st = V_PARM_SEP_SWS;
		break;

	    default:
		DBG("',' expected, found '%c'\n",*c);
		return MALFORMED_SIP_MSG;
	    }
	    break;

	case V_PARM_SEP_SWS:
	    switch(*c){

	    case_CR_LF;

	    case SP:
	    case HTAB:
		break;
		
	    default:
		st = V_TRANS;
		c--;
		break;
	    }
	    break;

	case_ST_CR(*c);

	case ST_LF:
	case ST_CRLF:
	    st = saved_st;
	    break;
	}
    }

    return 0;
}
