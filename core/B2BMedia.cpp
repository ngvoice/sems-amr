#include "B2BMedia.h"
#include "AmAudio.h"
#include "amci/codecs.h"

AmAudioBuffer::AmAudioBuffer(): 
  //AmAudio(), 
  AmAudio(new AmAudioSimpleFormat(CODEC_PCM16)),
  stored(0), wpos(0), rpos(0)
{
}

int AmAudioBuffer::read(unsigned int user_ts, unsigned int size)
{
  unsigned char* dst = samples;
  if (stored > 0) {
    if (size >= stored) size = stored;

    unsigned space = buffer_size - rpos;
    if (space >= size) {
      memcpy(dst, buffer + rpos, size);
      rpos += size;
    } else {
      memcpy(dst, buffer + rpos, space);
      memcpy(dst + space, buffer, size - space);
      rpos = size - space;
    }
    stored -= size;
    return size;
  }
  else return 0;
}

int AmAudioBuffer::write(unsigned int user_ts, unsigned int size)
{
  if (size == 0) return 0;
  if (size > buffer_size) {
    ERROR("too large packet for buffer\n");
    return -1;
  }
  unsigned char* src = samples;

  unsigned space = buffer_size - wpos;
  if (space >= size) {
    memcpy(buffer + wpos, src, size);
    wpos += size;
  }
  else {
    memcpy(buffer + wpos, src, space);
    memcpy(buffer, src + space, size - space);
    wpos = size - space;
  }
  stored += size;
  if (stored > buffer_size) {
    // overflow
    stored = buffer_size;
    rpos = wpos;
    ERROR("overflow\n");
  }

  return size;
}


//////////////////////////////////////////////////////////////////////////////////////////////////////////

void B2BMedia::stopRelay()
{
  lock();
  // we must avoid relaying data between the streams
  if (a_leg_stream) a_leg_stream->disableRtpRelay();
  if (b_leg_stream) b_leg_stream->disableRtpRelay();

  // clear stored streams
  a_leg_stream = NULL;
  b_leg_stream = NULL;
  unlock();
}

static std::vector<SdpPayload>::const_iterator findPayload(const SdpMedia &m, const SdpPayload *payload)
{
  std::vector<SdpPayload>::const_iterator i = m.payloads.begin();
  for (; i != m.payloads.end(); ++i) {
    if ((i->encoding_name == payload->encoding_name) && (i->clock_rate == payload->clock_rate)) {
      // FIXME: test another params?
      break; // found
    }
  }
  return i;
}

void B2BMedia::computeRelayPayloads(const SdpMedia &local, const SdpMedia &remote, std::map<int, int> &dst)
{
  dst.clear();
  for (std::vector<SdpPayload>::const_iterator i = local.payloads.begin(); i != local.payloads.end(); ++i) {
    //ERROR("looking for %s in B leg\n", i->encoding_name.c_str());
    std::vector<SdpPayload>::const_iterator j = findPayload(remote, &(*i));
    if (j != remote.payloads.end()) {
      dst[i->payload_type] = j->payload_type;
      ERROR("payloads to relay %d <-> %d\n", i->payload_type, j->payload_type);
    }
  }
}
    
void B2BMedia::normalize(AmSdp &sdp)
{
  // TODO: normalize SDP
  //  - encoding names for static payloads
  //  - convert encoding names to lowercase
  //  - add clock rate if not given (?)
}

void B2BMedia::updateRelayPayloads(bool a_leg, const AmSdp &local_sdp, const AmSdp &remote_sdp)
{
  lock();

  // store SDP for later usage (we need remote SDP of one leg combine together
  // with local SDP of other leg)
  if (a_leg) {
    a_leg_local_sdp = local_sdp;
    a_leg_remote_sdp = remote_sdp;
    normalize(a_leg_local_sdp);
    normalize(a_leg_remote_sdp);
  }
  else {
    b_leg_local_sdp = local_sdp;
    b_leg_remote_sdp = remote_sdp;
    normalize(b_leg_local_sdp);
    normalize(b_leg_remote_sdp);
  }
    
  // if we have both required SDPs we can update relay_payloads (one stream only for now)
  if ((a_leg_local_sdp.media.size() > 0) && (b_leg_remote_sdp.media.size() > 0)) {
    ERROR("computing A leg payloads to relay:\n");
    computeRelayPayloads(a_leg_local_sdp.media[0], b_leg_remote_sdp.media[0], a_leg_relay_payloads);
    if (a_leg_stream) a_leg_stream->enableRtpRelay(a_leg_relay_payloads, b_leg_stream);
  }
  if ((b_leg_local_sdp.media.size() > 0) && (a_leg_remote_sdp.media.size() > 0)) {
    ERROR("computing B leg payloads to relay:\n");
    computeRelayPayloads(b_leg_local_sdp.media[0], a_leg_remote_sdp.media[0], b_leg_relay_payloads);
    if (b_leg_stream) b_leg_stream->enableRtpRelay(b_leg_relay_payloads, a_leg_stream);
  }

  unlock();
}

