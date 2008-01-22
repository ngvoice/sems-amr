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


#ifndef _hash_table_h
#define _hash_table_h

#include "cstring.h"

#include <pthread.h>

#include <list>
using std::list;

struct sip_trans;
struct sip_msg;

class trans_bucket
{

    pthread_mutex_t m;

public:
    typedef list<sip_trans*> trans_list;

    trans_list elmts;

    trans_bucket();
    ~trans_bucket();

    void lock();
    void unlock();
    
    // Match a request to UAS transactions
    // in this bucket
    sip_trans* match_request(sip_msg* msg);

    // Match a reply to UAC transactions
    // in this bucket
    sip_trans* match_reply(sip_msg* msg);

    sip_trans* add_trans(sip_msg* msg, int ttype);

    void remove_trans(sip_trans* t);
};

trans_bucket& get_trans_bucket(const cstring& callid, const cstring& cseq_num);

unsigned int hash(const cstring& ci, const cstring& cs);

#endif
