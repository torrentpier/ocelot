#include <string>
#include <map>
#include <vector>
#include <unordered_map>
#include <set>
#include <memory>
#include <mutex>

#ifndef OCELOT_H
#define OCELOT_H

class user;
typedef std::shared_ptr<user> user_ptr;

typedef struct {
	unsigned int port;
	int64_t uploaded;
	int64_t downloaded;
	int64_t corrupt;
	int64_t left;
	time_t last_announced;
	time_t first_announced;
	unsigned int announces;
	bool visible;
	bool invalid_ip;
	user_ptr user;
	std::string ip_port;
	std::string ip;
} peer;

typedef std::map<std::string, peer> peer_list;

enum freetype { NORMAL, FREE, NEUTRAL };

typedef struct {
	int id;
	int64_t balance;
	int completed;
	freetype free_torrent;
	time_t last_flushed;
	peer_list seeders;
	peer_list leechers;
	std::string last_selected_seeder;
} torrent;

enum {
	DUPE, // 0
	TRUMP // 1
};

typedef struct {
	int reason;
	time_t time;
} del_message;

typedef std::unordered_map<std::string, torrent> torrent_list;
typedef std::unordered_map<std::string, user_ptr> user_list;
typedef std::unordered_map<std::string, std::string> params_type;

struct stats {
	std::mutex mutex;
	unsigned int open_connections;
	uint64_t opened_connections;
	uint64_t connection_rate;
	unsigned int leechers;
	unsigned int seeders;
	uint64_t announcements;
	uint64_t succ_announcements;
	uint64_t scrapes;
	uint64_t bytes_read;
	uint64_t bytes_written;
	time_t start_time;
};
extern struct stats stats;
#endif
