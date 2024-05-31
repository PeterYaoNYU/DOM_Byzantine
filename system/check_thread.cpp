#include "thread.h"
#include "check_thread.h"

RC CheckThread::run()
{
    DEBUG("Running Check Thread\n");
    tsetup();
    DEBUG("Done setting up on Check Thread\n");

    succ = false;

    while (!simulation->is_done())
    {
        succ = deadline_pq.tryPeek(top_batch);
        while (succ && top_batch.get_deadline() < get_sys_clock()) {
            succ = deadline_pq.tryPop(top_batch);
            if (!succ) {
                break;
            }
            DEBUG("Check Thread: Pop from priority queue with deadline: %ld, current sys clock: %ld, difference: %ld\n", top_batch.get_deadline(), get_sys_clock(), get_sys_clock() - top_batch.get_deadline());
            dispatch_request_to_replicas(&top_batch);
            succ = deadline_pq.tryPeek(top_batch);
        }
    }

    return FINISH;
}
