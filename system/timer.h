#include "message.h"
#include <mutex>

#if TIMER_ON

class Timer
{
	uint64_t timestamp;
	string hash;
	Message *msg;

public:
	uint64_t get_timestamp();
	string get_hash();
	Message *get_msg();
	void set_data(uint64_t tst, string hsh, Message *cqry);
};

// Timer for servers
class ServerTimer
{
	// Stores time of arrival for each transaction.
	std::vector<Timer *> txn_map;
	bool timer_state;

public:
	void startTimer(string digest, Message *clqry);
	void endTimer(string digest);
	bool checkTimer();
	void pauseTimer();
	void resumeTimer();
	Timer *fetchPendingRequests(uint64_t idx);
	uint64_t timerSize();
	void removeAllTimers();
};

// peter: timer for DOM servers 
class DOMTimer 
{
	// I want to order the timer and the corresponding txn in increasing order of deadline
	std::map<uint64_t, std::tuple<Timer *, TxnManager *>> txn_map_sorted;
	bool timer_state;
public:
// Here I boldly assume that the deadline is not the same acorss different client batches 
// This is generally true but debatable with multiple clients

	void startTimer(uint64_t deadline, TxnManager *txn);
	void endTimer(uint64_t deadline);
	bool checkTimer();
	void pauseTimer();
	void resumeTimer();
	Timer *fetchPendingRequests(uint64_t idx);
	uint64_t timerSize();
	void removeAllTimers();
};

// Timer for clients.
class ClientTimer
{
	// Stores time of arrival for each transaction.
	std::unordered_map<string, Timer *> txn_map;
	std::map<uint64_t, Timer *> txn_map_sorted;

public:
	void startTimer(uint64_t timestp, ClientQueryBatch *cqry);
	void endTimer(string hash);
	bool checkTimer(ClientQueryBatch *&cbatch);
	Timer *fetchPendingRequests();
	void removeAllTimers();
};

/************************************/

extern ServerTimer *server_timer;
extern ClientTimer *client_timer;
extern std::mutex tlock;

#endif // TIMER_ON
