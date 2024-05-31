#include "thread.h"
#include "check_thread.h"
#include "msg_queue.h"
#include "message.h"

RC CheckThread::run()
{
    DEBUG("Running Check Thread\n");
    tsetup();
    DEBUG("Done setting up on Check Thread\n");

    succ = false;

    while (!simulation->is_done())
    {
        succ = deadline_pq.tryPeek(top_batch);
        // if (succ == true) {
        //     DEBUG("Check Thread: Peek Success\n");
        // }
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


RC CheckThread::dispatch_request_to_replicas(DeadlinePQObj *deadline_pq_obj)
{
    // for calculating hash
    string batchStr;
    std::cout << "Check Thread: dispatching batch requests to replicas from recv proxy from the deadline obj" << std::endl;
    uint64_t client_node_id = deadline_pq_obj->get_client_node_id();
    auto cqrySet = deadline_pq_obj->get_cqrySet();
    DEBUG("THE CQRYSet size is %ld\n", cqrySet.size());

    std::cout << "Dispatching: got the cqryset and the client node id: " << client_node_id << std::endl;

    // create a batch request message
    Message *msg = Message::create_message(BATCH_REQ);
    BatchRequests *breq = (BatchRequests *)msg;
    breq->init(get_thd_id());
    breq->index.init(get_batch_size());

    printf("Done init the batch request in recv proxy\n");

    vector<uint64_t> dest;
    // push back all the destinations 
    for (uint64_t i = 0; i < g_node_cnt; i++) {
       if (i == g_node_id) {
            continue;
        }
        std::cout << "adding destination: " << i << std::endl;
        dest.push_back(i);
    }

    // assign the recv_proxy speculative txn id and batch id
    recv_proxy_txn_assignment_mtx.lock();

    msg->batch_id = ++g_next_set; 
    msg->txn_id = get_next_txn_id();

    recv_proxy_txn_assignment_mtx.unlock();

    std::cout << "Dispatching: assigned the txn id: "<< msg->txn_id <<" and batch id: " << msg->batch_id << std::endl;

    // add the queries to the message
    for (uint64_t i = 0; i < cqrySet.size(); i++) {
        YCSBClientQueryMessage *yqry_to_copy = (YCSBClientQueryMessage *)cqrySet[i];

        // append string representation of this txn
        batchStr += yqry_to_copy->getString();

        if (yqry_to_copy == nullptr) {
            std::cerr << "Error: yqry_to_copy is null!" << std::endl;
            continue; // or handle the error appropriately
        }
        char *brf = (char *)malloc(yqry_to_copy->get_size());
        cqrySet[i]->copy_to_buf(brf);
        Message *tmsg = Message::create_message(brf);
        YCSBClientQueryMessage *yqry = (YCSBClientQueryMessage *)tmsg;

        free(brf);

        breq->hash = calculateHash(batchStr);
        breq->hashSize = breq->hash.length();
        breq->requestMsg[i]= yqry;
        breq->index.add(msg->txn_id+i);
    } 

    // fill the return node: the client to get back to, who will be doing the linearization and the vote check
    msg->return_node_id = client_node_id;

    // send it out to all the replicas
    msg_queue.enqueue(get_thd_id(), breq, dest);
    dest.clear();

    std::cout << "dispatching request to replicas, tid: " << msg->txn_id << " client node id: "<< client_node_id << std::endl;
    fflush(stdout);
    return RCOK;
}

/* Returns the id for the next txn. */
uint64_t CheckThread::get_next_txn_id()
{
    uint64_t txn_id = get_batch_size() * g_next_set;
    return txn_id;
}

void CheckThread::setup()
{
    DEBUG("IN check thread setup, waiting for the lock\n");
    batchMTX.lock();
    commonVar++;
    DEBUG("CheckThread: My thread id is %ld, commonVar %u, and I have already come to setup\n", get_thd_id(), commonVar);
    batchMTX.unlock();  

    // I don't think this is going to happen
	if (_thd_id == 0)
	{
		while (commonVar < g_client_thread_cnt + g_client_rem_thread_cnt + g_client_send_thread_cnt)
			;

		send_init_done_to_all_nodes();
        DEBUG("the check thread unexpectedly has a thd id of 0, it SHOULD NOT be the first thread\n");
	}
} 
