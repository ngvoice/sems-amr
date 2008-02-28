/*
 * $Id: AmFileCache.h 145 2006-11-26 00:01:18Z sayer $
 *
 * Copyright (C) 2007 iptego GmbH
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
/** @file AmCachedAudioFile.h */
#ifndef _AMFILECACHE_H
#define _AMFILECACHE_H

#include "AmAudioFile.h"

#include <string>

/**
 * \brief memory cache for AmAudioFile 
 * 
 * The MemStream class loads a file once into memory 
 * to be used e.g. by AmCachedAudioFile.
 */
class MemStream: public BitStream
{
  int fd;
  unsigned char*  data;
  unsigned char*  cursor;

  size_t data_size;
  std::string name;

 public:
  MemStream();
  ~MemStream();

  /** load filename into memory 
   * @return 0 if everything's OK 
   */
  int load(const std::string& filename);
  /** get the size of the file */
  size_t getSize();
  /** get the filename */
  const string& getFilename();
  /** get a pointer to the file's data - use with caution! */
  void* getData() { return data; }

  //
  // BitStream interface
  //
  int read(void* buf, int len);
  int write(void* buf, int len);
  int seek(long p);
  long pos();
  int close();
};

/**
 * \brief AmAudio implementation for cached file
 *  
 *  This uses an AmFileCache instance to read the data 
 *  rather than a file. 
 */
class AmCachedAudioFile 
: public AmAudio
{
    MemStream* cache;
    /** beginning of data in file */
    size_t begin; 
    bool good;
    
    /** @see AmAudio::read */
    int read(unsigned int user_ts, unsigned int size);
    
    /** @see AmAudio::write */
    int write(unsigned int user_ts, unsigned int size);
    
    /** get the file format from the file name */
    amci_file_fmt_t* fileName2Fmt(const string& name);

    /** Format of that file. @see fp, open(). */
    amci_file_fmt_t* file_fmt;

 public:
  AmCachedAudioFile(MemStream* cache);
  ~AmCachedAudioFile();

  /** loop the file? */
  AmSharedVar<bool> loop;

  /**
   * Rewind the file.
   */
  void rewind();

  /** Closes the file. */
  void close();

  /** everything ok? */	
  bool is_good() { return good; }
};
#endif //_AMFILECACHE_H

// Local Variables:
// mode:C++
// End:
