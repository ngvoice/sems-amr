/*
 * $Id: AmAudio.cpp 633 2008-01-28 18:17:36Z sayer $
 *
 * Copyright (C) 2002-2003 Fhg Fokus
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

#include "AmAudioFile.h"
#include "AmPlugIn.h"
#include "AmUtils.h"
#include "compat/solaris.h"

#include <string.h>

int FileStream::read(void* buf, int len)
{
    int ret = fread(buf,1,len,fptr);
    return (ferror(fptr)? -1 : ret);
}

int FileStream::write(void* buf, int len)
{
    fwrite(buf,1,len,fptr);
    return (ferror(fptr)? -1 : 0);
}

int FileStream::seek(long p)
{
    if(p<0){
	fseek(fptr,0,SEEK_END);
    }
    else {
	fseek(fptr,p,SEEK_SET);
    }

    return (ferror(fptr) ? -1 : 0);
}

long FileStream::pos()
{
    return ftell(fptr);
}

int FileStream::close()
{
    return fclose(fptr);
}

// AmAudioFileFormat::AmAudioFileFormat(const string& name)
//   : name(name)
// {
// }

AmAudioFileFormat::AmAudioFileFormat(const char* f_name, const amci_file_desc_t* fd)
{
    name       = f_name;

    codec_name = (const char*)fd->codec;
    rate       = fd->rate;
    channels   = fd->channels;

    // TODO: frame_length
    // TODO: frame_size
    // TODO: frame_encoded_size
}

amci_file_fmt_t* AmAudioFile::fileName2Fmt(const string& name)
{
    string ext = file_extension(name);
    if(ext == ""){
	ERROR("fileName2Fmt: file name has no extension (%s)\n",name.c_str());
	return NULL;
    }
    
    amci_file_fmt_t* f_fmt = AmPlugIn::instance()->fileFormat("",ext);
    if(!f_fmt){
	ERROR("fileName2Fmt: could not find a format with that extension: '%s'\n",ext.c_str());
	return NULL;
    }
    
    return f_fmt;
}


// returns 0 if everything's OK
// return -1 if error
int  AmAudioFile::open(const string& filename, OpenMode mode, bool is_tmp)
{
  close();

  this->close_on_exit = true;

  FILE* n_fp = NULL;

  if(!is_tmp){
    n_fp = fopen(filename.c_str(),mode == AmAudioFile::Read ? "r" : "w+");
    if(!n_fp){
      if(mode == AmAudioFile::Read)
	ERROR("file not found: %s\n",filename.c_str());
      else
	ERROR("could not create/overwrite file: %s\n",filename.c_str());
      return -1;
    }
  } else {	
    n_fp = tmpfile();
    if(!n_fp){
      ERROR("could not create temporary file: %s\n",strerror(errno));
      return -1;
    }
  }

  return fpopen_int(filename, mode, n_fp);
}

int AmAudioFile::fpopen(const string& filename, OpenMode mode, FILE* n_fp)
{
  close();
  return fpopen_int(filename, mode, n_fp);
}

int AmAudioFile::fpopen_int(const string& filename, OpenMode mode, FILE* n_fp)
{
    amci_file_fmt_t* f_fmt = fileName2Fmt(filename);
    if(!f_fmt){
	ERROR("while trying to determine the format of '%s'\n",filename.c_str());
	return -1;
    }

    open_mode = mode;

    fp = new FileStream(n_fp);
    fp->seek(0L);

    amci_file_desc_t fd;
    memset(&fd, 0, sizeof(amci_file_desc_t));

    if (mode == AmAudioFile::Write) {
	
	// select default codec
	if(!f_fmt->subtypes || f_fmt->subtypes[0]) {
	    ERROR("file format has no compatible codecs defined\n");
	    return -1;
	}
	
	fd.codec = f_fmt->subtypes[0];
    }
    
    assert(f_fmt->open);

    if( (*f_fmt->open)(fp,mode,&fd) != 0 ) {

	ERROR("file format plugin open function failed\n");
	close();
	return -1;
    }

    file_fmt = f_fmt;
    data_size = fd.data_size;
   
    DBG("before fmt.reset(): fmt.get() == %p\n",fmt.get());
    fmt.reset(new AmAudioFileFormat(file_fmt->name,&fd));
    DBG("after fmt.reset()\n");
    setBufferSize(fd.buffer_size, fd.buffer_thresh, fd.buffer_full_thresh);

    begin = fp->pos();

    return 0;
}


AmAudioFile::AmAudioFile()
  : AmBufferedAudio(0, 0, 0), data_size(0), 
    fp(0), begin(0), loop(false),
    on_close_done(false),
    close_on_exit(true)
{
}

AmAudioFile::~AmAudioFile()
{
  close();
}

void AmAudioFile::rewind()
{
    fp->seek(begin);
    //fseek(fp,begin,SEEK_SET);
    clearBufferEOF();
}

void AmAudioFile::on_close()
{
  if(fp && !on_close_done){

    AmAudioFileFormat* f_fmt = 
      dynamic_cast<AmAudioFileFormat*>(fmt.get());

    if(f_fmt){
	amci_file_desc_t fmt_desc = {
	    fmt->getCodecName(),
	    f_fmt->rate, 
	    f_fmt->channels, 
	    data_size 
	};
	    
	if(!file_fmt){
	    ERROR("file format pointer not initialized: on_close will not be called\n");
	}
	else if(file_fmt->on_close)
	    (*file_fmt->on_close)(fp,open_mode,&fmt_desc);
    }

    if(open_mode == AmAudioFile::Write){

      DBG("After close:\n");
      DBG("fmt::subtype = %s\n",f_fmt->getCodecName().c_str());
      DBG("fmt::channels = %i\n",f_fmt->channels);
      DBG("fmt::rate = %i\n",f_fmt->rate);
    }

    on_close_done = true;
  }
}


void AmAudioFile::close()
{
  if(fp){
    on_close();

    if(close_on_exit)
      fp->close();

    delete fp;
    fp = 0;
  }
}

string AmAudioFile::getMimeType()
{
  if(!file_fmt)
    return "";
    
  return file_fmt->mime_type;
}


int AmAudioFile::read(unsigned int user_ts, unsigned int size)
{
  if(!fp){
    ERROR("AmAudioFile::read: file is not opened\n");
    return -1;
  }

  int ret;
  int s = size;

 read_block:
  long fpos  = fp->pos();
  if(data_size < 0 || fpos - begin < data_size){
    
    if((data_size > 0) && (fpos - begin + (int)size > data_size)){
      s = data_size - fpos + begin;
    }
    
    s = fp->read((void*)((unsigned char*)samples),s);
    
    ret = s; //(!ferror(fp) ? s : -1);
    
#if (defined(__BYTE_ORDER) && (__BYTE_ORDER == __BIG_ENDIAN))
#define bswap_16(A)  ((((u_int16_t)(A) & 0xff00) >> 8) | \
		      (((u_int16_t)(A) & 0x00ff) << 8))
    
    unsigned int i;
    for(i=0;i<=size/2;i++) {
      ((u_int16_t *)((unsigned char*)samples))[i]=bswap_16(((u_int16_t *)((unsigned char*)samples))[i]);
    }
    
#endif
  }
  else {
    if(loop.get() && data_size>0){
      DBG("rewinding audio file...\n");
      rewind();
      goto read_block;
      }
    
    DBG("data_size = %i\n",data_size);
    DBG("fpos = %li\n",fpos);
    DBG("begin = %li\n",begin);
    ret = -2; // eof
  }

  if(ret > 0 && s > 0 && (unsigned int)s < size){
    DBG("0-stuffing packet: adding %i bytes (packet size=%i)\n",size-s,size);
    memset((unsigned char*)samples + s,0,size-s);
    return size;
  }

  return ret;
}

int AmAudioFile::write(unsigned int user_ts, unsigned int size)
{
  if(!fp){
    ERROR("AmAudioFile::write: file is not opened\n");
    return -1;
  }

  int s = fp->write((void*)((unsigned char*)samples),size);
  if(s>0)
    data_size += s;
  return s;//(!ferror(fp) ? s : -1);
}

int AmAudioFile::getLength() 
{ 
  if (!data_size || !fmt.get())
    return 0;

  return 
    fmt->bytes2samples(data_size) /
    (fmt->rate/1000); 
}
