#include "call.hpp"


class deadcall : public virtual task, public virtual listener
{
public:
    deadcall(const char *id, const char * reason);
    ~deadcall();

    bool process_incoming(const char* msg, const struct sockaddr_storage *) override;
    bool process_twinSippCom(char* msg) override;
	// WARNING: this pure function should never be used
    char* createSendingMessage(SendingMessage*src, int P_index, char *msg_buffer, int buflen) override { return nullptr; }

    virtual bool run();

    /* When should this call wake up? */
    virtual unsigned int wake();

    /* Dump call info to error log. */
    virtual void dump();

protected:
    unsigned long expiration;
    char *reason;
};
