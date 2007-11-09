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

#include "hash_table.h"
#include "hash.h"

#include "log.h"

#define TABLE_POWER   10
#define TABLE_ENTRIES (1<<TABLE_POWER)

//
// Global transaction table
//
trans_bucket _trans_table[TABLE_ENTRIES];



trans_bucket::trans_bucket()
{
    pthread_mutex_init(&m,NULL);
}

trans_bucket::~trans_bucket()
{
    pthread_mutex_destroy(&m);
}

void trans_bucket::lock()
{
    pthread_mutex_lock(&m);
}

void trans_bucket::unlock()
{
    pthread_mutex_unlock(&m);
}


bool trans_bucket::is_retr(sip_msg* msg)
{
    ERROR("NYI\n");
    return false;
}

inline unsigned int hash(const cstring& ci, const cstring& cs)
{
    unsigned int h=0;

    h = hashlittle(ci.s,ci.len,h);
    h = hashlittle(cs.s,ci.len,h);

    return h & (TABLE_ENTRIES-1);
}


trans_bucket& get_trans_bucket(const cstring& callid, const cstring& cseq_num)
{
    return _trans_table[hash(callid,cseq_num)];
}
