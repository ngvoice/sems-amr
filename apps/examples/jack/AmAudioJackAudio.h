/*
 * $Id$
 *
 * Copyright (C) 2002-2003 Fhg Fokus
 *
 * This file is part of sems, a free SIP media server.
 *
 * sems is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * For a license to use the sems software under conditions
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

#ifndef _AmAudioJackAudio_h_
#define _AmAudioJackAudio_h_

#include "AmAudio.h"
#include "SampleArray.h"

#include <jack/jack.h>

#define MAX_JACK_CHANNELS 36 // maximum jack channels one client can have - usually 2 is enough


/** \brief audio device that connects to jack port */
class AmAudioJackAudio : public AmAudio
{
 public:
  enum OpenMode { Input=1, Output=2 };

 private:
  auto_ptr<SampleArrayShort> timed_buffer[MAX_JACK_CHANNELS];
  ShortSample buf[SIZE_MIX_BUFFER];
  AmMutex timed_buffer_mut;

  jack_client_t* client;
  jack_port_t* jack_port[MAX_JACK_CHANNELS];

  unsigned int jack_ts;

  OpenMode mode;

  int read(unsigned int user_ts, unsigned int size);
  int write(unsigned int user_ts, unsigned int size);

public:
  AmAudioJackAudio();
  ~AmAudioJackAudio();

  bool open(OpenMode open_mode, const char* client_name, 
	    const string& port_name);
  bool open(OpenMode open_mode, const char* client_name, 
	    const char** port_name, int channels = 1);
  void close();
  bool isOpen();

  bool connectPhysical();
  bool connect(const string& to_port);
  bool connect(const char** to_ports);
  string getPortName(int index = 0);
  string getClientName();

  // callbacks
  void jack_shutdown ();
  int process (jack_nframes_t nframes);

  int get(unsigned int user_ts, unsigned char* buffer, unsigned int time_millisec);
};

#endif

