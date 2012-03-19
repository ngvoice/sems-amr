#include "AmB2BMedia.h"
#include "AmAudio.h"
#include "amci/codecs.h"
#include <string.h>
#include "AmB2BSession.h"
#include "AmRtpReceiver.h"

#define TRACE ERROR

AmB2BMedia::AudioStreamData::AudioStreamData(AmB2BSession *session):
  in(NULL), initialized(false),
  dtmf_detector(NULL), dtmf_queue(NULL)
{
  stream = new AmRtpAudio(session, session->getRtpRelayInterface());
  stream->setRtpRelayTransparentSeqno(session->getRtpRelayTransparentSeqno());
  stream->setRtpRelayTransparentSSRC(session->getRtpRelayTransparentSSRC());
}

static void stopStreamProcessing(AmRtpStream *stream)
{
  if (stream->hasLocalSocket())
    AmRtpReceiver::instance()->removeStream(stream->getLocalSocket());
}

//////////////////////////////////////////////////////////////////////////////////

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
    AmDtmfEventQueue *dtmf_handler,
    const char *str)
{
  unsigned int f_size = dst->getFrameSize();
  if (dst->sendIntReached(ts)) {
    // A leg is ready to send data
    int got = 0;
    if (alternative_src) got = alternative_src->get(ts, buffer, src->getSampleRate(), f_size);
    else {
      if (src->checkInterval(ts)) {
        got = src->get(ts, buffer, src->getSampleRate(), f_size);
        if ((got > 0) && dtmf_handler) 
          dtmf_handler->putDtmfAudio(buffer, got, ts);
      }
    }
    if (got < 0) return -1;
    if (got > 0) return dst->put(ts, buffer, dst->getSampleRate(), got);
  }
  return 0;
}

int AmB2BMedia::writeStreams(unsigned long long ts, unsigned char *buffer)
{
  int res = 0;
  mutex.lock();
  for (AudioStreamIterator i = audio.begin(); i != audio.end(); ++i) {
    if (i->a.initialized && (i->b.initialized || i->a.in))
      if (writeStream(ts, buffer, i->a.stream, i->b.stream, i->a.in, i->a.dtmf_queue, NULL) < 0) { res = -1; break; }
    if (i->b.initialized && (i->a.initialized || i->b.in))
      if (writeStream(ts, buffer, i->b.stream, i->a.stream, i->b.in, i->b.dtmf_queue, "B") < 0) { res = -1; break; }
  }
  mutex.unlock();
  return res;
}

void AmB2BMedia::processDtmfEvents()
{
  // FIXME: really locking here?
  mutex.lock();
  for (AudioStreamIterator i = audio.begin(); i != audio.end(); ++i) {
    if (i->a.dtmf_queue) i->a.dtmf_queue->processEvents();
    if (i->b.dtmf_queue) i->b.dtmf_queue->processEvents();
  }

  if (a) a->processDtmfEvents();
  if (b) b->processDtmfEvents();
  mutex.unlock();
}

void AmB2BMedia::clearAudio()
{
  mutex.lock();

  for (AudioStreamIterator i = audio.begin(); i != audio.end(); ++i) {
    // remove streams from AmRtpReceiver
    stopStreamProcessing(i->a.stream);
    stopStreamProcessing(i->b.stream);

    // stop and delete alternative inputs
    if (i->a.in) {
      i->a.in->close();
      delete i->a.in;
    }
    if (i->b.in) {
      i->b.in->close();
      delete i->b.in;
    }

    // delete streams
    delete i->a.stream;
    delete i->b.stream;

    if (i->a.dtmf_detector) delete i->a.dtmf_detector;
    if (i->b.dtmf_detector) delete i->b.dtmf_detector;
    
    if (i->a.dtmf_queue) delete i->a.dtmf_queue;
    if (i->b.dtmf_queue) delete i->b.dtmf_queue;
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
    i->a.stream->clearRTPTimeout();
    i->b.stream->clearRTPTimeout();
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
        if (a_leg) it->port = streams->a.stream->getLocalPort();
        else it->port = streams->b.stream->getLocalPort();
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
    
bool AmB2BMedia::resetInitializedStream(AudioStreamData &data)
{
  if (data.initialized) {
    data.initialized = false;

    if (data.dtmf_detector) {
      delete data.dtmf_detector;
      data.dtmf_detector = NULL;
    }
    if (data.dtmf_queue) {
      delete data.dtmf_queue;
      data.dtmf_queue = NULL;
    }
    return true;
  }
  return false;
}
    
bool AmB2BMedia::resetInitializedStreams(bool a_leg)
{
  bool res = false;
  for (AudioStreamIterator i = audio.begin(); i != audio.end(); ++i) {
    if (a_leg) res = res || resetInitializedStream(i->a);
    else res = res || resetInitializedStream(i->b);
  }
  return res;
}
      
void AmB2BMedia::initStream(AudioStreamData &data, AmSession *session, 
    AmSdp &local_sdp, AmSdp &remote_sdp, int media_idx)
{
  // remove from processing to safely update the stream
  if (data.stream->hasLocalSocket())
    AmRtpReceiver::instance()->removeStream(data.stream->getLocalSocket());

  data.stream->forceSdpMediaIndex(media_idx);
  data.stream->init(local_sdp, remote_sdp);
  data.stream->setPlayoutType(playout_type);
  data.initialized = true;
  if (session->isDtmfDetectionEnabled()) {
    data.dtmf_detector = new AmDtmfDetector(session);
    data.dtmf_queue = new AmDtmfEventQueue(data.dtmf_detector);
    data.dtmf_detector->setInbandDetector(AmConfig::DefaultDTMFDetector, data.stream->getSampleRate());
  }

  // return back for processing if needed
  if (data.stream->hasLocalSocket())
    AmRtpReceiver::instance()->addStream(data.stream->getLocalSocket(), data.stream);
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
      AudioStreamPair pair(a, b);
      audio.push_back(pair);
      stream_idx = audio.size() - 1;
    }
    AudioStreamPair &pair = audio[stream_idx];

    if (init_relay) {
      // initialize RTP relay stream and payloads in the other leg (!)
      // Payloads present in this SDP can be relayed directly by the AmRtpStream
      // of the other leg to the AmRtpStream of this leg.
      if (a_leg) setStreamRelay(pair.b.stream, *m, pair.a.stream);
      else setStreamRelay(pair.a.stream, *m, pair.b.stream);
    }

    // initialize the stream for current leg if asked to do so
    if (init_transcoding) {
      if (a_leg) {
        initStream(pair.a, a, a_leg_local_sdp, a_leg_remote_sdp, media_idx);
        TRACE("A leg stream initialized\n");
      } else {
        initStream(pair.b, b, b_leg_local_sdp, b_leg_remote_sdp, media_idx);
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
