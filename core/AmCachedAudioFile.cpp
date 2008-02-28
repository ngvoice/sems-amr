/*
 * $Id: AmFileCache.cpp 145 2006-11-26 00:01:18Z sayer $
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

#include "AmCachedAudioFile.h"
#include "AmUtils.h"
#include "log.h"
#include "AmPlugIn.h"

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <string.h>
#include <sys/mman.h>



using std::string;

MemStream::MemStream() 
    : data(NULL),fd(0),
      cursor(NULL),
      data_size(0)
{ }

MemStream::~MemStream() 
{
    if ((data != NULL) &&
	munmap(data, data_size)) {
	ERROR("while unmapping file.\n");
    }

    if(fd != 0)
	::close(fd);
}

int MemStream::load(const std::string& filename) 
{
  struct stat sbuf;

  name = filename;

  if ((fd = open(name.c_str(), O_RDONLY)) == -1) {
    ERROR("while opening file '%s' for caching.\n", 
	  filename.c_str());
    return -1;
  }

  if (fstat(fd,  &sbuf) == -1) {
    ERROR("cannot stat file '%s'.\n", 
	  name.c_str());
    return -2;
  }
  
  data = (unsigned char*)mmap((void*)0, sbuf.st_size, 
			      PROT_READ, MAP_PRIVATE, fd, 0);

  if ((void*)(data) == (void*)(-1)) {
    ERROR("cannot mmap file '%s'.\n", 
	  name.c_str());
    return -3;
  }

  data_size = sbuf.st_size;
  cursor = data;

  return 0;
}

//   int read(void* buf, int len);
//   int write(void* buf, int len);
//   int seek(long p);
//   long pos();
//   int close();

int MemStream::read(void* buf,int len) 
{
    if (cursor >= data + data_size)
	return -1; // eof

    size_t r_len = len;
    if (cursor + len > data + data_size)
	r_len = data + data_size - cursor;
    
    memcpy(buf, cursor, r_len);
    cursor += r_len;
    
    return r_len;
}

int MemStream::write(void* buf, int len)
{
    if (cursor >= data + data_size)
	return -1; // eof

    size_t r_len = len;
    if (cursor + len > data + data_size)
	r_len = data + data_size - cursor;
    
    memcpy(buf, cursor, r_len);
    cursor += r_len;
    
    return r_len;
}

int MemStream::seek(long p)
{
    if(p < 0){
	cursor = data + data_size;
	return 0;
    }

    if ((unsigned long)p > data_size)
	return -1;

    cursor = data + p;
    return 0;
}

long MemStream::pos()
{
    return (long)(cursor - data);
}

int MemStream::close()
{
    return 0;
}

inline size_t MemStream::getSize() {
  return data_size;
}

inline const string& MemStream::getFilename() {
  return name;
}


AmCachedAudioFile::AmCachedAudioFile(MemStream* cache) 
  : cache(cache), loop(false), begin(0), good(false)
{
  if (!cache) {
    ERROR("Need open file cache.\n");
    return;
  }

  amci_file_fmt_t* f_fmt = fileName2Fmt(cache->getFilename());
  if(!f_fmt){
      ERROR("while trying to determine the format of '%s'\n",
	    cache->getFilename().c_str());
      return;
  }
	
  amci_file_desc_t fd;
  memset(&fd, 0, sizeof(amci_file_desc_t));

  assert(f_fmt->open);
  if( (*f_fmt->open)(cache,AMCI_RDONLY,&fd) != 0 ) {

      ERROR("file format plugin open function failed\n");
      close();
      return;
  }
  
  file_fmt = f_fmt;
    
  fmt.reset(new AmAudioFileFormat(file_fmt->name,&fd));

  begin = cache->pos();
  good = true;

  return;
}

AmCachedAudioFile::~AmCachedAudioFile() {
}

amci_file_fmt_t* AmCachedAudioFile::fileName2Fmt(const string& name)
{
  string ext = file_extension(name);
  if(ext == ""){
    ERROR("fileName2Fmt: file name has no extension (%s)",name.c_str());
    return NULL;
  }

  amci_file_fmt_t* f_fmt = AmPlugIn::instance()->fileFormat("",ext);
  if(!f_fmt){
      ERROR("fileName2Fmt: could not find a format with that extension: '%s'",ext.c_str());
      return NULL;
  }

  return f_fmt;
}

void AmCachedAudioFile::rewind() 
{
    cache->seek(begin);
}

/** Closes the file. */
void AmCachedAudioFile::close() 
{
}

int AmCachedAudioFile::read(unsigned int user_ts, unsigned int size) 
{
    if(!good){
	ERROR("AmAudioFile::read: file is not opened\n");
	return -1;
    }
    
    int ret = cache->read((unsigned char*)samples,size);
    
    if(loop.get() && (ret <= 0) && cache->pos()==(long)cache->getSize()){
	DBG("rewinding audio file...\n");
	rewind();
	ret = cache->read((unsigned char*)samples,size);
    }
    
    if(ret > 0 && (unsigned int)ret < size){
	DBG("0-stuffing packet: adding %i bytes (packet size=%i)\n",size-ret,size);
	memset((unsigned char*)samples + ret,0,size-ret);
	return size;
    }
    
    return (cache->pos()==(long)cache->getSize() && !loop.get() ? -2 : ret);
}

int AmCachedAudioFile::write(unsigned int user_ts, unsigned int size) 
{
    ERROR("AmCachedAudioFile writing not supported!\n");
    return -1;
}

