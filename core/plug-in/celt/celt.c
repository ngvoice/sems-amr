/*
  This is a simple interface to the CELT library for SEMS.

  This is experimental code! It does not even comply to 
   http://tools.ietf.org/html/draft-valin-celt-rtp-profile-01

  see http://www.celt-codec.org,
      http://www.ietf.org/internet-drafts/draft-valin-celt-codec-00.txt

  (C) 2009 iptego GmbH

  This code is free software; you can redistribute it and/or
  modify it under the terms of the GNU Lesser General Public
  License as published by the Free Software Foundation.
  
  This code is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
  Lesser General Public License for more details.
  
  You should have received a copy of the GNU Lesser General Public
  License along with this library; if not, write to the Free Software
  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
*/

#include <string.h>
#include <stdlib.h>

#include "amci.h"
#include "codecs.h"
#include "celt/celt.h"
#include "../../log.h"

int Pcm16_2_CELT( unsigned char* out_buf, unsigned char* in_buf, unsigned int size,
		     unsigned int channels, unsigned int rate, long h_codec );
int CELT_2_Pcm16( unsigned char* out_buf, unsigned char* in_buf, unsigned int size,
		     unsigned int channels, unsigned int rate, long h_codec );

/* this sucks big time... */
long CELT32_create(const char* format_parameters, amci_codec_fmt_info_t* format_description);
long CELT44_create(const char* format_parameters, amci_codec_fmt_info_t* format_description);
long CELT48_create(const char* format_parameters, amci_codec_fmt_info_t* format_description);
long CELT32_2_create(const char* format_parameters, amci_codec_fmt_info_t* format_description);
long CELT44_2_create(const char* format_parameters, amci_codec_fmt_info_t* format_description);
long CELT48_2_create(const char* format_parameters, amci_codec_fmt_info_t* format_description);
void CELT_destroy(long handle);

static unsigned int CELT_bytes2samples(long, unsigned int);
static unsigned int CELT_samples2bytes(long, unsigned int);

BEGIN_EXPORTS("celt", AMCI_NO_MODULEINIT, AMCI_NO_MODULEDESTROY)

BEGIN_CODECS
CODEC(CODEC_CELT32, Pcm16_2_CELT, CELT_2_Pcm16, AMCI_NO_CODEC_PLC, 
      CELT32_create, CELT_destroy, 
      CELT_bytes2samples, CELT_samples2bytes)
CODEC(CODEC_CELT44, Pcm16_2_CELT, CELT_2_Pcm16, AMCI_NO_CODEC_PLC, 
      CELT44_create, CELT_destroy, 
      CELT_bytes2samples, CELT_samples2bytes)
CODEC(CODEC_CELT48, Pcm16_2_CELT, CELT_2_Pcm16, AMCI_NO_CODEC_PLC, 
      CELT48_create, CELT_destroy, 
      CELT_bytes2samples, CELT_samples2bytes)
END_CODECS
  

BEGIN_PAYLOADS
#if SYSTEM_SAMPLERATE >=32000

PAYLOAD(-1, "CELT", 32000, 32000, 1, CODEC_CELT32, AMCI_PT_AUDIO_FRAME)

#ifdef SUPPORT_STEREO
PAYLOAD(-1, "CELT", 32000, 32000, 2, CODEC_CELT32_2, AMCI_PT_AUDIO_FRAME)
#endif

#if SYSTEM_SAMPLERATE >=44100
PAYLOAD(-1, "CELT", 44100, 44100, 1, CODEC_CELT44, AMCI_PT_AUDIO_FRAME)

#ifdef SUPPORT_STEREO
PAYLOAD(-1, "CELT", 44100, 44100, 2, CODEC_CELT44_2, AMCI_PT_AUDIO_FRAME)
#endif

#if SYSTEM_SAMPLERATE >=48000
PAYLOAD(-1, "CELT", 48000, 48000, 1, CODEC_CELT48, AMCI_PT_AUDIO_FRAME)

#ifdef SUPPORT_STEREO
PAYLOAD(-1, "CELT", 48000, 48000, 2, CODEC_CELT48_2, AMCI_PT_AUDIO_FRAME)
#endif

#endif
#endif
#endif


END_PAYLOADS
  
BEGIN_FILE_FORMATS
END_FILE_FORMATS

END_EXPORTS


#define PCM16_B2S(b) ((b) >> 1)
#define PCM16_S2B(s) ((s) << 1)

typedef struct {
  CELTMode *celt_mode;

  CELTEncoder* encoder;
  CELTDecoder* decoder;

  int rate;
  int channels;
  int frame_size;   /* in samples */
  int encoded_size; /* in bytes */
} CeltState;

#define BITRATE 128000
#define FRAME_MS 10             /* 10 ms frame size */

long CELT_create(unsigned int rate, unsigned int channels, 
		   const char* format_parameters, amci_codec_fmt_info_t* format_description) {
  CeltState* cs = (CeltState*) calloc(1, sizeof(CeltState));
    
  if (!cs)
    return -1;

  cs->rate = rate;
  cs->channels = channels;
  cs->frame_size = FRAME_MS * rate / 1000;
  cs->celt_mode = celt_mode_create(rate, channels, cs->frame_size, NULL);

  if (!cs->celt_mode) {
    ERROR("creating celt mode!\n");
    return -1;
  }

  /* stolen from baresip */
  celt_mode_info(cs->celt_mode, CELT_GET_FRAME_SIZE, &cs->frame_size);  
  cs->encoded_size = (BITRATE * cs->frame_size / rate + 4)/8;

  DBG("CELTxy_create: rate=%u, channels=%u, frame_size=%u, encoded_size=%u\n", 
      rate, channels, cs->frame_size, cs->encoded_size);

  cs->encoder = celt_encoder_create(cs->celt_mode);
  cs->decoder = celt_decoder_create(cs->celt_mode);

  if (!cs->encoder || !cs->decoder) {
    ERROR("creating celt encoder/decoder pair!\n");
    return -1;
  }
  
  format_description[0].id = AMCI_FMT_FRAME_LENGTH;
  format_description[0].value = FRAME_MS;
    
  format_description[1].id = AMCI_FMT_FRAME_SIZE;
  format_description[1].value = cs->frame_size;
    
  DBG("set AMCI_FMT_FRAME_LENGTH to %d\n", format_description[0].value);
  DBG("set AMCI_FMT_FRAME_SIZE to %d\n", format_description[1].value);
    
  format_description[2].id = 0;
    
  DBG("CELTState %p inserted with rate %d\n", cs, cs->rate);

  return (long)cs;
}

long CELT32_create(const char* format_parameters, amci_codec_fmt_info_t* format_description)
{
  return CELT_create(32000, 1, format_parameters, format_description);
}

long CELT44_create(const char* format_parameters, amci_codec_fmt_info_t* format_description)
{
  return CELT_create(44100, 1, format_parameters, format_description);
}

long CELT48_create(const char* format_parameters, amci_codec_fmt_info_t* format_description)
{
  return CELT_create(48000, 1, format_parameters, format_description);
}

long CELT32_2_create(const char* format_parameters, amci_codec_fmt_info_t* format_description)
{
  return CELT_create(32000, 2, format_parameters, format_description);
}

long CELT44_2_create(const char* format_parameters, amci_codec_fmt_info_t* format_description)
{
  return CELT_create(44100, 2, format_parameters, format_description);
}

long CELT48_2_create(const char* format_parameters, amci_codec_fmt_info_t* format_description)
{
  return CELT_create(48000, 2, format_parameters, format_description);
}

void CELT_destroy(long handle)
{
  CeltState* cs = (CeltState*) handle;
    
  DBG("CELT_destroy for handle %ld\n", handle);
    
  if (!cs)
    return;
   
  if (cs->encoder)
    celt_encoder_destroy(cs->encoder);

  if (cs->decoder)
    celt_decoder_destroy(cs->decoder);

  celt_mode_destroy(cs->celt_mode);

  free(cs);
}

static unsigned int CELT_bytes2samples(long h_codec, unsigned int num_bytes) {
  if (!h_codec || h_codec == -1)
    return 0;

  CeltState* cs = (CeltState*) h_codec;
  return  cs->frame_size * num_bytes / cs->encoded_size; 
}

static unsigned int CELT_samples2bytes(long h_codec, unsigned int num_samples) {
  if (!h_codec  || h_codec == -1)
    return 0;

  CeltState* cs = (CeltState*) h_codec;
  return cs->encoded_size * num_samples / cs->frame_size; 
}


int Pcm16_2_CELT(unsigned char* out_buf, unsigned char* in_buf, unsigned int size,
		 unsigned int channels, unsigned int rate, long h_codec )
{
  /* TODO: multiple frames, length bytes */
  int res = 0;
  CeltState* cs = (CeltState*) h_codec;
  if (!h_codec || h_codec == -1)
    return 0;

  if (rate != cs->rate) {
    ERROR("sampling rate mismatch (%u vs %u)\n", rate, cs->rate);
    return 0;
  }

  if (channels != cs->channels) {
    ERROR("channels count mismatch (%u vs %u)\n", channels, cs->channels);
    return 0;
  }

  if (PCM16_B2S(size) / channels != cs->frame_size) {
    ERROR("expected %u, got %u bytes\n", PCM16_S2B(cs->frame_size) * channels, size);
    return 0; /* todo! */
  }

  res = celt_encode(cs->encoder, (celt_int16_t*)in_buf, NULL, out_buf, cs->encoded_size);
  /*   DBG("encoded %d from %d, encoded = %u\n", res, size, cs->encoded_size); */
  return res;
}

int CELT_2_Pcm16(unsigned char* out_buf, unsigned char* in_buf, unsigned int size,
		 unsigned int channels, unsigned int rate, long h_codec )
{
  /* TODO: multiple frames, length bytes */
  int res = 0;
  CeltState* cs = (CeltState*) h_codec;

  if (!h_codec || h_codec == -1)
    return 0;
  
  res = celt_decode(cs->decoder, in_buf, size, (celt_int16_t*) out_buf);
/*   DBG("decoded %d from %d\n", res, size); */
  return  res;
}
