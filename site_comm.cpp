#include <iostream>
#include <istream>
#include <ostream>
#include <string>
#include <sstream>
#include <queue>
#include <mutex>
#include <boost/asio.hpp>
#include <thread>

#include "config.h"
#include "site_comm.h"

using boost::asio::ip::tcp;

site_comm::site_comm(config * conf) : t_active(false) {
	load_config(conf);
}

void site_comm::load_config(config * conf) {
	readonly = conf->get_bool("readonly");
}

void site_comm::reload_config(config * conf) {
	load_config(conf);
}

bool site_comm::all_clear() {
	return (token_queue.size() == 0);
}

void site_comm::expire_token(int torrent, int user) {
	std::stringstream token_pair;
	token_pair << user << ':' << torrent;
	if (expire_token_buffer != "") {
		expire_token_buffer += ",";
	}
	expire_token_buffer += token_pair.str();
	if (expire_token_buffer.length() > 350) {
		std::cout << "Flushing overloaded token buffer" << std::endl;
		if (!readonly) {
			std::lock_guard<std::mutex> lock(expire_queue_lock);
			token_queue.push(expire_token_buffer);
		}
		expire_token_buffer.clear();
	}
}

void site_comm::flush_tokens()
{
	if (readonly) {
		expire_token_buffer.clear();
		return;
	}
	std::lock_guard<std::mutex> lock(expire_queue_lock);
	size_t qsize = token_queue.size();
	if (verbose_flush || qsize > 0) {
		std::cout << "Token expire queue size: " << qsize << std::endl;
	}
	if (expire_token_buffer == "") {
		return;
	}
	token_queue.push(expire_token_buffer);
	expire_token_buffer.clear();
	if (!t_active) {
		std::thread thread(&site_comm::do_flush_tokens, this);
		thread.detach();
	}
}

void site_comm::do_flush_tokens()
{
	t_active = false;
}

site_comm::~site_comm()
{
}
