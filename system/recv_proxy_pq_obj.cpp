#include "recv_proxy_pq_obj.h"
#include "message.h"

DeadlinePQObj::DeadlinePQObj(uint64_t deadline, std::vector<YCSBClientQueryMessage *>& cqrySet, uint64_t client_node_id)
    : deadline(deadline), client_node_id(client_node_id)
{
    for (uint64_t i = 0; i < cqrySet.size(); i++)
    {
        char *buf = (char *)malloc(cqrySet[i]->get_size());
        cqrySet[i]->copy_to_buf(buf);
        Message *tmsg = Message::create_message(buf);
        YCSBClientQueryMessage *qry = (YCSBClientQueryMessage *)tmsg;

        free(buf);
        this->cqrySet.push_back(qry);
    }
}