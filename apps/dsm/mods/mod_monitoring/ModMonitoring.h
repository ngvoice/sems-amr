/*
 * Copyright (C) 2008 iptego GmbH
 *
 * This file is part of SEMS, a free SIP media server.
 *
 * SEMS is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version. This program is released under
 * the GPL with the additional exemption that compiling, linking,
 * and/or using OpenSSL is allowed.
 *
 * For a license to use the SEMS software under conditions
 * other than those described here, or to purchase support for this
 * software, please contact iptel.org by e-mail at the following addresses:
 *    info@iptel.org
 *
 * SEMS is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License 
 * along with this program; if not, write to the Free Software 
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */
#ifndef _MOD_SYS_H
#define _MOD_SYS_H
#include "DSMModule.h"
#include "DSMSession.h"

class MonitoringModule 
: public DSMModule {

 public:
  MonitoringModule() { }
  ~MonitoringModule() { }
  
  DSMAction* getAction(const string& from_str);
  DSMCondition* getCondition(const string& from_str);
};

DEF_ACTION_2P(MonLogAction);
DEF_ACTION_2P(MonLogAddAction);
DEF_ACTION_1P(MonLogVarsAction);

DEF_ACTION_2P(MonLogGlobalAction);
DEF_ACTION_2P(MonLogAddGlobalAction);

DEF_ACTION_2P(MonLogIncAction);
DEF_ACTION_2P(MonLogDecAction);

#endif
