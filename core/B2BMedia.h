#ifndef __B2BMEDIA_H
#define __B2BMEDIA_H

#include "AmAudio.h"
#include "AmRtpStream.h"
#include "AmRtpAudio.h"
#include "AmMediaProcessor.h"

/* class reimplementing AmAudioBridge with regard to return just really stored
 * samples */

class AmAudioBuffer: public AmAudio {
  protected:
    static const unsigned int buffer_size = 4096;

    unsigned wpos, rpos, stored;
    unsigned char buffer[buffer_size];

    /** Gets 'size' bytes directly from stream (Read,Pull). */
    virtual int read(unsigned int user_ts, unsigned int size);
    /** Puts 'size' bytes directly from stream (Write,Push). */
    virtual int write(unsigned int user_ts, unsigned int size);

  public:
    AmAudioBuffer();
};

class AmB2BSession;

/* class for handling media in a B2B session
 * TODO:
 *  - make independent sendIntReached and checkInterval in AmRtpAudio
 *  - non-audio streams - list of AmRtpStream pairs which can be just relayed
 *  - RTCP - another pair of streams (?)
 *  - independent clear of one call leg (to be able to connect one to another
 *    call leg)
 *  - forward DTMF directly without using AmB2BSession? (but might need
 *    signaling, not only media!)
 *
 * Because generating SDP is no more based on our offer/answer but is based on
 * relayed SDP with just slight changes (some paylaods filtered out, some
 * payloads added before forwarding) we don't need to remember payload ID
 * mapping any more. Payload IDs should be simply generated correctly by the
 * other party. (TODO: test)
 *
 * For music on hold we need to understand at least one format to that
 * destination! (FIXME: if not, generate reINVITE with that or just do not allow
 * MOH?)
 *
 */

class B2BMedia: public AmMediaSession
{
  private:
    /* remembered both legs of the B2B call
     * currently required for DTMF processing and used for reading RTP relay
     * parameters (rtp_relay_transparent_seqno, rtp_relay_transparent_ssrc,
     * rtp_interface) */
    AmB2BSession *a, *b;

    /* Pair of audio streams with the possibility to use given audio as input
     * instead of the other stream. */
    struct AudioStreamPair {
      AmRtpAudio *a, *b;

      /* non-stream input (required for music on hold) */
      AmAudio *a_in, *b_in;

      PayloadMask a_leg_relay_payloads;
      PayloadMask b_leg_relay_payloads;

      bool a_initialized, b_initialized;
    };

    // reqired by AmMediaProcessor
    string callgroup;
      
    // needed for updating relayed payloads
    AmSdp a_leg_local_sdp, a_leg_remote_sdp;
    AmSdp b_leg_local_sdp, b_leg_remote_sdp;

    AmMutex mutex;
    int ref_cnt;

    void normalize(AmSdp &sdp);
    void initStreamPair(AudioStreamPair &pair);

    std::vector<AudioStreamPair> audio;

    void updateProcessingState();

  public:
    B2BMedia(AmB2BSession *_a, AmB2BSession *_b);

    //void updateRelayPayloads(bool a_leg, const AmSdp &local_sdp, const AmSdp &remote_sdp);

    void addReference() { mutex.lock(); ref_cnt++; mutex.unlock(); }

    /* Releases reference.
     * Returns true if this was the last reference and the object should be
     * destroyed (call "delete this" here?) */
    bool releaseReference() { mutex.lock(); int r = --ref_cnt; mutex.unlock(); return (r == 0); }

    // ----------------- SDP manipulation & updates -------------------

    /* replace connection address and ports within SDP 
     * throws an exception (string) in case of error (FIXME?) */
    void replaceConnectionAddress(AmSdp &parser_sdp, bool a_leg, const string &relay_address);

    void updateRemoteSdp(bool a_leg, const AmSdp &remote_sdp);
    void updateLocalSdp(bool a_leg, const AmSdp &local_sdp);

    /* clear audio and stop processing */
    void stop();

    // ---- AmMediaSession interface for processing audio in a standard way ----

    /* all processing in writeStreams */
    virtual int readStreams(unsigned int ts, unsigned char *buffer) { return 0; }
    
    /* handle non-relayed data in audio streams */
    virtual int writeStreams(unsigned int ts, unsigned char *buffer);

    virtual void processDtmfEvents();

    /* clear streams of both legs */
    virtual void clearAudio();

    virtual void clearRTPTimeout();
    
    virtual void onMediaProcessingTerminated();

};

#endif
