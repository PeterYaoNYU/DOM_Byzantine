#ifndef _CHECKTHREAD_H_
#define _CHECKTHREAD_H_

#include "global.h"
#include "message.h"
#include "recv_proxy_pq_obj.h"
#include "thread.h"

class CheckThread: public Thread
{
public:
    RC run();
    RC recv_proxy_check_loop();
    void setup();

private:
    RC dispatch_request_to_replicas(DeadlinePQObj *deadline_pq_obj);
    bool succ;
    DeadlinePQObj top_batch;
    uint64_t get_next_txn_id();
};

#endif // _CHECKTHREAD_H_