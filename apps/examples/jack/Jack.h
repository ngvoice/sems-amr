/*
 * $Id$
 *
 * Copyright (C) 2009 IPTEGO GmbH
 *
 * This file is part of SEMS, a free SIP media server.
 *
 * SEMS is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
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

#ifndef _JACK_H_
#define _JACK_H_

#include "AmSession.h"
#include "AmAudioJackAudio.h"
#include "AmConfigReader.h"

#include "ampi/UACAuthAPI.h"

#include <string>
using std::string;

#include <map>
using std::map;

#include <memory>

/** \brief Factory for announcement sessions */
class JackFactory: public AmSessionFactory
{
  inline string getAnnounceFile(const AmSipRequest& req);
public:

  JackFactory(const string& _app_name);

  int onLoad();
  AmSession* onInvite(const AmSipRequest& req);
  AmSession* onInvite(const AmSipRequest& req,
		      AmArg& session_params);
};

/**\brief  announcement session logic implementation */
class JackDialog : public AmSession,
		   public CredentialHolder
{
  AmAudioJackAudio jack_dev_in;
  AmAudioJackAudio jack_dev_out;

  std::auto_ptr<UACAuthCred> cred;

public:
  JackDialog(UACAuthCred* credentials = NULL);
  ~JackDialog();

  void onSessionStart(const AmSipRequest& req);
  void onSessionStart(const AmSipReply& rep);
  void startSession();
  void onBye(const AmSipRequest& req);
  void onDtmf(int event, int duration_msec) {}

  void process(AmEvent* event);

  UACAuthCred* getCredentials();
};

#endif
// Local Variables:
// mode:C++
// End:

