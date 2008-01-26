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

#ifndef _sip_trans_h
#define _sip_trans_h

#include "cstring.h"

struct sip_msg;

enum {

    //
    // Transaction types
    //

    TT_UAS=1,
    TT_UAC
};


enum {

    //
    // Transaction states
    //

    TS_TRYING,     // UAC:!INV;     UAS:!INV
    TS_CALLING,    // UAC:INV
    TS_PROCEEDING, // UAC:INV,!INV; UAS:INV,!INV
    TS_COMPLETED,  // UAC:INV,!INV; UAS:INV,!INV
    TS_CONFIRMED,  //               UAS:INV
    TS_TERMINATED, // UAC:INV,!INV; UAS:INV,!INV

    TS_REMOVED
};


struct sip_trans
{
    // Transaction type
    int type;
    
    // Request that initiated 
    //  the transaction
    sip_msg* msg;

    // To-tag included in reply.
    // (useful for ACK matching)
    cstring to_tag;

    // reply code of last
    // sent/received reply
    int reply_status;

    // Transaction state
    int state;

    // Retransmission buffer
    //  - UAS transaction: ACK
    //  - UAC transaction: final reply
    char* retr_buf;

    // Length of the retransmission buffer
    int   retr_len;
    
    sip_trans()
	: msg(0),
	 retr_buf(0),
	 retr_len(0)
    {}

    ~sip_trans() {
	delete msg;
	delete [] retr_buf;
	if(type == TT_UAC)
	    delete [] to_tag.s;
    }
};

#endif
