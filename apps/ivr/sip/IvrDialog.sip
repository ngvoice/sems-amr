

class IvrDialog
{
%TypeHeaderCode
#include "../Ivr.h"
%End

public:

    AmSipDialog dlg;

    IvrDialog();
    ~IvrDialog();

    int acceptAudio(const string& /* body */,
		    const string& /* hdrs */,
		    string* /Out/ /* sdp_reply */);

    void setStopped();

    virtual void onInvite(const AmSipRequest& /* req */);
    virtual void onSessionStart(const AmSipRequest& /* req */);
    virtual void onBye(const AmSipRequest& /* req */);
    virtual void onCancel();

    virtual void onDtmf(int /* event */, int /* duration_msec */);

    virtual void onOtherBye(const AmSipRequest& /* req */);
    virtual void onOtherReply(const AmSipReply& /* r */);

private:
    IvrDialog(const IvrDialog&);
};
