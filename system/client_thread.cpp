#include "global.h"
#include "thread.h"
#include "client_thread.h"
#include "query.h"
#include "ycsb_query.h"
#include "client_query.h"
#include "transport.h"
#include "client_txn.h"
#include "msg_thread.h"
#include "msg_queue.h"
#include "wl.h"
#include "message.h"
#include "timer.h"

uint64_t ClientThread::get_next_txn_id()
{
	uint64_t txn_id;
	client_next_txn_id_mtx.lock();
	txn_id = client_next_txn_id;
	client_next_txn_id++;
	client_next_txn_id_mtx.unlock();
	return txn_id;
}

uint64_t ClientThread::get_estimated_exec_time_interval()
{
	// peter: currently just add 10 seconds to the current time.
	return 10 * 1000000000ULL;
}

void ClientThread::send_key()
{
	// Send everyone the public key.
	for (uint64_t i = 0; i < g_node_cnt + g_client_node_cnt + g_recv_proxy_cnt + g_send_proxy_cnt; i++)
	{
		if (i == g_node_id)
		{
			continue;
		}

#if CRYPTO_METHOD_RSA || CRYPTO_METHOD_ED25519
		Message *msg = Message::create_message(KEYEX);
		KeyExchange *keyex = (KeyExchange *)msg;
		// The four first letters of the message set the type
#if CRYPTO_METHOD_RSA
		cout << "Sending the key RSA: " << g_public_key.size() << endl;
		keyex->pkey = "RSA-" + g_public_key;
#elif CRYPTO_METHOD_ED25519
		cout << "Sending the key ED25519: " << g_public_key.size() << endl;
		keyex->pkey = "ED2-" + g_public_key;
#endif

		keyex->pkeySz = keyex->pkey.size();
		keyex->return_node = g_node_id;

		vector<uint64_t> dest;
		dest.push_back(i);

		msg_queue.enqueue(get_thd_id(), keyex, dest);
#endif

#if CRYPTO_METHOD_CMAC_AES
		cout << "Sending the key CMAC: " << cmacPrivateKeys[i].size() << endl;
		Message *msgCMAC = Message::create_message(KEYEX);
		KeyExchange *keyexCMAC = (KeyExchange *)msgCMAC;
		keyexCMAC->pkey = "CMA-" + cmacPrivateKeys[i];

		keyexCMAC->pkeySz = keyexCMAC->pkey.size();
		keyexCMAC->return_node = g_node_id;
		//msg_queue.enqueue(get_thd_id(), keyexCMAC, i);
		msg_queue.enqueue(get_thd_id(), keyexCMAC, dest);
		dest.clear();
#endif
	}
}

void ClientThread::setup()
{

	// Increment commonVar.
	batchMTX.lock();
	commonVar++;
	batchMTX.unlock();

	if (_thd_id == 0)
	{
		while (commonVar < g_client_thread_cnt + g_client_rem_thread_cnt + g_client_send_thread_cnt)
			;

		send_init_done_to_all_nodes();
		send_key();
	}
}

// peter: calculate which proxy the client should send the request to
uint64_t ClientThread::calculate_send_proxy() 
{
	return (g_node_id - g_node_cnt + 1 + g_node_id);
	// return (g_node_cnt + g_client_node_cnt + 1 );
}

RC ClientThread::run()
{
	printf("Running ClientThread %ld\n", _thd_id);
	
	tsetup();
	printf("Done setting up ClientThread %ld\n", _thd_id);

	while (true)
	{
		keyMTX.lock();
		if (keyAvail)
		{
			keyMTX.unlock();
			break;
		}
		keyMTX.unlock();
	}
#if !BANKING_SMART_CONTRACT
	BaseQuery *m_query;
#endif
	uint64_t iters = 0;
	uint32_t num_txns_sent = 0;
	int txns_sent[g_node_cnt];
	for (uint32_t i = 0; i < g_node_cnt; ++i)
		txns_sent[i] = 0;

	run_starttime = get_sys_clock();

#if CLIENT_BATCH
	uint addMore = 0;

	// Initializing first batch
	Message *mssg = Message::create_message(CL_BATCH);
	ClientQueryBatch *bmsg = (ClientQueryBatch *)mssg;
	bmsg->init();
#endif

	uint32_t next_node_id = get_client_view();
	while (!simulation->is_done())
	{
		heartbeat();
		progress_stats();
		int32_t inf_cnt;
		uint32_t next_node = get_client_view();

#if GBFT
		next_node_id = view_to_primary(get_client_view(get_cluster_number()));
#else
		next_node_id = get_client_view();
#endif

#if VIEW_CHANGES
		//if a request by this client hasnt been completed in time
		ClientQueryBatch *cbatch = NULL;
		if (client_timer->checkTimer(cbatch))
		{
			cout << "TIMEOUT!!!!!!\n";

			//TODO for experimental purpose: force one view change
			if (get_client_view() == 0)
				resend_msg(cbatch);
		}
#endif

#if LOCAL_FAULT
		//if a request by this client hasnt been completed in time
		ClientQueryBatch *cbatch = NULL;
		if (client_timer->checkTimer(cbatch))
		{
			cout << "TIMEOUT!!!!!!\n";
			for (uint64_t i = 0; i < get_batch_size(); i++)
			{
				client_man.dec_inflight(get_client_view());
			}
		}
#endif

		// Just in case...
		if (iters == UINT64_MAX)
			iters = 0;

#if !CLIENT_BATCH // If client batching disable
		if ((inf_cnt = client_man.inc_inflight(next_node)) < 0)
			continue;

		m_query = client_query_queue.get_next_query(next_node, _thd_id);
		if (last_send_time > 0)
		{
			INC_STATS(get_thd_id(), cl_send_intv, get_sys_clock() - last_send_time);
		}
		last_send_time = get_sys_clock();
		assert(m_query);

		DEBUG("Client: thread %lu sending query to node: %u, %d, %f\n",
			  _thd_id, next_node_id, inf_cnt, simulation->seconds_from_start(get_sys_clock()));

		Message *msg = Message::create_message((BaseQuery *)m_query, CL_QRY);
		((ClientQueryMessage *)msg)->client_startts = get_sys_clock();

		YCSBClientQueryMessage *clqry = (YCSBClientQueryMessage *)msg;
		clqry->return_node = g_node_id;

		msg_queue.enqueue(get_thd_id(), msg, next_node_id);
		num_txns_sent++;
		txns_sent[next_node]++;
		INC_STATS(get_thd_id(), txn_sent_cnt, 1);

#else // If client batching enable

		// peter: number of inflight txn >= max, do not send anymore, wait 
		if ((inf_cnt = client_man.inc_inflight(next_node)) < 0)
		{
			continue;
		}
#if BANKING_SMART_CONTRACT
		uint64_t source = (uint64_t)rand() % 10000;
		uint64_t dest = (uint64_t)rand() % 10000;
		uint64_t amount = (uint64_t)rand() % 10000;
		BankingSmartContractMessage *clqry = new BankingSmartContractMessage();
		clqry->rtype = BSC_MSG;
		clqry->inputs.init(!(addMore % 3) ? 3 : 2);
		clqry->type = (BSCType)(addMore % 3);
		clqry->inputs.add(amount);
		clqry->inputs.add(source);
		((ClientQueryMessage *)clqry)->client_startts = get_sys_clock();
		if (addMore % 3 == 0)
			clqry->inputs.add(dest);
		clqry->return_node_id = g_node_id;
#else
		m_query = client_query_queue.get_next_query(_thd_id);
		if (last_send_time > 0)
		{
			INC_STATS(get_thd_id(), cl_send_intv, get_sys_clock() - last_send_time);
		}
		last_send_time = get_sys_clock();
		assert(m_query);

		Message *msg = Message::create_message((BaseQuery *)m_query, CL_QRY);
		((ClientQueryMessage *)msg)->client_startts = get_sys_clock();

		YCSBClientQueryMessage *clqry = (YCSBClientQueryMessage *)msg;
		clqry->return_node = g_node_id;
		uint64_t c_txn_id = get_next_txn_id();
		clqry->client_txn_id = c_txn_id;
		DEBUG("Client txn id: %lu\n", c_txn_id);

		// maybe not ideal to init at this loc
		// init at send time is ideal
		// peter: also init the dom time field in the message
		// clqry->client_dom_time = get_sys_clock() + get_estimated_exec_time_interval();

#endif

		bmsg->cqrySet.add(clqry);
		addMore++;

		// Resetting and sending the message
		if (addMore == g_batch_size)
		{
			// peter: okay here since we are using ed25519, the next_node_id does not really matter. 
			// when changing to cmac, we have to be careful to change it to the destintaion id
			// o/w the validation process will fail.
			// bmsg->sign(next_node_id); // Sign the message.
			string batchStr = "";
			for (uint64_t i = 0; i < get_batch_size(); i++)
			{
				// peter: init the dom time field in the message
				bmsg->cqrySet[i]->client_dom_time = get_sys_clock() + get_estimated_exec_time_interval();
				batchStr += bmsg->cqrySet[i]->getString();
			}
			string hash = calculateHash(batchStr);
			// cout << "client query hash:" << "   " << hexStr(hash.c_str(), hash.length()) << endl;
			client_responses_count.add(hash, 0);

#if TIMER_ON
			char *buf = create_msg_buffer(bmsg);
			Message *deepCMsg = deep_copy_msg(buf, bmsg);
			ClientQueryBatch *deepCqry = (ClientQueryBatch *)deepCMsg;
			uint64_t c_txn_id = get_sys_clock();
			deepCqry->txn_id = c_txn_id;
			client_timer->startTimer(deepCqry->cqrySet[get_batch_size() - 1]->client_startts, deepCqry);
			delete_msg_buffer(buf);
#endif // TIMER_ON

			// peter: if DOM, we need the client to send it to every node.
			// O/W just send it to the primary
#if DOM
			DEBUG ("Sending to the send Porxy %llu, and the primary is %u\n", (unsigned long long int)calculate_send_proxy(), next_node_id);
			// for (uint64_t i = 0; i < g_node_cnt; i++)
			// {
			// 	char *buf = create_msg_buffer(bmsg);
			// 	Message *deepCMsg = deep_copy_msg(buf, bmsg);
			// 	ClientQueryBatch *deepCqry = (ClientQueryBatch *)deepCMsg;
			// 	// deepCqry->txn_id = 
			// 	deepCqry->sign(i);
			// 	delete_msg_buffer(buf);

			// 	vector<uint64_t> dest;
			// 	dest.push_back(i);
			// 	msg_queue.enqueue(get_thd_id(), deepCMsg, dest);
			// 	dest.clear();
			// }

			// peter: change of plan, I only send to the corresponding send proxy, and the send proxy will relay to the receiving proxy
			{
			uint64_t corresponding_send_proxy_id = calculate_send_proxy();
			char *buf = create_msg_buffer(bmsg);
			Message *deepCMsg = deep_copy_msg(buf, bmsg);
			ClientQueryBatch *deepCqry = (ClientQueryBatch *)deepCMsg;
			// deepCqry->txn_id = 
			deepCqry->sign(corresponding_send_proxy_id);
			delete_msg_buffer(buf);

			vector<uint64_t> dest;
			dest.push_back(corresponding_send_proxy_id);
			msg_queue.enqueue(get_thd_id(), deepCMsg, dest);
			dest.clear();
			}

#else
			msg_queue.enqueue(get_thd_id(), bmsg, {next_node_id});
#endif //DOM
			num_txns_sent += g_batch_size;
			txns_sent[next_node] += g_batch_size;
			INC_STATS(get_thd_id(), txn_sent_cnt, g_batch_size);

			mssg = Message::create_message(CL_BATCH);
			bmsg = (ClientQueryBatch *)mssg;
			bmsg->init();
			addMore = 0;
		}

#endif // Batch Enable
	}

	printf("FINISH %ld:%ld\n", _node_id, _thd_id);
	fflush(stdout);
	return FINISH;
}

#if VIEW_CHANGES
#if GBFT
// Resend message to all the servers.
void ClientThread::resend_msg(ClientQueryBatch *symsg)
{
	//cout << "Resend: " << symsg->cqrySet[get_batch_size()-1]->client_startts << "\n";
	//fflush(stdout);

	char *buf = create_msg_buffer(symsg);
	uint64_t first_node_in_cluster = get_cluster_number(g_node_id) * gbft_cluster_size;
	for (uint64_t j = first_node_in_cluster; j < first_node_in_cluster + gbft_cluster_size; j++)
	{
		vector<uint64_t> dest;
		dest.push_back(j);

		Message *deepCMsg = deep_copy_msg(buf, symsg);
		msg_queue.enqueue(get_thd_id(), deepCMsg, dest);
		dest.clear();
	}
	delete_msg_buffer(buf);
	Message::release_message(symsg);
}
#else
// Resend message to all the servers.
void ClientThread::resend_msg(ClientQueryBatch *symsg)
{
	//cout << "Resend: " << symsg->cqrySet[get_batch_size()-1]->client_startts << "\n";
	//fflush(stdout);

	char *buf = create_msg_buffer(symsg);
	for (uint64_t j = 0; j < g_node_cnt; j++)
	{
		vector<uint64_t> dest;
		dest.push_back(j);

		Message *deepCMsg = deep_copy_msg(buf, symsg);
		msg_queue.enqueue(get_thd_id(), deepCMsg, dest);
		dest.clear();
	}
	delete_msg_buffer(buf);
	Message::release_message(symsg);
}
#endif
#endif // VIEW_CHANGES