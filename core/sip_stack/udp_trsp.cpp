
#include "udp_trsp.h"
#include "sip_parser.h"
#include "trans_layer.h"
#include "log.h"

#include <netinet/in.h>
#include <errno.h>
#include <strings.h>

udp_trsp::udp_trsp(trans_layer* tl)
    : transport(tl), sd(0)
{
}

udp_trsp::~udp_trsp()
{
}


/** @see AmThread */
void udp_trsp::run()
{
    char buf[MAX_UDP_MSGLEN];
    int buf_len;

    sockaddr_storage from_addr;
    socklen_t        from_addr_len = sizeof(sockaddr_storage);
    
    while(true){

	buf_len = recvfrom(sd,buf,MAX_UDP_MSGLEN,MSG_TRUNC,(sockaddr*)&from_addr,&from_addr_len);
	if(buf_len <= 0){
	    ERROR("recvfrom returned %d: %s\n",buf_len,strerror(errno));
	    continue;
	}

	if(buf_len > MAX_UDP_MSGLEN){
	    ERROR("Message was too big (>%d)\n",MAX_UDP_MSGLEN);
	    continue;
	}

	sip_msg* msg = new sip_msg(buf,buf_len);
	memcpy(&msg->recved,&from_addr,from_addr_len);
	msg->recved_len = from_addr_len;

	// pass message to the parser / transaction layer
	tl->received_msg(msg);
    }
}

/** @see AmThread */
void udp_trsp::on_stop()
{

}

    
/** @see transport */
int udp_trsp::bind(const string& address, int port)
{
    // FIXME: address is ignored

    if(sd){
	WARN("re-binding socket\n");
	close(sd);
    }

    if((sd = socket(PF_INET,SOCK_DGRAM,0)) == -1){
	ERROR("socket: %s\n",strerror(errno));
	return -1;
    } 
    
    sockaddr_in addr;
    
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    addr.sin_addr.s_addr = INADDR_ANY;

    if(::bind(sd,(const struct sockaddr*)&addr,
	     sizeof(struct sockaddr_in))) {

	ERROR("bind: %s\n",strerror(errno));
	close(sd);
	return -1;
    }
    
    int true_opt = 1;
    if(setsockopt(sd, SOL_SOCKET, SO_REUSEADDR,
		  (void*)&true_opt, sizeof (true_opt)) == -1) {
	
	ERROR("%s\n",strerror(errno));
	close(sd);
	return -1;
    }

    return 0;
}

/** @see transport */
int udp_trsp::send(const sockaddr_storage* sa, const char* msg, const int msg_len)
{
    // NYI
    return -1;
}
