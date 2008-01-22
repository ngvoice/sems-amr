
#include "udp_trsp.h"
#include "sip_parser.h"
#include "trans_layer.h"
#include "log.h"

#include <netinet/in.h>
#include <sys/param.h>

#include <errno.h>
#include <strings.h>

// FIXME: support IPv6

#if defined IP_RECVDSTADDR
# define DSTADDR_SOCKOPT IP_RECVDSTADDR
# define DSTADDR_DATASIZE (CMSG_SPACE(sizeof(struct in_addr)))
# define dstaddr(x) (CMSG_DATA(x))
#elif defined IP_PKTINFO
# define DSTADDR_SOCKOPT IP_PKTINFO
# define DSTADDR_DATASIZE (CMSG_SPACE(sizeof(struct in_pktinfo)))
# define dstaddr(x) (&(((struct in_pktinfo *)(CMSG_DATA(x)))->ipi_addr))
#else
# error "can't determine socket option (IP_RECVDSTADDR or IP_PKTINFO)"
#endif

union control_data {
    struct cmsghdr  cmsg;
    u_char          data[DSTADDR_DATASIZE];

};


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

    msghdr           msg;
    control_data     cmsg;
    cmsghdr*         cmsgptr; 
    sockaddr_storage from_addr;
    iovec            iov[1];

    iov[0].iov_base = buf;
    iov[0].iov_len  = MAX_UDP_MSGLEN;

    memset(&msg,0,sizeof(msg));
    msg.msg_name       = &from_addr;
    msg.msg_namelen    = sizeof(sockaddr_storage);
    msg.msg_iov        = iov;
    msg.msg_iovlen     = 1;
    msg.msg_control    = &cmsg;
    msg.msg_controllen = sizeof(cmsg);

    while(true){

	DBG("before recvmsg\n");
	buf_len = recvmsg(sd,&msg,0);
	if(buf_len <= 0){
	    ERROR("recvfrom returned %d: %s\n",buf_len,strerror(errno));
	    continue;
	}

	if(buf_len > MAX_UDP_MSGLEN){
	    ERROR("Message was too big (>%d)\n",MAX_UDP_MSGLEN);
	    continue;
	}

	sip_msg* s_msg = new sip_msg(buf,buf_len);

	memcpy(&s_msg->remote_ip,&msg.msg_name,sizeof(sockaddr_storage));
	//msg->remote_ip_len = sizeof(sockaddr_storage);

	for (cmsgptr = CMSG_FIRSTHDR(&msg);
             cmsgptr != NULL;
             cmsgptr = CMSG_NXTHDR(&msg, cmsgptr)) {
	    
            if (cmsgptr->cmsg_level == IPPROTO_IP &&
                cmsgptr->cmsg_type == DSTADDR_SOCKOPT) {
		
		s_msg->local_ip.ss_family = AF_INET;
		((sockaddr_in*)(&s_msg->local_ip))->sin_port   = htons(_port);
                memcpy(&((sockaddr_in*)(&s_msg->local_ip))->sin_addr,dstaddr(cmsgptr),sizeof(in_addr));
            }
        } 

	// pass message to the parser / transaction layer
	tl->received_msg(s_msg);
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

    if(setsockopt(sd, IPPROTO_IP, DSTADDR_SOCKOPT,
		  (void*)&true_opt, sizeof (true_opt)) == -1) {
	
	ERROR("%s\n",strerror(errno));
	close(sd);
	return -1;
    }

    _port = port;

    return 0;
}

/** @see transport */
int udp_trsp::send(const sockaddr_storage* sa, const char* msg, const int msg_len)
{
    // NYI
    return -1;
}
