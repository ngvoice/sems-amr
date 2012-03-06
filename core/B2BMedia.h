#ifndef __B2BMEDIA_H
#define __B2BMEDIA_H

#include "AmAudio.h"
#include "AmRtpStream.h"

/* class reimplementing AmAudioBridge with regard to really stored samples */

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

class B2BMedia
{
  private:
    AmMutex mutex;
    int ref_cnt;
    AmAudioBuffer a_leg_sink, b_leg_sink;

    std::map<int, int> a_leg_relay_payloads;
    std::map<int, int> b_leg_relay_payloads;
    AmSdp a_leg_local_sdp, a_leg_remote_sdp;
    AmSdp b_leg_local_sdp, b_leg_remote_sdp;
  
    /* generate pair of payloads which are known to both remote parties, 
     * note that payload IDs in each leg may differ for the same payload */
    void computeRelayPayloads(const SdpMedia &a, const SdpMedia &b, std::map<int, int> &dst);
    void normalize(AmSdp &sdp);

    // needed for updating relayed payloads
    AmRtpStream *a_leg_stream, *b_leg_stream;

  public:
    B2BMedia(AmRtpStream *a_stream, AmRtpStream *b_stream): 
      ref_cnt(1), a_leg_stream(a_stream), b_leg_stream(b_stream) { }

    void getAAudio(AmAudio *&sink, AmAudio *&source) { sink = &a_leg_sink; source = &b_leg_sink; }
    void getBAudio(AmAudio *&sink, AmAudio *&source) { sink = &b_leg_sink; source = &a_leg_sink; }
    AmAudio *getASink() { return &a_leg_sink; }
    AmAudio *getASource() { return &b_leg_sink; }
    AmAudio *getBSink() { return &b_leg_sink; }
    AmAudio *getBSource() { return &a_leg_sink; }

    void lock() { mutex.lock(); }
    void unlock() { mutex.unlock(); }
    
    void updateRelayPayloads(bool a_leg, const AmSdp &local_sdp, const AmSdp &remote_sdp);

    // stops direct relaying, clears both stored streams
    void stopRelay();

    void addReference() { lock(); ref_cnt++; unlock(); }

    /* Releases reference.
     * Returns true if this was the last reference and the object should be
     * destroyed (call "delete this" here?) */
    bool releaseReference() { lock(); int r = --ref_cnt; unlock(); return (r == 0); }

};

#endif
