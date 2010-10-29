/*
    Copyright (C) Anton Zagorskiy amberovsky@gmail.com
    Oyster-Telecom Laboratory

    Published under BSD License
*/
#ifndef _CALLCENTER_H_
#define _CALLCENTER_H_
#include "AmApi.h"
#include "AmAudioFile.h"
#include "AmB2BForkSession.h"

#include <string>
#include <mysql++/mysql++.h>

class CallCenterFactory: public AmSessionFactory
{
public:
    static string db_url;
    static string db_user;
    static string db_password;
    static string db_name;
    static string audio_base_dir;

    CallCenterFactory(const string& _app_name);
    ~CallCenterFactory();

    int onLoad();
    AmSession* onInvite(const AmSipRequest& req);

    static CallCenterFactory* instance;
};


class CallCenterDialog: public AmB2BForkCallerSession
{
    AmAudioFile w_file, b_file;
    AmPlaylist playlist;

    mysqlpp::Connection mysql_connection;
    string ring_group_id;
    int busy_check;
    string service_header;

public:
    CallCenterDialog(const string ring_group_id);

    virtual void process(AmEvent* event);
    virtual void onSessionStart(const AmSipRequest& req);
    virtual void onBusyTimer ();
    virtual void onCallStarted (const CalleeInfo& callee_info) { AmB2BForkCallerSession::onCallStarted(callee_info); };
    virtual void onCallFailed (const CalleeInfo& callee_info) { AmB2BForkCallerSession::onCallFailed(callee_info); };
};

#endif
