#include "tcp_trsp.h"
#include "ip_util.h"
#include "parse_common.h"
#include "sip_parser.h"
#include "trans_layer.h"

#include "AmUtils.h"

#include <netdb.h>
#include <event.h>
#include <string.h>
#include <fcntl.h>
#include <sys/ioctl.h>

fd_trigger::fd_trigger()
{
  event_pipe[0] = event_pipe[1] = 0;
}

fd_trigger::~fd_trigger()
{
  close(event_pipe[0]);
  close(event_pipe[1]);
  event_del(&ev_trigger);
}

int fd_trigger::init()
{
  if(event_pipe[0])
    return 0;

  if(pipe(event_pipe) < 0) {
    ERROR("while creating internal pipe: %s\n",strerror(errno));
    return -1;
  }

  if(fcntl(event_pipe[0], F_SETFL, O_NONBLOCK) ||
     fcntl(event_pipe[1], F_SETFL, O_NONBLOCK)) {
    ERROR("could not setup pipe to be non-blocking: %s\n",
	  strerror(errno));
    return -1;
  }

  return 0;
}

int fd_trigger::trigger()
{
  return write(event_pipe[1], ".", 1);
}

void fd_trigger::clear_trigger()
{
  char c[16];
  read(event_pipe[0], &c, 16);
}

static void on_sock_read(int fd, short ev, void* arg)
{
  if(ev & EV_READ){
    ((tcp_trsp_socket*)arg)->on_read();
  }
}

static void on_sock_write(int fd, short ev, void* arg)
{
  if(ev & EV_WRITE){
    ((tcp_trsp_socket*)arg)->on_write();
  }
}

tcp_trsp_socket::tcp_trsp_socket(tcp_server_socket* server_sock,
				 int sd, const sockaddr_storage* sa,
				 struct event_base* evbase)
  : trsp_socket(server_sock->get_if(),0,0,sd),
    server_sock(server_sock), closed(false), connected(false),
    input_len(0), evbase(evbase)
{
  // local address
  ip = server_sock->get_ip();
  port = server_sock->get_port();
  server_sock->copy_addr_to(&addr);

  // peer address
  memcpy(&peer_addr,sa,sizeof(sockaddr_storage));

  char host[NI_MAXHOST] = "";
  peer_ip = am_inet_ntop(&peer_addr,host,NI_MAXHOST);
  peer_port = am_get_port(&peer_addr);

  // async parser state
  pst.reset((char*)input_buf);

  if(sd > 0) {
    connected = true;
    add_events();
  }
}

void tcp_trsp_socket::add_events()
{
  // libevent stuff
  event_set(&read_ev, sd, EV_READ|EV_PERSIST, ::on_sock_read, (void *)this);
  event_base_set(evbase, &read_ev);
  event_add(&read_ev, NULL);

  event_set(&write_ev, sd, EV_WRITE, ::on_sock_write, (void *)this);
  event_base_set(evbase, &write_ev);
  // do not add event now: only when there is something to write!
}

tcp_trsp_socket::~tcp_trsp_socket()
{
  DBG("********* connection destructor ***********");
}

void tcp_trsp_socket::copy_peer_addr(sockaddr_storage* sa)
{
  memcpy(sa,&peer_addr,sizeof(sockaddr_storage));
}

tcp_trsp_socket::msg_buf::msg_buf(const sockaddr_storage* sa, const char* msg, 
				  const int msg_len)
  : msg_len(msg_len)
{
  memcpy(&addr,sa,sizeof(sockaddr_storage));
  cursor = this->msg = new char[msg_len];
  memcpy(this->msg,msg,msg_len);
}

tcp_trsp_socket::msg_buf::~msg_buf()
{
  delete [] msg;
}

int tcp_trsp_socket::connect()
{
  if((sd = socket(peer_addr.ss_family,SOCK_STREAM,0)) == -1){
    ERROR("socket: %s\n",strerror(errno));
    return -1;
  } 

  int true_opt = 1;
  if(ioctl(sd, FIONBIO , &true_opt) == -1) {
    ERROR("could not make new connection non-blocking: %s\n",strerror(errno));
    ::close(sd);
    return -1;
  }

  int ret = ::connect(sd, (const struct sockaddr*)&peer_addr, SA_len(&peer_addr));
  if(ret < 0) {
    if(errno != EINPROGRESS && errno != EALREADY) {
      ERROR("could not connect: %s",strerror(errno));
      ::close(sd);
      return -1;
    }
  }
  else {
    // connect succeeded immediatly
    connected = true;
  }

  add_events();
  return 0;
}

int tcp_trsp_socket::send(const sockaddr_storage* sa, const char* msg, 
			  const int msg_len)
{
  if(!closed && sd < 0){
    if(connect() < 0)
      return -1;
  }

  // async send
  // TODO: do we need a sync-send as well???
  //   (for ex., when sending errors from recv-thread?)
  send_q_mut.lock();
  send_q.push_back(new msg_buf(sa,msg,msg_len));
  send_q_mut.unlock();

  server_sock->trigger_write(this);
    
  return 0;
}

void tcp_trsp_socket::close()
{
  // TODO:
  // - remove the socket object from 
  //   server mapping and from memory.

  closed = true;
  DBG("********* closing connection ***********");

  event_del(&read_ev);
  event_del(&write_ev);
  ::close(sd);

  server_sock->remove_connection(this);
}

void tcp_trsp_socket::on_read()
{
  DBG("on_read (connected = %i)",connected);

  char* old_cursor = (char*)get_input();

  int bytes = ::read(sd,get_input(),get_input_free_space());
  if(bytes < 0) {
    switch(errno) {
    case EAGAIN:
      return; // nothing to read

    case ECONNRESET:
    case ENOTCONN:
      DBG("connection has been closed (sd=%i)",sd);
      close();
      return;

    case ETIMEDOUT:
      DBG("transmission timeout (sd=%i)",sd);
      return;

    default:
      DBG("unknown error (%i): %s",errno,strerror(errno));
      return;
    }
  }
  else if(bytes == 0) {
    // connection closed
    DBG("connection has been closed (sd=%i)",sd);
    close();
    return;
  }

  input_len += bytes;

  DBG("received: <%.*s>",bytes,old_cursor);

  // ... and parse it
  if(parse_input() < 0) {
    DBG("Error while parsing input: closing connection!");
    close();
  }
}

int tcp_trsp_socket::parse_input()
{
  int err = skip_sip_msg_async(&pst, (char*)(input_buf+input_len));
  if(err) {

    if((err == UNEXPECTED_EOT) &&
       get_input_free_space()) {

      return 0;
    }

    if(!get_input_free_space()) {
      DBG("message way too big! should drop connection...");
    }

    //TODO: drop connection
    // close connection? let's see...
    ERROR("parsing error %i",err);

    pst.reset((char*)input_buf);
    reset_input();

    return -1;
  }

  int msg_len = pst.c - (char*)input_buf + pst.content_len;
  DBG("received msg:\n%.*s",msg_len,input_buf);

  sip_msg* s_msg = new sip_msg((const char*)input_buf,msg_len);
  pst.reset((char*)input_buf);
  reset_input();

  copy_peer_addr(&s_msg->remote_ip);
  copy_addr_to(&s_msg->local_ip);

  s_msg->local_socket = this;
  inc_ref(this);

  // pass message to the parser / transaction layer
  trans_layer::instance()->received_msg(s_msg);

  return 0;
}

void tcp_trsp_socket::trigger_write_cycle()
{
  event_add(&write_ev,NULL);
}

void tcp_trsp_socket::on_write()
{
  DBG("on_write (connected = %i)",connected);

  // async close mechanism
  if(is_closed()) {
    close();
    return;
  }

  send_q_mut.lock();
  while(!send_q.empty()) {
    msg_buf* msg = send_q.front();
    send_q.pop_front();
    send_q_mut.unlock();    

    if(!msg || !msg->bytes_left()) {
      delete msg;
      send_q_mut.lock();
      continue;
    }

    // send msg
    int bytes = write(sd,msg->cursor,msg->bytes_left());
    if(bytes < 0) {
      DBG("error on write: %i",bytes);
      switch(errno){
      case EINTR:
      case EAGAIN: // would block
	send_q_mut.lock();
	send_q.push_front(msg);
	send_q_mut.unlock();
	break;

      default: // unforseen error: close connection
	ERROR("unforseen error: close connection (%i/%s)",errno,strerror(errno));
	close();
	break;
      }

      return;
    }

    DBG("bytes written: <%.*s>",bytes,msg->cursor);

    if(bytes < msg->bytes_left()) {
      msg->cursor += bytes;
      send_q_mut.lock();
      send_q.push_front(msg);
      send_q_mut.unlock();
      return;
    }

    if(!msg->bytes_left()) {
      delete msg;
    }
    
    send_q_mut.lock();
  }

  send_q_mut.unlock();
}

tcp_server_socket::tcp_server_socket(unsigned short if_num)
  : trsp_socket(if_num,0),
    evbase(NULL)
{
}

static void on_trigger_callback(int fd, short ev, void* arg)
{
  ((tcp_server_socket*)arg)->write_cycle();
}

int tcp_server_socket::bind(const string& bind_ip, unsigned short bind_port)
{
  if(sd){
    WARN("re-binding socket\n");
    close(sd);
  }

  if(send_trigger.init() < 0) {
    ERROR("could not initialize send-trigger");
    return -1;
  }

  if(am_inet_pton(bind_ip.c_str(),&addr) == 0){
	
    ERROR("am_inet_pton(%s): %s\n",bind_ip.c_str(),strerror(errno));
    return -1;
  }
    
  if( ((addr.ss_family == AF_INET) && 
       (SAv4(&addr)->sin_addr.s_addr == INADDR_ANY)) ||
      ((addr.ss_family == AF_INET6) && 
       IN6_IS_ADDR_UNSPECIFIED(&SAv6(&addr)->sin6_addr)) ){

    ERROR("Sorry, we cannot bind to 'ANY' address\n");
    return -1;
  }

  am_set_port(&addr,bind_port);

  if((sd = socket(addr.ss_family,SOCK_STREAM,0)) == -1){
    ERROR("socket: %s\n",strerror(errno));
    return -1;
  } 

  int true_opt = 1;
  if(setsockopt(sd, SOL_SOCKET, SO_REUSEADDR,
		(void*)&true_opt, sizeof (true_opt)) == -1) {
    
    ERROR("%s\n",strerror(errno));
    close(sd);
    return -1;
  }

  if(ioctl(sd, FIONBIO , &true_opt) == -1) {
    ERROR("setting non-blocking: %s\n",strerror(errno));
    close(sd);
    return -1;
  }

  if(::bind(sd,(const struct sockaddr*)&addr,SA_len(&addr)) < 0) {

    ERROR("bind: %s\n",strerror(errno));
    close(sd);
    return -1;
  }

  if(::listen(sd, 16) < 0) {
    ERROR("listen: %s\n",strerror(errno));
    close(sd);
    return -1;
  }

  port = bind_port;
  ip   = bind_ip;

  DBG("TCP transport bound to %s/%i\n",ip.c_str(),port);

  return 0;
}

static void on_accept(int fd, short ev, void* arg)
{
  tcp_server_socket* trsp = (tcp_server_socket*)arg;
  trsp->on_accept(fd,ev);
}

void tcp_server_socket::add_event(struct event_base *evbase)
{
  this->evbase = evbase;

  event_set(&ev_accept, sd, EV_READ|EV_PERSIST, ::on_accept, (void *)this);
  event_base_set(evbase, &ev_accept);
  event_add(&ev_accept, NULL); // no timeout

  event_set(&ev_trigger, send_trigger.get_read_fd(), EV_READ|EV_PERSIST, 
	    ::on_trigger_callback, this);
  event_base_set(evbase, &ev_trigger);
  event_add(&ev_trigger, NULL); // no timeout
}

void tcp_server_socket::add_connection(tcp_trsp_socket* client_sock)
{
  string conn_id = client_sock->get_peer_ip()
    + ":" + int2str(client_sock->get_peer_port());

  DBG("new TCP connection from %s:%u",
      client_sock->get_peer_ip().c_str(),
      client_sock->get_peer_port());

  connections_mut.lock();

  map<string,tcp_trsp_socket*>::iterator sock_it = connections.find(conn_id);
  if(sock_it != connections.end()) {
    dec_ref(sock_it->second);
    sock_it->second = client_sock;
  }
  else {
    connections[conn_id] = client_sock;
  }
  inc_ref(client_sock);

  connections_mut.unlock();
}

void tcp_server_socket::remove_connection(tcp_trsp_socket* client_sock)
{
  string conn_id = client_sock->get_peer_ip()
    + ":" + int2str(client_sock->get_peer_port());

  DBG("removing TCP connection from %s:%u",
      client_sock->get_peer_ip().c_str(), client_sock->get_peer_port());

  connections_mut.lock();

  map<string,tcp_trsp_socket*>::iterator sock_it = connections.find(conn_id);
  if(sock_it != connections.end()) {
    dec_ref(sock_it->second);
    connections.erase(sock_it);
  }

  connections_mut.unlock();
}

void tcp_server_socket::on_accept(int sd, short ev)
{
  sockaddr_storage src_addr;
  socklen_t        src_addr_len = sizeof(sockaddr_storage);

  int connection_sd = accept(sd,(sockaddr*)&src_addr,&src_addr_len);
  if(connection_sd < 0) {
    WARN("error while accepting connection");
    return;
  }

  int true_opt = 1;
  if(ioctl(connection_sd, FIONBIO , &true_opt) == -1) {
    ERROR("could not make new connection non-blocking: %s\n",strerror(errno));
    close(connection_sd);
    return;
  }

  // in case of thread pooling, do following in worker thread
  tcp_trsp_socket* client_sock =
    new tcp_trsp_socket(this,connection_sd,&src_addr,evbase); 
  add_connection(client_sock);
}

int tcp_server_socket::send(const sockaddr_storage* sa, const char* msg, const int msg_len)
{
  char host_buf[NI_MAXHOST];
  string dest = am_inet_ntop(sa,host_buf,NI_MAXHOST);
  dest += ":" + int2str(am_get_port(sa));

  int ret=0;
  connections_mut.lock();
  map<string,tcp_trsp_socket*>::iterator sock_it = connections.find(dest);
  if(sock_it != connections.end()) {
    ret = sock_it->second->send(sa,msg,msg_len);
  }
  else {
    tcp_trsp_socket* new_sock = new tcp_trsp_socket(this,-1,sa,evbase);
    connections[dest] = new_sock;
    inc_ref(new_sock);
    ret = new_sock->send(sa,msg,msg_len);
  }
  
  connections_mut.unlock();

  return ret;
}

void tcp_server_socket::trigger_write(tcp_trsp_socket* sock)
{
  send_q_mut.lock();
  send_q.push_back(sock);
  send_q_mut.unlock();

  send_trigger.trigger();
}

void tcp_server_socket::write_cycle()
{
  send_trigger.clear_trigger();

  send_q_mut.lock();
  while(!send_q.empty()) {
    send_q.front()->trigger_write_cycle();
    send_q.pop_front();
  }
  send_q_mut.unlock();
}


/** @see trsp_socket */

tcp_trsp::tcp_trsp(tcp_server_socket* sock)
    : transport(sock)
{
  evbase = event_base_new();
  sock->add_event(evbase);
}

tcp_trsp::~tcp_trsp()
{
  if(evbase) {
    event_base_free(evbase);
  }
}

/** @see AmThread */
void tcp_trsp::run()
{
  int server_sd = sock->get_sd();
  if(server_sd <= 0){
    ERROR("Transport instance not bound\n");
    return;
  }

  INFO("Started SIP server TCP transport on %s:%i\n",
       sock->get_ip(),sock->get_port());

  /* Start the event loop. */
  event_base_dispatch(evbase);
}

/** @see AmThread */
void tcp_trsp::on_stop()
{
}

