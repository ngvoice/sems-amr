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

#ifndef _amci_h_
#define _amci_h_

/** AUDIO_BUFFER_SIZE must be a power of 2 */
#define AUDIO_BUFFER_SIZE (1<<12) /* 2 KB */

/* #ifdef __cplusplus */
/* extern "C" { */
/* #endif */

#include <stdio.h>

class BitStream
{
public:
    BitStream(){}
    virtual ~BitStream(){}
    
    virtual int read(void* buf, int len)=0;
    virtual int write(void* buf, int len)=0;
    virtual int seek(long p)=0;
    virtual long pos()=0;
    virtual int close()=0;
};

/**
 * @file amci.h
 * @brief Definition of the codec interface.
 * 
 * This codec interface makes it possible for third-party 
 * developer to implement and integrate their own codecs and file formats.
 * 
 * Three entities can be declared within a plug-in:
 * <ol>
 * <li>Codecs: transform samples to/from the internal coding scheme 
 *             from/to these described in the plug-in.
 *
 * <li>Payloads: special format definition used by the RTP protocol 
 *               and including a codec.
 *
 * <li>File formats: collection of subtypes sharing the same header 
 *                   scheme whereby each subtype can use a different codec.
 * </ol>
 * @warning
 *   Please use only the macros at the end of this file
 *   for export definition. This is a much more portable solution
 *   than using directly the structures.<br>
 * \example plug-in/wav/wav.c
 */



/** @def AMCI_RDONLY Read only mode. */
#define AMCI_RDONLY   1
/** @def AMCI_WRONLY Write only mode. */
#define AMCI_WRONLY   2

  /** @def AMCI_FMT_FRAME_LENGTH  frame length in ms (for framed codecs; must be multiple of 10) see codec_init */
#define AMCI_FMT_FRAME_LENGTH       1
  /** @def AMCI_FMT_FRAME_SIZE frame size in samples */
#define AMCI_FMT_FRAME_SIZE         2
  /** @def AMCI_FMT_ENCODED_FRAME_SIZE encoded frame size in bytes */
#define AMCI_FMT_ENCODED_FRAME_SIZE 3

struct amci_codec_t;
struct BitStream;

/**
 * \brief File format declaration 
 */
struct amci_file_desc_t {

    /** file format: @see */
    //char* file_format;

    /** codec: @see */
    const char* codec;
    
    /** sampling rate */
    int rate;

    /** # channels */
    int channels;

    /** size of the data. -1 for unknown/unlimited */
    int data_size;

    /** output buffer size. 0 for no buffering */
    int buffer_size;

    /** output buffer refill threshold */
    int buffer_thresh;

    /** output buffer refill threshold */
    int buffer_full_thresh;
};

/**
 * \brief Sound converter function pointer.
 * @param out      [out] output buffer
 * @param in       [in] input buffer
 * @param size       [in] size of input buffer
 * @param channels [in] number of channels
 * @param rate     [in] sampling rate
 * @param h_codec  [in] codec handle
 * @return
 *     <ul>
 *     <li>if sucess:  bytes written in output buffer (0 is legal)
 *     <li>if failure: err < 0
 *     </ul>
 * @see amci_codec_t::intern2type
 * @see amci_codec_t::type2intern
 */
typedef int (*amci_converter_t)( unsigned char* out,
				 unsigned char* in,
				 unsigned int   size,
				 unsigned int   channels,
				 unsigned int   rate,
				 long           h_codec
                               );

/**
 * \brief Codec specific packet loss concealment function pointer.
 * @param out      [out] output buffer
 * @param size     [in] required size of output
 * @param channels [in] number of channels
 * @param rate     [in] sampling rate
 * @param h_codec  [in] codec handle
 * @return
 *     <ul>
 *     <li>if sucess:  bytes written in output buffer (0 is legal)
 *     <li>if failure: err < 0
 *     </ul>
 * @see amci_codec_t::plc
 */
typedef int (*amci_plc_t)( unsigned char* out,
			   unsigned int   size,
			   unsigned int   channels,
			   unsigned int   rate,
			   long           h_codec );

/**
 * \brief File format handler's open function
 * @param fptr     [in] fresh opened file pointer
 * @param options  [in] options (see amci_inoutfmt_t)
 * @param fmt_desc [out] file description
 * @return if failure -1, else 0.
 * @see amci_inoutfmt_t::open
 */
typedef int (*amci_file_open_t)( BitStream* bs_ptr, int  options,
				 struct amci_file_desc_t* fmt_desc );

/**
 * File format handler's close function
 * @param fptr     [in] fresh opened file pointer
 * @param options  [in] options (see amci_inoutfmt_t)
 * @param fmt_desc [out] file description
 * @return if failure -1, else 0.
 * @see amci_inoutfmt_t::on_close
 */
typedef int (*amci_file_close_t)( BitStream* bs_ptr, int  options,
				  struct amci_file_desc_t* fmt_desc );

/**
 * \brief Codec module 's init function pointer.
 * this function initializes the codec module.
 * @return 0 on success, <0 on error
 */
typedef int (*amci_codec_module_load_t)(void);

/**
 * \brief Codec's module's destroy function pointer.
 */
typedef void (*amci_codec_module_destroy_t)(void);


/**
 * \brief Codec's init function pointer.
 *
 * @param format_parameters  [in] parameters as passed by fmtp tag, 0 if none 
 * @param format_description [out] pointer to describing block, with amci_codec_fmt_info_t array; zero-terminated. 0 if none
 *   <table><tr><td><b>key</b></td><td><b>value</b></td></tr>
 *     <tr><td>AMCI_FMT_FRAME_LENGTH (1)</td><td>  frame length in ms (for framed codecs; must be multiple of 10) </td></tr>
 *     <tr><td>AMCI_FMT_FRAME_SIZE (2)</td><td>  frame size in samples </td></tr>
 *     <tr><td>AMCI_FMT_ENCODED_FRAME_SIZE (3)</td><td>  encoded frame size </td></tr></table>
 * @return -1 if failed, else some handle which will be 
 *         passed by each further call (0 is legal).
 *
 */

// TODO: use SDP-style options only (text based)
/*   typedef struct { */
/*     int id; */
/*     int value; */
/*   } amci_codec_fmt_info_t; */

struct amci_codec_params_t;

typedef long (*amci_codec_init_t)(amci_codec_params_t* codec_desc);

/**
 * \brief Codec's destroy function pointer.
 * @param h_codec Codec handle (from init function).
 */
typedef void (*amci_codec_destroy_t)(long h_codec);

/**
 * \brief Codec's function for calculating the number of samples from bytes
 */
typedef unsigned int (*amci_codec_bytes2samples_t)(long h_codec, unsigned int num_bytes);

/**
 * \brief Codec's function for calculating the number of bytes from samples
 */
typedef unsigned int (*amci_codec_samples2bytes_t)(long h_codec, unsigned int num_samples);

/**
 * \brief Codec description
 */
struct amci_codec_t {

    /** internal codec name */
    char* name;

    /** 
     * Converts the input buffer (internal format: Pcm16)
     * to the format described in this structure. 
     */
    amci_converter_t encode;

    /** Does the opposite of encode. */
    amci_converter_t decode;

    /** Codec specific packet loss concealment. can be NULL */
    amci_plc_t plc;

    /** Init function. can be NULL. */
    amci_codec_init_t    init;
    /** Destroy function. can be NULL. */
    amci_codec_destroy_t destroy;

    /** Function for calculating the number of bytes from samples. */
    amci_codec_bytes2samples_t bytes2samples;

    /** Function for calculating the number of samples from bytes. */
    amci_codec_samples2bytes_t samples2bytes;
};

struct amci_codec_params_t 
{
    /** Number of channels. */
    int channels;

    /** Sampling rate. */
    int rate;

    /** frame length in samples (frame based codecs) */
    int frame_length;

    /** frame size in bytes */
    int frame_size;

    /** encoded frame size in bytes */
    int frame_encoded_size;
};  

/**
 * \brief File format declaration.
 */
struct amci_file_fmt_t {

    /** example: "Wav". */
    char* name; 
  
    /** example: "wav". */
    char* ext;

    /** example: "audio/x-wav". */
    char* mime_type; 

    /** options: AMCI_RDONLY, AMCI_WRONLY. */
    amci_file_open_t open;

    /** no options at the moment. */
    amci_file_close_t on_close;

    /** NULL terminated array of supported codec names. */
    char** subtypes;
};
    
/** Payload type for continuous audio */
#define AMCI_PT_AUDIO_LINEAR 0
/** Payload type for frame based audio */
#define AMCI_PT_AUDIO_FRAME 1

/**
 * \brief ayload declaration
 */
struct amci_payload_t {

    /** static payload id (see RFC 1890) */
    int payload_id;

    /** example: "PCMU" (see RFC 1890)*/
    char* name;

    /** 
     * example: 8000 (Hz).
     * @note For frame based payloads:
     *    use instead the in/out sampling rate
     *    needed for the converting functions.
     */
    int sample_rate;

    /**
     * If this type is not bound to a fixed number of channels,
     * set this parameter to -1.
     * <br>example 1 (mono), 2 (stereo).
    */
    int channels;


    char* codec;

    /** @see AMCI_PT_AUDIO_LINEAR, AMCI_PT_AUDIO_FRAME */
    int type;
};


/**
 * \brief Complete plug-in declaration.
 */
struct amci_exports_t {

    /** Module name */
    char* name;

    /** Codec module load function. can be NULL */
    amci_codec_module_load_t module_load;
    /** Codec module destroy function. can be NULL */
    amci_codec_module_destroy_t module_destroy;

    /** NULL terminated array of amci_codec_t */
    amci_codec_t*    codecs;
    /** NULL terminated array of amci_payload_t */
    amci_payload_t*  payloads;
    /** NULL terminated array of amci_inoutfmt_t */
    amci_file_fmt_t* file_formats;
};

/**
 * Portable export definition macro
 * see example media plug-in 'wav' (plug-in/wav/wav.c).
 * @hideinitializer
 */
#define BEGIN_EXPORTS(name, module_load, module_destroy)	   \
            struct amci_exports_t amci_exports = { \
                name,\
                module_load,\
		module_destroy,


/**
 * Portable export definition macro
 * see example media plug-in 'wav' (plug-in/wav/wav.c).
 * @hideinitializer
 */
#define END_EXPORTS \
            };

/**
 * Portable export definition macro
 * see example media plug-in 'wav' (plug-in/wav/wav.c).
 * @hideinitializer
 */
#define BEGIN_CODECS \
                (struct amci_codec_t[]) {

/**
 * Portable export definition macro
 * see example media plug-in 'wav' (plug-in/wav/wav.c).
 * @hideinitializer
 */
#define END_CODECS \
                    { NULL, 0, 0, 0, 0, 0, 0, 0 } \
                },

/**
 * Portable export definition macro
 * see example media plug-in 'wav' (plug-in/wav/wav.c).
 * @hideinitializer
 */
#define CODEC(id, intern2type,type2intern,plc,init,destroy,bytes2samples,samples2bytes) \
                    { id, intern2type, type2intern, plc, init, destroy, bytes2samples, samples2bytes },

/**
 * Portable export definition macro
 * see example media plug-in 'wav' (plug-in/wav/wav.c).
 * @hideinitializer
 */
#define BEGIN_PAYLOADS \
                (struct amci_payload_t[]) {

/**
 * Portable export definition macro
 * see example media plug-in 'wav' (plug-in/wav/wav.c).
 * @hideinitializer
 */
#define END_PAYLOADS \
                    { -1, 0, -1, -1, NULL, -1 } \
                },

/**
 * Portable export definition macro
 * see example media plug-in 'wav' (plug-in/wav/wav.c).
 * @hideinitializer
 */
#define PAYLOAD(id,name,rate,channels,codec_id,type) \
                    { id, name, rate, channels, codec_id, type },

/**
 * Portable export definition macro
 * see example media plug-in 'wav' (plug-in/wav/wav.c).
 * @hideinitializer
 */
#define BEGIN_FILE_FORMATS \
                (struct amci_file_fmt_t[]) {

/**
 * Portable export definition macro
 * see example media plug-in 'wav' (plug-in/wav/wav.c).
 * @hideinitializer
 */
#define END_FILE_FORMATS \
                    { 0,0,0,0,0, \
                      (char*[]) { NULL } \
                    } \
                }

/**
 * Portable export definition macro
 * see example media plug-in 'wav' (plug-in/wav/wav.c).
 * @hideinitializer
 */
#define BEGIN_FILE_FORMAT(name,ext,email_content_type,open,on_close) \
                    { name,ext,email_content_type,open,on_close,

/**
 * Portable export definition macro
 * see example media plug-in 'wav' (plug-in/wav/wav.c).
 * @hideinitializer
 */
#define END_FILE_FORMAT \
                    },

/**
 * Portable export definition macro
 * see example media plug-in 'wav' (plug-in/wav/wav.c).
 * @hideinitializer
 */
#define BEGIN_SUBTYPES \
                        (char*[]) {

/**
 * Portable export definition macro
 * see example media plug-in 'wav' (plug-in/wav/wav.c).
 * @hideinitializer
 */
#define END_SUBTYPES \
                            NULL \
                        }

/**
 * Portable export definition macro
 * see example media plug-in 'wav' (plug-in/wav/wav.c).
 * @hideinitializer
 */
#define SUBTYPE(codec_name) \
                        codec_name,


  /* defines to make definitions more expressive */
#define AMCI_NO_MODULEINIT    NULL
#define AMCI_NO_MODULEDESTROY NULL
#define AMCI_NO_CODEC_PLC     NULL
#define AMCI_NO_CODECCREATE   NULL
#define AMCI_NO_CODECDESTROY  NULL

/* #ifdef __cplusplus */
/* } */
/* #endif */

#endif







