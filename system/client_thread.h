#ifndef _CLIENT_THREAD_H_
#define _CLIENT_THREAD_H_

#include "global.h"

#if VIEW_CHANGES == true || LOCAL_FAULT
#include "message.h"
#endif

class Workload;

class ClientThread : public Thread
{
public:
    RC run();

#if VIEW_CHANGES == true
    void resend_msg(ClientQueryBatch *symsg);
#endif

    void setup();
    void send_key();

private:
    uint64_t last_send_time;
    uint64_t send_interval;
    uint64_t get_next_txn_id();
    uint64_t get_estimated_exec_time_interval();

    // peter: calculate which proxy the client should send the request to
    uint64_t calculate_send_proxy();
};

#endif
