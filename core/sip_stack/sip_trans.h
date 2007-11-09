/*
 * $Id: parse_cseq.h 549 2007-11-07 16:12:10Z rco $
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

enum {

    //
    // Transaction types
    //

    TT_UAS,
    TT_UAC
};


enum {

    //
    // UAS Transaction states
    //
    TS_TRYING,     // !INV
    TS_PROCEEDING, // INV, !INV
    TS_COMPLETED,  // INV, !INV
    TS_CONFIRMED,  // INV
    TS_TERMINATED  // INV, !INV
};


struct sip_trans
{
    // Transaction type
    int type; 
    
    // Depending on type, this
    // could be a request or a reply
    sip_msg* msg;
};

#endif
