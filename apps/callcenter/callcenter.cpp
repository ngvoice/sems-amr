/*
    Copyright (C) Anton Zagorskiy amberovsky@gmail.com
    Oyster-Telecom Laboratory

    Published under BSD License
*/

#include "callcenter.h"
#include "AmPlugIn.h"

#define MOD_NAME "callcenter"

string CallCenterFactory::db_url = "";
string CallCenterFactory::db_user = "";
string CallCenterFactory::db_password = "";
string CallCenterFactory::db_name = "";
string CallCenterFactory::audio_base_dir = "";

CallCenterFactory* CallCenterFactory::instance = NULL;
EXPORT_SESSION_FACTORY(CallCenterFactory,MOD_NAME);


CallCenterFactory::CallCenterFactory(const string& _app_name)
  : AmSessionFactory(_app_name)
{
    instance = this;
};


CallCenterFactory::~CallCenterFactory()
{
};



int CallCenterFactory::onLoad()
{
    AmConfigReader cfg;

    if(cfg.loadFile(AmConfig::ModConfigPath + "callcenter.conf"))
    {
	ERROR("can not load configuration file\n");
	return -1;
    };


    db_url = cfg.getParameter("db_url");
    if (db_url.length() == 0)
    {
	ERROR("a parameter 'db_url' didn't found in the configuration file\n");
	return -1;
    };

    db_user = cfg.getParameter("db_user");
    if (db_user.length() == 0)
    {
	ERROR("a parameter 'db_user' didn't found in the configuration file\n");
	return -1;
    };

    db_password = cfg.getParameter("db_password");
    if (db_password.length() == 0)
    {
	ERROR("a parameter 'db_password' didn't found in the configuration file\n");
	return -1;
    };

    db_name = cfg.getParameter("db_name");
    if (db_name.length() == 0)
    {
	ERROR("a parameter 'db_name' didn't found in the configuration file\n");
	return -1;
    };

    audio_base_dir = cfg.getParameter("audio_base_dir");
    if (audio_base_dir.length() == 0)
    {
	ERROR("a parameter 'audio_base_dir' didn't found in the configuration file\n");
	return -1;
    };

    DBG("CallCenter Loaded\n");
    return 0;
};


AmSession* CallCenterFactory::onInvite(const AmSipRequest& req)
{
    return new CallCenterDialog(req.user);
};


CallCenterDialog::CallCenterDialog(const string ring_group_id)
  : playlist(this), ring_group_id(ring_group_id),
    AmB2BForkCallerSession()
{
};


void CallCenterDialog::onSessionStart(const AmSipRequest& req)
{
    AmB2BForkCallerSession::onSessionStart(req);

    string remote_party, remote_uri, fork_tmp;

    try
    {
	mysql_connection.connect(CallCenterFactory::db_name.c_str(), CallCenterFactory::db_url.c_str(), CallCenterFactory::db_user.c_str(), CallCenterFactory::db_password.c_str());
	
	mysqlpp::Query ring_groups_query = mysql_connection.query("SELECT * FROM ring_groups WHERE id=" + ring_group_id);
	mysqlpp::StoreQueryResult ring_groups_res = ring_groups_query.store();
	
	reinvite_after = atoi(ring_groups_res[0]["reinvite_after"]);
	requeue_after = atoi(ring_groups_res[0]["requeue_after"]);
	busy_check = atoi(ring_groups_res[0]["busy_check"]);
	ring_groups_res[0]["service_header"].to_string(service_header);
	service_header += "\r\n";
	ring_groups_res[0]["fork_type"].to_string(fork_tmp);
	
	mysqlpp::Query ring_users_query = mysql_connection.query(" SELECT * FROM ring_users WHERE group_id=" + ring_group_id);
	mysqlpp::StoreQueryResult ring_users_res = ring_users_query.store();
	
	for (size_t i = 0; i < ring_users_res.num_rows(); i++)
	{
	    ring_users_res[i]["remote_party"].to_string(remote_party);
	    ring_users_res[i]["remote_uri"].to_string(remote_uri);
	    addCallee(remote_party, remote_uri, req.from, req.from_uri, service_header);
	};
    }
    catch (const mysqlpp::Exception& er)
    {
	ERROR("MySQL++ error: %s\n", er.what());
	dlg.bye();
	setStopped();

	return ;
    };


    setBusyTimer(busy_check);

    if (fork_tmp == "0")
	setForkMode(ForkParallel);
    else
	setForkMode(ForkSerial);

    if (connectCallees() != E_OK)
    {
	dlg.bye();
	setStopped();
	
	return;
    };

    if (w_file.open(CallCenterFactory::audio_base_dir + ring_group_id + "/" + "waiting.wav",AmAudioFile::Read))
	throw AmSession::Exception(0, "CallCenterDialog::onSessionStart: Cannot open file (waiting)", "");

    if (b_file.open(CallCenterFactory::audio_base_dir + ring_group_id + "/" + "all_busy.wav",AmAudioFile::Read))
	throw AmSession::Exception(0, "CallCenterDialog::onSessionStart: Cannot open file (all_busy)", "");

    setInOut(&playlist, &playlist);
    playlist.addToPlaylist(new AmPlaylistItem(&w_file, NULL));
};



void CallCenterDialog::process(AmEvent* event)
{
    AmAudioEvent* audio_event = dynamic_cast<AmAudioEvent*>(event);

    if(audio_event && (audio_event->event_id == AmAudioEvent::noAudio))
    {
	w_file.rewind();
	playlist.addToPlayListFront(new AmPlaylistItem(&w_file, NULL));
	return;
    };
    AmB2BForkCallerSession::process(event);
};


void CallCenterDialog::onBusyTimer ()
{
    b_file.rewind();
    playlist.addToPlayListFront(new AmPlaylistItem(&b_file, NULL));
    setBusyTimer(busy_check);
};
