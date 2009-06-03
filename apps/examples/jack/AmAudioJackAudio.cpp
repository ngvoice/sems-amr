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

#include "AmAudioJackAudio.h"
#include "log.h"

#define MAX_JACK_DELAY 90 // ms
#define DEF_JACK_DELAY 30 // ms
#define MIN_JACK_DELAY 20 // ms
#include <stdio.h>

AmAudioJackAudio::AmAudioJackAudio() 
  : client(NULL), jack_ts(0) {
  memset(jack_port, 0, sizeof(jack_port));
}

AmAudioJackAudio::~AmAudioJackAudio() {
  if (isOpen())
    close();
}

int jack_process_cb (jack_nframes_t nframes, void *arg) {
  //  DBG("process_cb\n");
  return static_cast<AmAudioJackAudio*>(arg)->process(nframes);
}
void jack_shutdown_cb (void *arg) {
  static_cast<AmAudioJackAudio*>(arg)->jack_shutdown();
}

#define MAX_LINEAR_SAMPLE 32737

/** jack process callback */
int AmAudioJackAudio::process (jack_nframes_t nframes) {
  if (!isOpen())
    return -1;

  if (nframes > SIZE_MIX_BUFFER) {
    ERROR("too many frames requested (%d vs. max %d\n)", nframes, SIZE_MIX_BUFFER);
    return -1;
  }

  timed_buffer_mut.lock();  
  for (int i=0;i<fmt->channels;i++) {
    jack_default_audio_sample_t *jack_buf = 
      (jack_default_audio_sample_t*) jack_port_get_buffer (jack_port[i], nframes);
    if (mode == Input) {
      // read from jack_port to buffer
      for (size_t s=0;s<nframes;s++) {
	buf[s] = (short)(jack_buf[s] * MAX_LINEAR_SAMPLE);
      }
//       DBG("[%p] put@%u %d\n", this, jack_ts, nframes);
      timed_buffer[i]->put(jack_ts, buf, nframes);
//       DBG("[%d]->put(%u, ..., %d)\n", i, jack_ts, nframes);

    } else {

      // write to jack_port from buffer      
      //     DBG("[%p] get@%u %d\n", this, jack_ts, nframes);      
      timed_buffer[i]->get(jack_ts, buf, nframes);
      for (size_t s=0;s<nframes;s++) {
	jack_buf[s] = (jack_default_audio_sample_t)buf[s] / 
	  (jack_default_audio_sample_t)MAX_LINEAR_SAMPLE;
      }    
    }
  }
  jack_ts+=nframes;
  timed_buffer_mut.unlock();  
  return 0;
}

void AmAudioJackAudio::jack_shutdown ()
{
  WARN("jack_shutdown()\n");
  client = NULL;
  for (int i=0;i<fmt->channels;i++)
    
  jack_port[i] = NULL;
}

bool AmAudioJackAudio::open(OpenMode open_mode, const char* client_name, 
			    const string& port_name) {
  const char* p_names[2];
  p_names[0] = port_name.c_str();
  p_names[1] = NULL;
  return open(open_mode, client_name, p_names);
}

bool AmAudioJackAudio::open(OpenMode open_mode, const char* client_name, 
			    const char** port_name, int channels) {
  if (channels > MAX_JACK_CHANNELS) {
    ERROR("maximum supported jack channels of one "
	  "client is %d, requested %d\n", MAX_JACK_CHANNELS, channels);
    return false;
  }  
  fmt->channels = channels;
  for (int i=0;i<channels;i++) 
    timed_buffer[i].reset(new SampleArrayShort());
  mode = open_mode;

  jack_options_t options = JackNullOption;
  jack_status_t status;
  const char* server_name = NULL;

  client = jack_client_open (client_name, options, &status, server_name);
  if (client == NULL) {
    ERROR("jack_client_open() failed, "
	     "status = 0x%2.0x\n", status);
    if (status & JackServerFailed) {
      ERROR("Unable to connect to JACK server\n");
    }
    return false;
  }
  if (status & JackServerStarted) {
    fprintf (stderr, "JACK server started\n");
  }
  if (status & JackNameNotUnique) {
    client_name = jack_get_client_name(client);
    fprintf (stderr, "unique name `%s' assigned\n", client_name);
  }

  jack_set_process_callback (client, jack_process_cb, this);

  jack_on_shutdown (client, jack_shutdown_cb, this);

  int server_sample_rate = jack_get_sample_rate (client);

  DBG("using JACK engine's sample rate: %u\n",
      server_sample_rate);

  fmt->rate = server_sample_rate;
  
  for (int i=0;i<channels;i++) {
    jack_port[i] = jack_port_register (client, port_name[i],
				       JACK_DEFAULT_AUDIO_TYPE,
				       open_mode==Input?JackPortIsInput:JackPortIsOutput, 0);
    if (jack_port[i] == NULL) {
      ERROR("no more JACK ports available\n");
      jack_client_close (client);
      client = NULL;
      return false;
    }
  }

  if (jack_activate (client)) {
    ERROR("cannot activate client\n");
    return false;
  }

  return true;

}

void AmAudioJackAudio::close() {
  if (client != NULL) {
    jack_client_close (client);
    client = NULL;
    memset(jack_port, 0, sizeof(jack_port));
  }
}

bool AmAudioJackAudio::isOpen() {
  if (client == NULL)
    return false;

  for (int i=0;i<fmt->channels;i++)
    if (NULL == jack_port[i])
      return false;

  return true;
}

string AmAudioJackAudio::getClientName() {
  if (!isOpen())
    return "";

 return jack_get_client_name(client);
}

string AmAudioJackAudio::getPortName(int index) {
  if (!isOpen() || index >= fmt->channels)
    return "";

  return jack_port_name(jack_port[index]);
}

bool AmAudioJackAudio::connectPhysical() {
  const char **ports;
  ports = jack_get_ports (client, NULL, NULL,
			  JackPortIsPhysical|
			  (mode==Input? JackPortIsOutput : JackPortIsInput));
  if (ports == NULL) {
    ERROR("no physical %s port\n", mode==Input?"output":"input");
    return false;
  }
  
  for (int i=0;i<fmt->channels && ports[i];i++) {
    if (jack_connect (client, jack_port_name (jack_port[i]), ports[i])) {
      ERROR("cannot connect %s port #%d\n", mode==Input?"output":"input", i);
      return false;
    }
  }
    
  free (ports);
  return true;
}

bool AmAudioJackAudio::connect(const char** to_ports) {  
  for (int i=0;i<fmt->channels;i++) {
    if (jack_connect (client, 
		      mode==Output?jack_port_name(jack_port[i]):to_ports[i], 
		      mode==Output?to_ports[i]:jack_port_name(jack_port[i]))) {
      ERROR("cannot connect port to '%s'\n", to_ports[i]);
      return false;
    }
  }
  return true;
}

bool AmAudioJackAudio::connect(const string& to_port) {  
  const char* p_names[2];
  p_names[0] = to_port.c_str();
  p_names[1] = NULL;

  return connect(p_names);
}

/** read audio from buffer */
int AmAudioJackAudio::read(unsigned int user_ts, unsigned int size)
{
  if (mode != Input) {
    WARN("trying to read from Output port\n");
    return -1;
  }

  if (!isOpen()) {
    WARN("trying to read from closed port\n");
    return -1;
  }

  unsigned int l_user_ts = (unsigned int)
    (user_ts * ((float)fmt->rate) / (float)SYSTEM_SAMPLERATE);

//   DBG("[%p] get@%u %d samples\n", this, l_user_ts, PCM16_B2S(size));

  timed_buffer_mut.lock();
  if( ts_less()(l_user_ts, jack_ts - MAX_JACK_DELAY * fmt->rate / 1000) || 
      !ts_less()(l_user_ts, jack_ts - MIN_JACK_DELAY * fmt->rate / 1000) ){    
    DBG("[%p] resyncing jack_ts: jack_ts = %u; read_ts = %u;  -> jack_ts = %u\n",
	this, jack_ts, l_user_ts, l_user_ts + DEF_JACK_DELAY * fmt->rate / 1000);
    jack_ts = l_user_ts + DEF_JACK_DELAY * fmt->rate / 1000;
  }
  
  // interleave samples from separate buffers
  if (PCM16_B2S(size) > SIZE_MIX_BUFFER) {
    ERROR("trying to write too many samples (%d vs %d max)\n",
	  PCM16_B2S(size), SIZE_MIX_BUFFER);
    return -1;
  }
  unsigned int chan_samples = PCM16_B2S(size)/fmt->channels;

  for (int i=0;i<fmt->channels;i++) {    
    timed_buffer[i]->get(l_user_ts,buf,chan_samples);

//     DBG("[%d]->get(%u, ..., %d)\n", i, l_user_ts, chan_samples);
    ShortSample* src = (ShortSample*)buf;
    ShortSample* dst = (ShortSample*)((unsigned char*)samples);
    dst+=i;
    for (unsigned int s=0;s<chan_samples;s++) {      
      *dst = *src;
      src++;
      dst += fmt->channels;
    }
  }
  timed_buffer_mut.unlock();  
  return size;
}

/** write audio to buffer */
int AmAudioJackAudio::write(unsigned int user_ts, unsigned int size)
{
  if (mode != Output) {
    WARN("trying to write to Input port\n");
    return -1;
  }

  if (!isOpen()) {
    WARN("trying to write to closed port\n");
    return -1;
  }

  // user_ts is in SYSTEM_SAMPLERATE space
  unsigned int l_user_ts = (unsigned int)
    (user_ts * ((float)fmt->rate / (float)SYSTEM_SAMPLERATE));

//    DBG("[%p] put@%u %d samples\n", this, l_user_ts, PCM16_B2S(size));

  timed_buffer_mut.lock();
  if( ts_less()(jack_ts, l_user_ts - MAX_JACK_DELAY * fmt->rate / 1000) || 
      !ts_less()(jack_ts, l_user_ts - MIN_JACK_DELAY * fmt->rate / 1000) ){    
    DBG("[%p] resyncing jack_ts: jack_ts = %u; write_ts = %u; -> jack_ts = %u\n",
	this, jack_ts, l_user_ts, l_user_ts - DEF_JACK_DELAY * fmt->rate / 1000);
    jack_ts = l_user_ts - DEF_JACK_DELAY * fmt->rate / 1000;
  }

  if (fmt->channels == 1) {
    timed_buffer[0]->put(l_user_ts,(ShortSample*)((unsigned char*)samples),PCM16_B2S(size));
  } else {
    // separate interleaved samples 
    if (PCM16_B2S(size) > SIZE_MIX_BUFFER) {
      ERROR("trying to write too many samples (%d vs %d max)\n",
	    PCM16_B2S(size), SIZE_MIX_BUFFER);
      return -1;
    }
    unsigned int chan_samples = PCM16_B2S(size)/fmt->channels;
    ShortSample* src = (ShortSample*)((unsigned char*)samples);
    ShortSample* dst[MAX_JACK_CHANNELS];
    for (int i=0;i<fmt->channels;i++)
      dst[i] = buf + i*chan_samples;

    for (unsigned int i=0;i<PCM16_B2S(size);i++) {
      ShortSample* c_dst = dst[i%fmt->channels];
      *c_dst = *src;
      src++;
      c_dst++;
    }

    // write to the buffers
    for (int i=0;i<fmt->channels;i++) {  
      timed_buffer[i]->put(l_user_ts,(ShortSample*)((unsigned char*)(buf + i*chan_samples)),
			   chan_samples);
    }
  }
  timed_buffer_mut.unlock();  
  return size;
}

// returns bytes read, else -1 if error (0 is OK)
int AmAudioJackAudio::get(unsigned int user_ts, unsigned char* buffer, unsigned int time_millisec)
{
  unsigned int nb_samples = time_millisec * fmt->rate / 1000;
  
  int size = calcBytesToRead(nb_samples);
  size = read(user_ts,size);

  if (size <= 0)
    return size;

  size = decode(size);
  
  if (size < 0) {
    DBG("decode returned %i\n",size);
    return -1; 
  }

  /* into internal format */
  size = downMixRate(size);

  // for stereo output we don't do down mix the channels
  //   size = downMixChannels(size);
 
  if(size>0)
    memcpy(buffer,(unsigned char*)samples,size);

  return size;
}
