/*
    Copyright (C) Anton Zagorskiy amberovsky@gmail.com
    Oyster-Telecom Laboratory

    Published under BSD License
*/
#ifndef _AM_B2BForkSession_H
#define _AM_B2BForkSession_H
#include "AmB2ABSession.h"

#include <string>
#include <vector>

using std::string;

typedef enum
{
    None = 0,
    NoReply,
    Early,
    Ringing,
    Connected,
    Forking
} CalleeStatus;



struct CalleeInfo
{
    string remote_party;
    string remote_uri;
    string local_uri;
    string local_party;
    string headers;
    string session_id;
    CalleeStatus callee_status;

    int timer_id;

    CalleeInfo ( const string& remote_party, const string& remote_uri, const string& local_party, const string& local_uri, const string& headers, const int timer_id )
        :remote_party(remote_party), remote_uri(remote_uri), local_party(local_party), local_uri(local_uri), headers(headers), session_id(""), timer_id(timer_id)
    {
        callee_status = None;
    };
};



typedef enum
{
    B2BForkTerminateLeg,
    B2BForkConnectLeg,
    B2BForkConnectEarlyAudio,
    B2BForkConnectAudio,
    B2BForkConnectOtherLegFailed,
    B2BForkConnectOtherLegException,
    B2BForkOtherLegRinging
} B2BForkEventId;


/** \brief base class for event in B2BFork session */
struct B2BForkEvent: public AmEvent
{
    CalleeInfo* callee_info;

    B2BForkEvent (int ev_id, CalleeInfo* callee_info = NULL)
	: AmEvent(ev_id), callee_info(callee_info)
    {};
};





class AmB2BForkCalleeSession;

struct B2BForkConnectLegEvent;

typedef enum
{
    E_OK = 0,
    E_NO_CALLEE,
    E_FORKING_PROGRESS,
} ForkError;


class AmB2BForkSession: public AmSession
{
protected:
    static 	const int REQUEUE_TIMER_ID;
    static const int BUSY_CHECK_TIMER_ID;

    bool proxy_rtp;
    AmSessionAudioConnector* connector;
    string other_id;
public:
    typedef enum
    {
	ForkSerial = 0,
	ForkParallel
    } ForkType;

    AmB2BForkSession ( const string& other_local_tag, const bool proxy_rtp = false);
    virtual ~AmB2BForkSession ();

    virtual void process ( AmEvent* ev );
    virtual void onBye ( const AmSipRequest& req );

    virtual void onB2BForkEvent (B2BForkEvent* ev);
    virtual void disconnectAudioSession ();
    virtual void connectAudioSession ();

    // Terminate established leg
    virtual void terminateLeg ();

    // Terminate pending leg
    virtual void terminateOtherLeg ();
    virtual void relayEvent (AmEvent* ev);

};



class AmB2BForkCallerSession: public AmB2BForkSession
{
private:
    typedef std::vector<CalleeInfo*> CalleeInfoVector;

    AmSharedVar<CalleeInfoVector*> callees_info;

    virtual void setupCalleeSession(AmB2BForkCalleeSession* callee_session, CalleeInfo& callee_info);
    ForkType fork_type;
    void doFork (const int callee_index = -1);

    static AmDynInvoke* user_timer;

    CalleeStatus _callee_status;
    AmSharedVar<CalleeStatus> callee_status;

    bool accept_early_media;
    AmSipRequest invite_req;

public:
    int reinvite_after;
    int requeue_after;


    explicit AmB2BForkCallerSession (const int reinvite_after = 10, const int requeue_after = 10, const ForkType fork_type = ForkParallel, const bool accept_early_media = false, const bool proxy_rtp = true);
    virtual ~AmB2BForkCallerSession( );

    virtual AmB2BForkCalleeSession* createCalleeSession ();
    virtual void relayEvent (AmEvent* ev);

    void connectCallee ();
    ForkError setForkMode (const ForkType fork_type);
    void terminateOtherLeg ();

    virtual void onBye (const AmSipRequest& req);
    virtual void terminateOtherLegs();
    virtual void process (AmEvent* ev);
    virtual void onSessionStart (const AmSipRequest& req);

    void addCallee (const string& remote_party, const string& remote_uri, const string& local_party, const string& local_uri, const string& headers = "");
    ForkError connectCallees ();
    void abortConnectCallees ();

    void setBusyTimer (const int timer_delay);
    virtual void onCallStarted (const CalleeInfo& callee);
    virtual void onCallFailed (const CalleeInfo& callee);
    virtual void onBusyTimer ();
    virtual void onCalleeRinging (const CalleeInfo& callee);

protected:
    void onB2BForkEvent(B2BForkEvent* ev);
    void onBeforeDestroy();
};






/** \brief trigger connecting the audio in B2BFork session */
struct B2BForkConnectAudioEvent: public B2BForkEvent
{
    string other_ip;
    int other_port;

    B2BForkConnectAudioEvent(CalleeInfo* callee_info, string other_ip, int other_port)
	:B2BForkEvent(B2BForkConnectAudio, callee_info), other_ip(other_ip), other_port(other_port)
     {};
};



/** \brief event fired if the other leg is ringing (e.g. busy) */
struct B2BForkOtherLegRingingEvent: public B2BForkEvent
{
    B2BForkOtherLegRingingEvent(CalleeInfo* callee_info)
	:B2BForkEvent(B2BForkOtherLegRinging, callee_info)
    {}
};



/** \brief event fired if an exception occured while creating other leg */
struct B2BForkConnectOtherLegExceptionEvent: public B2BForkEvent
{
    unsigned int code;
    string reason;
    
    B2BForkConnectOtherLegExceptionEvent (CalleeInfo* callee_info, unsigned int code, const string& reason)
	:B2BForkEvent(B2BForkConnectOtherLegException, callee_info), code(code), reason(reason)
   {}
};



/** \brief event fired if the other leg could not be connected (e.g. busy) */
struct B2BForkConnectOtherLegFailedEvent: public B2BForkEvent
{
    unsigned int code;
    string reason;

    B2BForkConnectOtherLegFailedEvent (CalleeInfo* callee_info, unsigned int code, const string& reason)
	:B2BForkEvent(B2BForkConnectOtherLegFailed, callee_info), code(code), reason(reason)
    {};
};



/** \brief trigger connecting the callee leg in B2BFork session */
struct B2BForkConnectLegEvent: public B2BForkEvent
{
    string callgroup;
    AmSipRequest* invite_req;

    B2BForkConnectLegEvent (CalleeInfo* callee_info, const string& callgroup, AmSipRequest* invite_req)
	:B2BForkEvent(B2BForkConnectLeg, callee_info), callgroup(callgroup), invite_req(invite_req)
    {};
};



/** \brief trigger connecting the early audio in B2BFork session */
struct B2BForkConnectEarlyAudioEvent: public B2BForkEvent
{
    B2BForkConnectEarlyAudioEvent (CalleeInfo* callee_info)
	:B2BForkEvent(B2BForkConnectEarlyAudio, callee_info)
    {};
};


class AmB2BForkCalleeSession: public AmB2BForkSession
{
private:
    AmB2BForkCallerSession* caller_session;
    CalleeInfo* self_callee;

public:
    AmB2BForkCalleeSession(const string& other_local_tag, AmSessionAudioConnector* callers_connector);
    ~AmB2BForkCalleeSession();

    void onEarlySessionStart(const AmSipReply& rep);
    void onSessionStart(const AmSipReply& rep);
    void onSipReply(const AmSipReply& rep, int old_dlg_status, const string& trans_method);

protected:
    void onB2BForkEvent (B2BForkEvent* ev);
    void onBeforeDestroy();
    void terminateLeg();
};
#endif
