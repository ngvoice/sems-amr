/*
 * $Id$
 *
 * Copyright (C) 2002-2003 Fhg Fokus
 * Copyright (C) 2006 iptego GmbH
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
/** @file AmPlugIn.h */
#ifndef _AmPlugIn_h_
#define _AmPlugIn_h_

#include <string>
#include <map>
#include <vector>
#include <set>
using std::string;
using std::vector;

class AmPluginFactory;
class AmSessionFactory;
class AmSessionEventHandlerFactory;
class AmDynInvokeFactory;
class AmSIPEventHandler;
class AmLoggingFacility;
class AmCtrlInterfaceFactory;

struct amci_exports_t;
struct amci_codec_t;
struct amci_payload_t;
struct amci_file_fmt_t;
struct amci_subtype_t;

/**
 * \brief Container for loaded Plug-ins.
 */
class AmPlugIn
{
 public:
  //     enum PlugInType {
  //       Audio,
  //       App
  //     };

 private:
  static AmPlugIn* _instance;

  vector<void*> dlls;

  typedef std::map<string,amci_codec_t*> Codecs;
  Codecs codecs;

  typedef std::map<int,amci_payload_t*> Payloads;
  Payloads payloads;

  typedef std::map<int,int> PayloadOrder;
  PayloadOrder payload_order;

  typedef std::map<string,amci_file_fmt_t*> FileFormats;
  FileFormats file_formats;

  typedef std::map<string,AmSessionFactory*> Name2App;
  Name2App name2app;

  typedef std::map<string,AmSessionEventHandlerFactory*> Name2Seh;
  Name2Seh name2seh;

  typedef std::map<string,AmPluginFactory*> Name2Base;
  Name2Base name2base;

  typedef std::map<string,AmDynInvokeFactory*> Name2Di;
  Name2Di name2di;

  typedef std::map<string,AmSIPEventHandler*> Name2Sipeh;
  Name2Sipeh name2sipeh;

  typedef std::map<string,AmLoggingFacility*> Name2LogFac;
  Name2LogFac name2logfac;

  AmCtrlInterfaceFactory *ctrlIface;

  int dynamic_pl; // range: 96->127, see RFC 1890
  std::set<string> excluded_payloads;  // don't load these payloads (named)
    
  AmPlugIn();
  ~AmPlugIn();

  /** @return -1 if failed, else 0. */
  int loadPlugIn(const string& file);

  int loadAudioPlugIn(amci_exports_t* exports);
  int loadAppPlugIn(AmPluginFactory* cb);
  int loadSehPlugIn(AmPluginFactory* cb);
  int loadBasePlugIn(AmPluginFactory* cb);
  int loadDiPlugIn(AmPluginFactory* cb);
  int loadSIPehPlugIn(AmPluginFactory* f);
  int loadLogFacPlugIn(AmPluginFactory* f);
  int loadCtrlFacPlugIn(AmPluginFactory* f);

  int addCodec(amci_codec_t* c);
  int addPayload(amci_payload_t* p);
  int addFileFormat(amci_file_fmt_t* f);

 public:

  static AmPlugIn* instance();

  void init();

  /** 
   * Loads all plug-ins from the directory given as parameter. 
   * @return -1 if failed, else 0.
   */
  int load(const string& directory, const string& plugins);

  /** 
   * Payload lookup function.
   * @param payload_id Payload ID.
   * @return NULL if failed .
   */
  amci_payload_t*  payload(int payload_id);
  /** @return the suported payloads. */
  const std::map<int,amci_payload_t*>& getPayloads() { return payloads; }
  /** @return the order of payloads. */
  const std::map<int,int>& getPayloadOrder() { return payload_order; }
  /** 
   * File format lookup according to the 
   * format name and/or file extension.
   * @param fmt_name Format name.
   * @param ext File extension.
   * @return NULL if failed.
   */
  amci_file_fmt_t* fileFormat(const string& fmt_name, const string& ext = "");
  /** 
   * File format's subtype lookup function.
   * @param iofmt The file format.
   * @param subtype Subtype ID (see plug-in declaration for values).
   * @return NULL if failed.
   */
  amci_subtype_t*  subtype(amci_file_fmt_t* iofmt, int subtype);
  /** 
   * Codec lookup function.
   * @param id Codec ID (see amci/codecs.h).
   * @return NULL if failed.
   */
  amci_codec_t*    codec(const char* name);
  /**
   * Application lookup function
   * @param app_name application name
   * @return NULL if failed (-> application not found).
   */
  AmSessionFactory* getFactory4App(const string& app_name);

  /**
   * Session event handler lookup function
   * @param name application name
   * @return NULL if failed (-> handler not found).
   */
  AmSessionEventHandlerFactory* getFactory4Seh(const string& name);

  /**
   * Dynamic invokation component
   */
  AmDynInvokeFactory* getFactory4Di(const string& name);

  /**
   * SIP event handler lookup function
   * @param name application name
   * @return NULL if failed (-> handler not found).
   */
  AmSIPEventHandler* getFactory4SIPeh(const string& name);

  /**
   * logging facility lookup function
   * @param name application name
   * @return NULL if failed (-> handler not found).
   */
  AmLoggingFacility* getFactory4LogFaclty(const string& name);

  /** @return true if this record has been inserted. */
  bool registerFactory4App(const string& app_name, AmSessionFactory* f);
};

#endif
