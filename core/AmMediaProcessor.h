/*
 * Copyright (C) 2002-2003 Fhg Fokus
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
/** @file AmMediaProcessor.h */
#ifndef _AmMediaProcessor_h_
#define _AmMediaProcessor_h_

#include "AmEventQueue.h"
#include "amci/amci.h" // AUDIO_BUFFER_SIZE

#include <set>
using std::set;
#include <map>

struct SchedRequest;

class AmMediaSession
{
  private:
    AmCondition<bool> processing_media;

  public:
    AmMediaSession(): processing_media(false) { }

    /* first read from all RTP streams */
    virtual int readStreams(unsigned int ts, unsigned char *buffer) = 0;
    
    /* after read write to all RTP streams */
    virtual int writeStreams(unsigned int ts, unsigned char *buffer) = 0;

    virtual void processDtmfEvents() = 0;
    virtual void clearAudio() = 0;
    virtual void clearRTPTimeout() = 0;

    virtual ~AmMediaSession() { }

    void onMediaProcessingStarted() { processing_media.set(true); }
    void onMediaProcessingTerminated() { processing_media.set(false); }
  
    /** Is the session being processed in  media processor? */
    bool getProcessingMedia() { return processing_media.get(); }
  
    /** Is the session detached from media processor? */
    bool getDetached() { return !processing_media.get(); }
};

/**
 * \brief Media processing thread
 * 
 * This class implements a media processing thread.
 * It processes the media and triggers the sending of RTP
 * of all sessions added to it.
 */
class AmMediaProcessorThread :
  public AmThread,
  public AmEventHandler
{
  AmEventQueue    events;
  unsigned char   buffer[AUDIO_BUFFER_SIZE];
  set<AmMediaSession*> sessions;
  
  void processAudio(unsigned int ts);
  /**
   * Process pending DTMF events
   */
  void processDtmfEvents();

  // AmThread interface
  void run();
  void on_stop();
  AmSharedVar<bool> stop_requested;
    
  // AmEventHandler interface
  void process(AmEvent* e);
public:
  AmMediaProcessorThread();
  ~AmMediaProcessorThread();

  inline void postRequest(SchedRequest* sr);
  
  unsigned int getLoad();
};

/**
 * \brief Media processing thread manager
 * 
 * This class implements the manager that assigns and removes 
 * the Sessions to the various \ref MediaProcessorThreads, 
 * according to their call group. This class contains the API 
 * for the MediaProcessor.
 */
class AmMediaProcessor
{
  static AmMediaProcessor* _instance;

  unsigned int num_threads; 
  AmMediaProcessorThread**  threads;
  
  std::map<string, unsigned int> callgroup2thread;
  std::multimap<string, AmMediaSession*> callgroupmembers;
  std::map<AmMediaSession*, string> session2callgroup;
  AmMutex group_mut;

  AmMediaProcessor();
  ~AmMediaProcessor();
	
  void removeFromProcessor(AmMediaSession* s, unsigned int r_type);
public:
  /** 
   * InsertSession     : inserts the session to the processor
   * RemoveSession     : remove the session from the processor
   * SoftRemoveSession : remove the session from the processor but leave it attached
   * ClearSession      : remove the session from processor and clear audio
   */
  enum { InsertSession, RemoveSession, SoftRemoveSession, ClearSession };

  static AmMediaProcessor* instance();

  void init();
  /** Add session s to processor */
  void addSession(AmMediaSession* s, const string& callgroup);
  /** Remove session s from processor */
  void removeSession(AmMediaSession* s);
  /** Remove session s from processor and clear its audio */
  void clearSession(AmMediaSession* s);
  /** Change the callgroup of a session (use with caution) */
  void changeCallgroup(AmMediaSession* s, 
		       const string& new_callgroup);

  void stop();
  static void dispose();
};


#endif






