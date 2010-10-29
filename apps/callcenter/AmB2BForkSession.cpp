/*
    Copyright (C) Anton Zagorskiy amberovsky@gmail.com
    Oyster-Telecom Laboratory

    Published under BSD License
*/
#include "AmB2BForkSession.h"
#include "AmMediaProcessor.h"
#include "plug-in/session_timer/UserTimer.h"

AmDynInvoke* AmB2BForkCallerSession::user_timer = NULL;
const int AmB2BForkSession::REQUEUE_TIMER_ID = -1;
const int AmB2BForkSession::BUSY_CHECK_TIMER_ID = -2;

AmB2BForkSession::AmB2BForkSession  (const string& other_local_tag, const bool proxy_rtp)
    :AmSession(), other_id(other_local_tag), proxy_rtp(proxy_rtp), connector(NULL)
{
    RTPStream()->setPlayoutType(ADAPTIVE_PLAYOUT);
};



AmB2BForkSession::~AmB2BForkSession ()
{
};



void AmB2BForkSession::disconnectAudioSession ()
{
    if (!connector)
	return;

    connector->disconnectSession(this);
};



void AmB2BForkSession::connectAudioSession ()
{
    if (!connector)
	return;

    connector->connectSession(this);
    AmMediaProcessor::instance()->addSession(this, callgroup);
};



void AmB2BForkSession::process (AmEvent * ev)
{
    B2BForkEvent* b2bf_e = dynamic_cast<B2BForkEvent*>(ev);

    if (b2bf_e)
	onB2BForkEvent(b2bf_e);
    else
	AmSession::process(ev);
};



void AmB2BForkSession::terminateLeg ()
{
    dlg.bye();
    disconnectAudioSession();
    setStopped();
};



void AmB2BForkSession::onB2BForkEvent (B2BForkEvent* ev)
{
    switch (ev->event_id)
    {
	case B2BForkTerminateLeg:
	    terminateLeg();
	    break;
    };
};



void AmB2BForkSession::relayEvent (AmEvent * ev)
{
    if (!other_id.empty())
	AmSessionContainer::instance()->postEvent(other_id, ev);
};




void AmB2BForkSession::onBye (const AmSipRequest& req)
{
    terminateOtherLeg();
    disconnectAudioSession();
    setStopped();
};



AmB2BForkCallerSession::AmB2BForkCallerSession (const int reinvite_after, const int requeue_after, const ForkType fork_type, const bool accept_early_media, const bool proxy_rtp)
    :AmB2BForkSession("", proxy_rtp), reinvite_after(reinvite_after), requeue_after(requeue_after), fork_type(fork_type), accept_early_media(accept_early_media), _callee_status(None)
{
    if (proxy_rtp)
	connector = new AmSessionAudioConnector();

    AmDynInvokeFactory* user_timer_factory = AmPlugIn::instance()->getFactory4Di("user_timer");

    if ((!user_timer_factory) || (!(user_timer = user_timer_factory->getInstance())))
    {
	ERROR("could not load user_timer. Load a user_timer implemetation module\n");
	throw AmSession::Exception(0, "could not load user_timer module", "");
    };

    callees_info.set(new CalleeInfoVector());
    callee_status.set(_callee_status);
};



void AmB2BForkSession::terminateOtherLeg ()
{
    relayEvent(new B2BForkEvent(B2BForkTerminateLeg));
    other_id = "";
};



AmB2BForkCallerSession::~AmB2BForkCallerSession()
{
    callees_info.lock();

    for (std::vector<CalleeInfo*>::iterator callees_iter = callees_info.unsafe_get()->begin(); callees_iter != callees_info.unsafe_get()->end(); callees_iter++)
    {
	CalleeInfo* callee = (*callees_iter);
	delete callee;
    };

    callees_info.unlock();

    delete callees_info.unsafe_get();

    delete connector;
};



ForkError AmB2BForkCallerSession::setForkMode (const ForkType _fork_type)
{
    callee_status.lock();
    if (callee_status.unsafe_get() != None)
    {
	DBG("connectCallee() fails because of it is forking now or a session already established\n");
	callee_status.unlock();
	return E_FORKING_PROGRESS;
    };

    callee_status.unlock();

    fork_type = _fork_type;

    return E_OK;
};


void AmB2BForkCallerSession::onBeforeDestroy ()
{
    if (connector)
	connector->waitReleased();
};



void AmB2BForkCallerSession::onBye (const AmSipRequest& req)
{
    callees_info.lock();

    for (std::vector<CalleeInfo*>::iterator callees_iter = callees_info.unsafe_get()->begin(); callees_iter != callees_info.unsafe_get()->end(); callees_iter++)
    {
	if ((*callees_iter)->callee_status != None)
	    AmSessionContainer::instance()->postEvent((*callees_iter)->session_id, new B2BForkEvent(B2BForkTerminateLeg));
    };

    callees_info.unlock();

    disconnectAudioSession();
    setStopped();
};



void AmB2BForkCallerSession::terminateOtherLeg ()
{
    callee_status.lock();
    if (callee_status.unsafe_get() != None)
	AmB2BForkSession::terminateOtherLeg();

  _callee_status = None;
  callee_status.unlock();
};



void AmB2BForkCallerSession::terminateOtherLegs ()
{
    callees_info.lock();

    for (std::vector<CalleeInfo*>::iterator callees_iter = callees_info.unsafe_get()->begin(); callees_iter != callees_info.unsafe_get()->end(); callees_iter++)
    {
	if ((*callees_iter)->callee_status != None)
	{
	    if ((*callees_iter)->session_id != other_id)
		AmSessionContainer::instance()->postEvent((*callees_iter)->session_id, new B2BForkEvent(B2BForkTerminateLeg));
	};
    };

    callees_info.unlock();
};



void AmB2BForkCallerSession::process (AmEvent* ev)
{
    AmTimeoutEvent* te = dynamic_cast<AmTimeoutEvent*>(ev);

    if (te)
    {
	int timer_id = te->data.get(0).asInt();
	switch (timer_id)
	{
	    case BUSY_CHECK_TIMER_ID:
		callee_status.lock();
		if ( (callee_status.unsafe_get() != Connected) && (callee_status.unsafe_get() != Early) )
		    onBusyTimer();
		callee_status.unlock();
		break;
	    case REQUEUE_TIMER_ID:
		doFork();
		break;
	    default:
//		AmArg di_arg, ret;
//		di_arg.push(timer_id);
//		di_arg.push(dlg.local_tag.c_str());
//		user_timer->invoke("removeTimer", di_arg, ret);
	
		doFork(timer_id);
	};
    }
    else
	AmB2BForkSession::process(ev);
};



void AmB2BForkCallerSession::onB2BForkEvent (B2BForkEvent* ev)
{
    switch(ev->event_id)
    {
	case B2BForkConnectAudio:
	    {
		callee_status.lock();
		_callee_status = Connected;
		callee_status.unlock();
		DBG("ConnectAudio event received from other leg\n");
		B2BForkConnectAudioEvent* ca = dynamic_cast<B2BForkConnectAudioEvent*>(ev);
		if (!ca)
		    return;
		
		ca->callee_info->callee_status = Connected;
		other_id = ca->callee_info->session_id;
		terminateOtherLegs();
		connectAudioSession();
		
		if (!connector)
		{
		    setMute(true);
		    RTPStream()->close();
		    AmMediaProcessor::instance()->removeSession(this);
		    string sdp_body;
		    sdp.genResponse(ca->other_ip, ca->other_port, sdp_body);
		    dlg.reinvite(get_100rel_hdr(reliable_1xx), SIP_APPLICATION_SDP, sdp_body);
		}
	    };
	    break;

	case B2BForkConnectEarlyAudio:
	    {
		DBG("ConnectEarlyAudio event received from other leg\n");
		if (accept_early_media)
		{
		    B2BForkConnectEarlyAudioEvent* ca = dynamic_cast<B2BForkConnectEarlyAudioEvent*>(ev);
		    if (!ca)
			return;
			
		    other_id = ca->callee_info->session_id;
		    callee_status.lock();
		    callee_status = Connected;
		    callee_status.unlock();
		    terminateOtherLegs();
		    connectAudioSession();
		};
	    };
	    break;

	case B2BForkConnectOtherLegException:
	    ERROR("terminating");
	    terminateOtherLegs();
	    terminateLeg();
	    dlg.bye();
	    setStopped();
	    break;
	case B2BForkConnectOtherLegFailed:
	    {
		B2BForkConnectOtherLegFailedEvent * co = dynamic_cast<B2BForkConnectOtherLegFailedEvent*>(ev);
		onCallFailed(*(ev->callee_info));
		callees_info.lock();
		co->callee_info->callee_status = None;
		AmSessionContainer::instance()->postEvent(co->callee_info->session_id, new B2BForkEvent(B2BForkTerminateLeg));
		
		if (fork_type == ForkParallel)
		{
		    AmArg di_args,ret;
		    di_args.push(co->callee_info->timer_id);
		    di_args.push((int)reinvite_after);
		    di_args.push(dlg.local_tag.c_str());
		    user_timer->invoke("setTimer", di_args, ret);
		}
		else
		{
		    int next_callee_id;
		
		    if ((int)callees_info.unsafe_get()->size() == co->callee_info->timer_id + 1)
		    {
			callees_info.unlock();
			//onCalleeQueuePassed();
			doFork();
		    }
		    else
		    {
			next_callee_id = co->callee_info->timer_id + 1;
			callees_info.unlock();
			doFork(next_callee_id);
		    };
		};
		
	    };
	    break;

	case B2BForkOtherLegRinging:
		DBG("callee is ringing.\n");
		onCalleeRinging((*ev->callee_info));
	    break;
	    
	default:
	    AmB2BForkSession::onB2BForkEvent(ev);
    };
};



void AmB2BForkCallerSession::doFork ( const int callee_index )
{
    callees_info.lock();

    if (fork_type == ForkParallel)
    {
	if (callee_index != -1)
	{
	    setupCalleeSession(createCalleeSession(), *(callees_info.unsafe_get()->at(callee_index)));
	}
	else
	{
	    for (std::vector<CalleeInfo*>::iterator callee_iter = callees_info.unsafe_get()->begin(); (callee_iter != callees_info.unsafe_get()->end()) && (callee_status.get() != Connected); callee_iter++)
		setupCalleeSession(createCalleeSession(), *(*callee_iter));
	};
    }
    else
    {
	if (callee_index != -1)
	    setupCalleeSession(createCalleeSession(), *(callees_info.unsafe_get()->at(callee_index)));
	else
	    setupCalleeSession(createCalleeSession(), *(callees_info.unsafe_get()->at(0)));
    };
	
    callees_info.unlock();
};



ForkError AmB2BForkCallerSession::connectCallees ()
{
    if (callees_info.get()->size() == 0)
    {
	DBG("connectCallee() fails because of there are no callees in the list\n");
	return E_NO_CALLEE;
    };

    callee_status.lock();
    if (callee_status.unsafe_get() != None)
    {
	DBG("connectCallee() fails because of it is forking now or a session already established\n");
	callee_status.unlock();
	return E_FORKING_PROGRESS;
    };

    _callee_status = NoReply;
    callee_status.unlock();

    doFork();

    return E_OK;
};


void AmB2BForkCallerSession::abortConnectCallees ()
{
    terminateOtherLegs();
    terminateOtherLeg();
};


void AmB2BForkCallerSession::onSessionStart (const AmSipRequest& req)
{
    if (!proxy_rtp)
	invite_req = req;
	
    AmB2BForkSession::onSessionStart(req);
};


// Add new callee to callee's list
void AmB2BForkCallerSession::addCallee (const string& remote_party, const string& remote_uri, const string& local_party, const string& local_uri, const string& headers)
{
    callees_info.lock();

    CalleeInfo* callee_info = new CalleeInfo(remote_party, remote_uri, local_party, local_uri, headers, callees_info.unsafe_get()->size());
    callees_info.unsafe_get()->push_back(callee_info);

    callees_info.unlock();
};



void AmB2BForkCallerSession::relayEvent(AmEvent* ev)
{
    if(other_id.empty())
    {
	DBG("broadcasting event\n");
	callees_info.lock();
	
	for (std::vector<CalleeInfo*>::iterator callees_iter = callees_info.unsafe_get()->begin(); callees_iter != callees_info.unsafe_get()->end(); callees_iter++)
	{
	    if ((*callees_iter)->callee_status != None)
		AmSessionContainer::instance()->postEvent((*callees_iter)->session_id, ev);
	};
	
	callees_info.unlock();
    }
    else
	AmB2BForkSession::relayEvent(ev);
};



void AmB2BForkCallerSession::setupCalleeSession(AmB2BForkCalleeSession* callee_session, CalleeInfo& callee_info)
{
    if (NULL == callee_session)
    {
	DBG("callee session didn't created\n");
	return;
    };


    B2BForkConnectLegEvent* ev = new B2BForkConnectLegEvent(&callee_info, getLocalTag(), &invite_req);
    ev->callee_info->callee_status = NoReply;

    callee_info.session_id = AmSession::getNewId();

    AmSipDialog& callee_dlg = callee_session->dlg;

    callee_dlg.callid       = AmSession::getNewId() + "@" + AmConfig::LocalIP;
    callee_dlg.local_tag    = ev->callee_info->session_id; 

    callee_dlg.local_party  = dlg.remote_party;
    callee_dlg.remote_party = dlg.local_party;
    callee_dlg.remote_uri   = dlg.local_uri;

    callee_session->start();


    AmSessionContainer* sess_cont = AmSessionContainer::instance();
    sess_cont->addSession(callee_info.session_id,callee_session);
    AmSessionContainer::instance()->postEvent(callee_info.session_id, ev);

    onCallStarted(callee_info);
};



AmB2BForkCalleeSession* AmB2BForkCallerSession::createCalleeSession()
{
    return new AmB2BForkCalleeSession(getLocalTag(), connector);
};



AmB2BForkCalleeSession::AmB2BForkCalleeSession(const string& other_local_tag, AmSessionAudioConnector* callers_connector)
  : AmB2BForkSession(other_local_tag)
{
    connector = callers_connector;
};



AmB2BForkCalleeSession::~AmB2BForkCalleeSession() 
{
};



void AmB2BForkCalleeSession::onBeforeDestroy()
{
    if ( ((self_callee->callee_status == Connected) || (self_callee->callee_status == Early)) && (connector) )
	connector->release();
};



void AmB2BForkCalleeSession::onB2BForkEvent (B2BForkEvent* ev)
{
    if(ev->event_id == B2BForkConnectLeg)
    {
	try
	{
	    B2BForkConnectLegEvent* co_ev = dynamic_cast<B2BForkConnectLegEvent*>(ev);
	    if (co_ev)
	    {
		MONITORING_LOG4(getLocalTag().c_str(), "b2b_leg", other_id.c_str(), "from", co_ev->callee_info->local_party.c_str(), "to", co_ev->callee_info->remote_party.c_str(), "ruri", co_ev->callee_info->remote_uri.c_str());

		dlg.remote_party = co_ev->callee_info->remote_party;
		dlg.remote_uri   = co_ev->callee_info->remote_uri;

		self_callee = co_ev->callee_info;
		if (connector)
		{
		    setCallgroup(co_ev->callgroup);
		    setNegotiateOnReply(true);

		    if (sendInvite(co_ev->callee_info->headers))
			throw string("INVITE could not be sent\n");
		}
		else
		{
		    setCallgroup(co_ev->callgroup);
		    setNegotiateOnReply(true);
		    dlg.sendRequest(SIP_METH_INVITE, co_ev->invite_req->content_type, co_ev->invite_req->body, co_ev->callee_info->headers, SIP_FLAGS_VERBATIM);
		    //setMute(true);
		};
		return;
	    };
	}	
	catch(const AmSession::Exception& e)
	{
	    ERROR("%i %s\n",e.code,e.reason.c_str());
	    relayEvent(new B2BForkConnectOtherLegExceptionEvent(self_callee, e.code,e.reason));
	    setStopped();
	}
	catch(const string& err)
	{
	    ERROR("startSession: %s\n",err.c_str());
 	    relayEvent(new B2BForkConnectOtherLegExceptionEvent(self_callee, 500, err));
	    setStopped();
	}
	catch(...){
	    ERROR("unexpected exception\n");
	    relayEvent(new B2BForkConnectOtherLegExceptionEvent(self_callee, 500,"unexpected exception"));
	    setStopped();
	};
    };

    AmB2BForkSession::onB2BForkEvent(ev);
};


void AmB2BForkCalleeSession::onEarlySessionStart(const AmSipReply& rep)
{
    if (connector)
    {
	connector->block();
	connectAudioSession();
    };

    self_callee->callee_status = Early;

    relayEvent(new B2BForkConnectEarlyAudioEvent(self_callee));
};


void AmB2BForkCalleeSession::onSessionStart(const AmSipReply& rep)
{
    if (self_callee->callee_status != Connected)
    {
	self_callee->callee_status = Connected;
	if (connector)
	{
	    connector->block();
	    connectAudioSession();
	};
    };
    relayEvent(new B2BForkConnectAudioEvent(self_callee, RTPStream()->getRHost(), RTPStream()->getRPort()));
};



void AmB2BForkCalleeSession::onSipReply(const AmSipReply& rep, int old_dlg_status, const string& trans_method)
{
    AmSession::onSipReply(rep, old_dlg_status, trans_method);
    int status = dlg.getStatus();
 
    if ( (old_dlg_status == AmSipDialog::Pending) && (status == AmSipDialog::Disconnected) )
    {
	DBG("callee session creation failed. notifying caller session.\n");
	DBG("this happened with reply: %d.\n", rep.code);
	relayEvent(new B2BForkConnectOtherLegFailedEvent(self_callee, rep.code, rep.reason));
    }
    else if ( (status == AmSipDialog::Pending) && (rep.code == 180) )
	 {
	    relayEvent(new B2BForkOtherLegRingingEvent(self_callee));
	 };
};



void AmB2BForkCalleeSession::terminateLeg()
{
    if (self_callee->callee_status == Connected)
	disconnectAudioSession();

    dlg.bye();
    setStopped();
};


/*
void AmB2BForkCallerSession::onCalleeQueuePassed ()
{
    DBG("all callees unanswer, requeue\n");
    AmArg di_args,ret;
    di_args.push(REQUEUE_TIMER_ID);
    di_args.push(requeue_after);
    di_args.push(dlg.local_tag.c_str());
    user_timer->invoke("setTimer", di_args, ret);
};
*/



void AmB2BForkCallerSession::onCallStarted (const CalleeInfo& callee)
{
    DBG("onCallStarted [%s]", callee.remote_uri.c_str());
};



void AmB2BForkCallerSession::onCallFailed (const CalleeInfo& callee)
{
    DBG("onCallFailed [%s]\n", callee.remote_uri.c_str());
};



void AmB2BForkCallerSession::setBusyTimer (const int timer_delay)
{
    AmArg di_args,ret;
    di_args.push(BUSY_CHECK_TIMER_ID);
    di_args.push(timer_delay);
    di_args.push(dlg.local_tag.c_str());
    user_timer->invoke("setTimer", di_args, ret);
};



void AmB2BForkCallerSession::onBusyTimer ()
{
};



void AmB2BForkCallerSession::onCalleeRinging (const CalleeInfo&)
{
};
