#include "AmB2BMedia.h"
#include "AmAudio.h"
#include "amci/codecs.h"
#include <string.h>
#include "AmB2BSession.h"
#include "AmRtpReceiver.h"

#define TRACE ERROR

typedef std::vector<AmB2BMedia::AudioStreamPair>::iterator AudioStreamIterator;
typedef std::vector<SdpMedia>::iterator SdpMediaIterator;

AmB2BMedia::AmB2BMedia(AmB2BSession *_a, AmB2BSession *_b): 
  ref_cnt(0), // everybody who wants to use must add one reference itselves
  a(_a), b(_b),
  callgroup(AmSession::getNewId()),
  playout_type(ADAPTIVE_PLAYOUT)
  //playout_type(SIMPLE_PLAYOUT)
{ 
}

void AmB2BMedia::normalize(AmSdp &sdp)
{
  // TODO: normalize SDP
  //  - encoding names for static payloads
  //  - convert encoding names to lowercase
  //  - add clock rate if not given (?)
}
 
static int writeStream(unsigned long long ts, unsigned char *buffer,
    AmRtpAudio *dst, AmRtpAudio *src, 
    AmAudio *alternative_src, 
    AmSession *dtmf_handler)
{
  unsigned int f_size = dst->getFrameSize();
  if (dst->sendIntReached(ts, f_size)) {
    // A leg is ready to send data
    int got = 0;
    if (alternative_src) got = alternative_src->get(ts, buffer, src->getSampleRate(), f_size);
    else {
      if (src->checkInterval(ts)) {
        got = src->get(ts, buffer, src->getSampleRate(), f_size);
        if ((got > 0) && dtmf_handler->isDtmfDetectionEnabled()) 
          dtmf_handler->putDtmfAudio(buffer, got, ts);
      }
    }
    if (got < 0) return -1;
    if (got > 0) return dst->put(ts, buffer, src->getSampleRate(), got);
  }
  return 0;
}

int AmB2BMedia::writeStreams(unsigned long long ts, unsigned char *buffer)
{
  int res = 0;
  mutex.lock();
  for (AudioStreamIterator i = audio.begin(); i != audio.end(); ++i) {
    if (i->a_initialized && (i->b_initialized || i->a_in))
      if (writeStream(ts, buffer, i->a, i->b, i->a_in, a) < 0) { res = -1; break; }
    if (i->b_initialized && (i->a_initialized || i->b_in))
      if (writeStream(ts, buffer, i->b, i->a, i->b_in, b) < 0) { res = -1; break; }
  }
  mutex.unlock();
  return res;
}

void AmB2BMedia::processDtmfEvents()
{
  // FIXME: really locking here?
  mutex.lock();
  if (a) a->processDtmfEvents();
  if (b) b->processDtmfEvents();
  mutex.unlock();
}

void AmB2BMedia::clearAudio()
{
  mutex.lock();

  for (AudioStreamIterator i = audio.begin(); i != audio.end(); ++i) {
    // remove streams from AmRtpReceiver
    if (i->a->hasLocalSocket())
      AmRtpReceiver::instance()->removeStream(i->a->getLocalSocket());
    if (i->b->hasLocalSocket())
      AmRtpReceiver::instance()->removeStream(i->b->getLocalSocket());

    // stop and delete alternative inputs
    if (i->a_in) {
      i->a_in->close();
      delete i->a_in;
    }
    if (i->b_in) {
      i->b_in->close();
      delete i->b_in;
    }

    // delete streams
    delete i->a;
    delete i->b;
  }
  audio.clear();

  // forget sessions to avoid using them once clearAudio is called
  a = NULL;
  b = NULL;

  mutex.unlock();
}

void AmB2BMedia::clearRTPTimeout()
{
  mutex.lock();

  for (AudioStreamIterator i = audio.begin(); i != audio.end(); ++i) {
    i->a->clearRTPTimeout();
    i->b->clearRTPTimeout();
  }
  
  mutex.unlock();
}

void AmB2BMedia::replaceConnectionAddress(AmSdp &parser_sdp, bool a_leg, const string &relay_address) 
{
  mutex.lock();

  // place relay_address in connection address
  if (!parser_sdp.conn.address.empty()) {
    parser_sdp.conn.address = relay_address;
    DBG("new connection address: %s",parser_sdp.conn.address.c_str());
  }

  string replaced_ports;

  AudioStreamIterator streams = audio.begin();

  std::vector<SdpMedia>::iterator it = parser_sdp.media.begin();
  for (; (it != parser_sdp.media.end()) && (streams != audio.end()) ; ++it) {
  
    // FIXME: only audio streams are handled for now
    if (it->type != MT_AUDIO) continue;

    if(it->port) { // if stream active
      if (!it->conn.address.empty()) {
        it->conn.address = relay_address;
        DBG("new stream connection address: %s",it->conn.address.c_str());
      }
      try {
        if (a_leg) it->port = streams->a->getLocalPort();
        else it->port = streams->b->getLocalPort();
        replaced_ports += (streams != audio.begin()) ? int2str(it->port) : "/"+int2str(it->port);
      } catch (const string& s) {
        mutex.unlock();
        ERROR("setting port: '%s'\n", s.c_str());
        throw string("error setting RTP port\n");
      }
    }
    ++streams;
  }

  if (it != parser_sdp.media.end()) {
    // FIXME: create new streams here?
    WARN("trying to relay SDP with more media lines than "
        "relay streams initialized (%lu)\n", audio.size());
  }

  DBG("replaced connection address in SDP with %s:%s.\n",
      relay_address.c_str(), replaced_ports.c_str());
        
  mutex.unlock();
}

void AmB2BMedia::initStreamPair(AudioStreamPair &pair)
{
  pair.a = new AmRtpAudio(a, a->getRtpRelayInterface());
  pair.a->setRtpRelayTransparentSeqno(a->getRtpRelayTransparentSeqno());
  pair.a->setRtpRelayTransparentSSRC(a->getRtpRelayTransparentSSRC());

  pair.b = new AmRtpAudio(b, b->getRtpRelayInterface());
  pair.b->setRtpRelayTransparentSeqno(b->getRtpRelayTransparentSeqno());
  pair.b->setRtpRelayTransparentSSRC(b->getRtpRelayTransparentSSRC());

  pair.a_in = NULL;
  pair.b_in = NULL;

  pair.a_initialized = false;
  pair.b_initialized = false;
}

static void setStreamRelay(AmRtpStream *stream, const SdpMedia &m, AmRtpStream *other)
{
  // We are in locked section, so the stream can not change under our hands
  // remove the stream from processing to avoid changing relay params under the
  // hands of an AmRtpReceiver process.
  // Updating relay information is not done so often so this might be better
  // solution than using additional locking within AmRtpStream.
  if (stream->hasLocalSocket())
    AmRtpReceiver::instance()->removeStream(stream->getLocalSocket());
  
  if (m.payloads.size() > 0) {
    PayloadMask mask;

    TRACE("enabling stream relay\n");

    // walk through the media line and add all payload IDs to the bit mask
    for (std::vector<SdpPayload>::const_iterator i = m.payloads.begin(); 
        i != m.payloads.end(); ++i) 
    {
      mask.set(i->payload_type);
    
      TRACE(" ... payload %d\n", i->payload_type);
    }

    stream->enableRtpRelay(mask, other);
  }
  else {
    // nothing to relay
    stream->disableRtpRelay();
    TRACE("disabling stream relay\n");
  }

  // return back for processing if needed
  if (stream->hasLocalSocket())
    AmRtpReceiver::instance()->addStream(stream->getLocalSocket(), stream);
}
    
bool AmB2BMedia::resetInitializedStreams(bool a_leg)
{
  bool res = false;
  for (AudioStreamIterator i = audio.begin(); i != audio.end(); ++i) {
    if (a_leg) {
      res = res || i->a_initialized;
      i->a_initialized = false;
    } else {
      res = res || i->b_initialized;
      i->b_initialized = false;
    }
  }
  return res;
}

void AmB2BMedia::initStream(AmRtpAudio *stream, AmSdp &local_sdp, AmSdp &remote_sdp, int media_idx)
{
  // remove from processing to safely update the stream
  if (stream->hasLocalSocket())
    AmRtpReceiver::instance()->removeStream(stream->getLocalSocket());

  stream->forceSdpMediaIndex(media_idx);
  stream->init(local_sdp, remote_sdp);
  stream->setPlayoutType(playout_type);
  
  // return back for processing if needed
  if (stream->hasLocalSocket())
    AmRtpReceiver::instance()->addStream(stream->getLocalSocket(), stream);
}

void AmB2BMedia::updateStreams(bool a_leg, bool init_relay, bool init_transcoding)
{
  unsigned stream_idx = 0;
  unsigned media_idx = 0;

  AmSdp *sdp;
  if (a_leg) sdp = &a_leg_remote_sdp;
  else sdp = &b_leg_remote_sdp;

  for (SdpMediaIterator m = sdp->media.begin(); m != sdp->media.end(); ++m, ++media_idx) {
    if (m->type != MT_AUDIO) continue;

    // create pair of Rtp streams if it doesn't exist yet
    if (stream_idx >= audio.size()) {
      AudioStreamPair pair;
      initStreamPair(pair);
      audio.push_back(pair);
      stream_idx = audio.size() - 1;
    }
    AudioStreamPair &pair = audio[stream_idx];

    if (init_relay) {
      // initialize RTP relay stream and payloads in the other leg (!)
      // Payloads present in this SDP can be relayed directly by the AmRtpStream
      // of the other leg to the AmRtpStream of this leg.
      if (a_leg) setStreamRelay(pair.b, *m, pair.a);
      else setStreamRelay(pair.a, *m, pair.b);
    }

    // initialize the stream for current leg if asked to do so
    if (init_transcoding) {
      if (a_leg) {
        initStream(pair.a, a_leg_local_sdp, a_leg_remote_sdp, media_idx);
        pair.a_initialized = true;
        TRACE("A leg stream initialized\n");
      } else {
        initStream(pair.b, b_leg_local_sdp, b_leg_remote_sdp, media_idx);
        pair.b_initialized = true;
        TRACE("B leg stream initialized\n");
      }
    }

    stream_idx++;
  }

}

void AmB2BMedia::updateRemoteSdp(bool a_leg, const AmSdp &remote_sdp)
{
  
  TRACE("updating %s leg remote SDP\n", a_leg ? "A" : "B");

  mutex.lock();

  bool initialize_streams;

  if (a_leg) {
    a_leg_remote_sdp = remote_sdp;
    normalize(a_leg_remote_sdp);
  }
  else {
    b_leg_remote_sdp = remote_sdp;
    normalize(b_leg_remote_sdp);
  }

  if (resetInitializedStreams(a_leg)) { 
    // needed to reinitialize later 
    // streams were initialized before and the local SDP is still not up-to-date
    // otherwise streams would by alredy reset
    initialize_streams = false;
  }
  else {
    // streams were not initialized, we should initialize them if we have
    // local SDP already
    if (a_leg) initialize_streams = a_leg_local_sdp.media.size() > 0;
    else initialize_streams = b_leg_local_sdp.media.size() > 0;
  }

  updateStreams(a_leg, 
      true /* needed to initialize relay stuff on every remote SDP change */, 
      initialize_streams);

  updateProcessingState(); // start media processing if possible

  mutex.unlock();
}
    
void AmB2BMedia::updateLocalSdp(bool a_leg, const AmSdp &local_sdp)
{
  TRACE("updating %s leg local SDP\n", a_leg ? "A" : "B");

  mutex.lock();
  // streams should be created already (replaceConnectionAddress called
  // before updateLocalSdp uses/assignes their port numbers)

  if (a_leg) {
    a_leg_local_sdp = local_sdp;
    normalize(a_leg_local_sdp);
  }
  else {
    b_leg_local_sdp = local_sdp;
    normalize(b_leg_local_sdp);
  }
 
  bool initialize_streams;
  if (resetInitializedStreams(a_leg)) { 
    // needed to reinitialize later 
    // streams were initialized before and the remote SDP is still not up-to-date
    // otherwise streams would by alredy reset
    initialize_streams = false;
  }
  else {
    // streams were not initialized, we should initialize them if we have
    // remote SDP already
    if (a_leg) initialize_streams = a_leg_remote_sdp.media.size() > 0;
    else initialize_streams = b_leg_remote_sdp.media.size() > 0;
  }

  updateStreams(a_leg, 
      false /* local SDP change has no effect on relay */, 
      initialize_streams);

  updateProcessingState(); // start media processing if possible

  mutex.unlock();
}

void AmB2BMedia::updateProcessingState()
{
  // once we send local SDP to the other party we have to expect RTP so we
  // should start RTP processing now (though streams in opposite direction need
  // not to be initialized yet) ... FIXME: but what to do with data then? =>
  // wait for having all SDPs ready

  if (a_leg_local_sdp.media.size() &&
      a_leg_remote_sdp.media.size() &&
      b_leg_local_sdp.media.size() &&
      b_leg_remote_sdp.media.size() &&
      !isProcessingMedia()) 
  {
    ref_cnt++; // add reference (hold by AmMediaProcessor)
    TRACE("starting media processing (ref cnt = %d)\n", ref_cnt);
    AmMediaProcessor::instance()->addSession(this, callgroup);
  }
}

void AmB2BMedia::stop()
{
  clearAudio();
  if (isProcessingMedia()) 
    AmMediaProcessor::instance()->removeSession(this);
}

void AmB2BMedia::onMediaProcessingTerminated() 
{ 
  AmMediaSession::onMediaProcessingTerminated();
  clearAudio();

  TRACE("media processing terminated\n");

  // release reference held by AmMediaProcessor
  if (releaseReference()) { 
    TRACE("releasing myself!\n");
    delete this; // this should really work :-D
  }
}
