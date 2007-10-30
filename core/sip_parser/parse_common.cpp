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


#include "parse_common.h"
#include "log.h"

#include <string.h>

//
// SIP version constants
//

#define SIPVER_len 7 // "SIP" "/" 1*DIGIT 1*DIGIT

char* SIP = "SIP";
#define SIP_len 3

char* SUP_SIPVER = "2.0";
#define SUP_SIPVER_len 3


int parse_sip_version(char* beg, int len)
{
    DBG("**parse_sip_version**\n");
    DBG("sip_version = <%.*s>\n",len,beg);

    char* c = beg;
    char* end = c+len;

    if(len!=7){
	DBG("SIP-Version string length != SIPVER_len\n");
	return MALFORMED_SIP_MSG;
    }

    if(memcmp(c,SIP,SIP_len) != 0){
	DBG("SIP-Version does not begin with \"SIP\"\n");
	return MALFORMED_SIP_MSG;
    }
    c += SIP_len;

    if(*c++ != '/'){
	DBG("SIP-Version has no \"/\" after \"SIP\"\n");
	return MALFORMED_SIP_MSG;
    }

    if(memcmp(c,SUP_SIPVER,SUP_SIPVER_len) != 0){
	DBG("Unsupported or malformed SIP-Version\n");
	return MALFORMED_SIP_MSG;
    }

    DBG("SIP-Version OK\n");
    return 0;
}

