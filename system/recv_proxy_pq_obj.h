#include <queue>
#include <vector>
#include <mutex>
#include "ycsb_query.h"

class DeadlinePQObj
{
public:
    DeadlinePQObj(uint64_t deadline, std::vector<YCSBClientQueryMessage *>& cqrySet, uint64_t client_node_id): 
    deadline(deadline), cqrySet(cqrySet), client_node_id(client_node_id) {}

    DeadlinePQObj() : deadline(0), cqrySet(), client_node_id(0) {}

    uint64_t get_deadline() const { return deadline; }
    const std::vector<YCSBClientQueryMessage *> get_cqrySet() const { return cqrySet; }
    uint64_t get_client_node_id() const { return client_node_id; }

private:
    uint64_t deadline;
    std::vector<YCSBClientQueryMessage *> cqrySet;
    uint64_t client_node_id;

};

class CompareDeadlinePQObj
{
public:
    bool operator()(const DeadlinePQObj &lhs, const DeadlinePQObj &rhs) const
    {
        return lhs.get_deadline() > rhs.get_deadline();
    }
};

class ThreadSafePriorityQueue {
public:
    void push(const DeadlinePQObj& data) {
        std::lock_guard<std::mutex> lock(mtx);  // Lock the mutex to ensure thread safety
        pq.push(data);  // Add the new data to the priority queue
    }

    bool tryPop(DeadlinePQObj& data) {
        std::lock_guard<std::mutex> lock(mtx);  // Lock the mutex to ensure thread safety
        if (pq.empty()) {
            return false;  // If the priority queue is empty, return false
        }
        data = pq.top();  // Get the top element
        pq.pop();  // Remove the top element from the priority queue
        return true;  // Indicate that an element was successfully popped
    }

    bool tryPeek(DeadlinePQObj& data) {
        std::lock_guard<std::mutex> lock(mtx);  // Lock the mutex to ensure thread safety
        if (pq.empty()) {
            return false;  // If the priority queue is empty, return false
        }
        data = pq.top();  // Get the top element
        return true;  // Indicate that an element was successfully peeked at
    }

    bool empty() const {
        std::lock_guard<std::mutex> lock(mtx);  // Lock the mutex to ensure thread safety
        return pq.empty();  // Return whether the priority queue is empty
    }

    size_t size() const {
        std::lock_guard<std::mutex> lock(mtx);  // Lock the mutex to ensure thread safety
        return pq.size();  // Return the size of the priority queue
    }

private:
    std::priority_queue<DeadlinePQObj, std::vector<DeadlinePQObj>, CompareDeadlinePQObj> pq;  // The underlying priority queue
    mutable std::mutex mtx;  // Mutex to protect access to the priority queue
};