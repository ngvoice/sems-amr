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

#include "Jack.h"
#include "AmConfig.h"
#include "AmUtils.h"
#include "AmPlugIn.h"

#include "sems.h"
#include "log.h"

#define MOD_NAME "jack"

EXPORT_SESSION_FACTORY(JackFactory,MOD_NAME);

JackFactory::JackFactory(const string& _app_name)
  : AmSessionFactory(_app_name)
{
}

int JackFactory::onLoad()
{
  return 0;
}


AmSession* JackFactory::onInvite(const AmSipRequest& req)
{
  return new JackDialog(NULL);
}

AmSession* JackFactory::onInvite(const AmSipRequest& req,
					 AmArg& session_params)
{
  UACAuthCred* cred = NULL;
  if (session_params.getType() == AmArg::AObject) {
    ArgObject* cred_obj = session_params.asObject();
    if (cred_obj)
      cred = dynamic_cast<UACAuthCred*>(cred_obj);
  }

  AmSession* s = new JackDialog(cred); 
  
  if (NULL == cred) {
    WARN("discarding unknown session parameters.\n");
  } else {
    AmSessionEventHandlerFactory* uac_auth_f = 
      AmPlugIn::instance()->getFactory4Seh("uac_auth");
    if (uac_auth_f != NULL) {
      DBG("UAC Auth enabled for new announcement session.\n");
      AmSessionEventHandler* h = uac_auth_f->getHandler(s);
      if (h != NULL )
	s->addHandler(h);
    } else {
      ERROR("uac_auth interface not accessible. "
	    "Load uac_auth for authenticated dialout.\n");
    }		
  }

  return s;
}

JackDialog::JackDialog(UACAuthCred* credentials)
  : cred(credentials)
{
}

JackDialog::~JackDialog()
{
}

void JackDialog::onSessionStart(const AmSipRequest& req)
{
  DBG("JackDialog::onSessionStart\n");
  startSession();
}

void JackDialog::onSessionStart(const AmSipReply& rep)
{
  DBG("JackDialog::onSessionStart (SEMS originator mode)\n");
  startSession();
}

void JackDialog::startSession(){

  const char*  in_names[] = {"in_port", "in_port2"};
  if (!jack_dev_in.open(AmAudioJackAudio::Input, "sems_in", in_names, 2)) {
    ERROR("opening jack in device (2 channels)\n");
    return;
  }

  if (!jack_dev_out.open(AmAudioJackAudio::Output, "sems_out", "out_port")) {
    ERROR("opening jack out device\n");
    return;
  }

  INFO("out name is %s:%s\n",jack_dev_out.getClientName().c_str(), 
       jack_dev_out.getPortName().c_str());

  INFO("in name is %s:%s\n",jack_dev_in.getClientName().c_str(), 
       jack_dev_in.getPortName().c_str());

  // connect output (mono) to both left and right channel
  if (!jack_dev_out.connect("system:playback_1")) {
    ERROR("connecting output to pl_1\n");
  }

  if (!jack_dev_out.connect("system:playback_2")) {
    ERROR("connecting output to in_1\n");
  }

  // connect stereo input to both our two channels
  const char* system_out_names[] = {"system:capture_1", "system:capture_2"};
  if (!jack_dev_in.connect(system_out_names)) {
    ERROR("connecting input to system:capture_X\n");
  }

  DBG("setting in/out\n");
  setInput(&jack_dev_out);
  setOutput(&jack_dev_in);
}

void JackDialog::onBye(const AmSipRequest& req)
{
  DBG("onBye: stopSession\n");
  setStopped();
}


void JackDialog::process(AmEvent* event)
{

  AmAudioEvent* audio_event = dynamic_cast<AmAudioEvent*>(event);
  if(audio_event && (audio_event->event_id == AmAudioEvent::cleared)){
    dlg.bye();
    setStopped();
    return;
  }

  AmSession::process(event);
}

inline UACAuthCred* JackDialog::getCredentials() {
  return cred.get();
}
