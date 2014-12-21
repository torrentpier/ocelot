#include "ocelot.h"
#include "db.h"
#include "user.h"
#include "misc_functions.h"
#include <string>
#include <iostream>
#include <queue>
#include <unistd.h>
#include <time.h>
#include <mutex>
#include <thread>

#define DB_LOCK_TIMEOUT 50

mysql::mysql(std::string mysql_db, std::string mysql_host, std::string username, std::string password) :
	db(mysql_db), server(mysql_host), db_user(username), pw(password),
	u_active(false), t_active(false), p_active(false), s_active(false), tok_active(false)
{
	try {
		conn.connect(mysql_db.c_str(), mysql_host.c_str(), username.c_str(), password.c_str(), 0);
	} catch (const mysqlpp::Exception &er) {
		std::cout << "Failed to connect to MySQL (" << er.what() << ')' << std::endl;
		return;
	}

	std::cout << "Connected to MySQL" << std::endl;
	std::cout << "Clearing bb_bt_tracker and resetting peer counts...";
	std::cout.flush();
	clear_peer_data();
	std::cout << "done" << std::endl;
}

bool mysql::connected() {
	return conn.connected();
}

void mysql::clear_peer_data() {
	try {
		mysqlpp::Query query = conn.query("TRUNCATE bb_bt_tracker;");
		if (!query.exec()) {
			std::cerr << "Unable to truncate bb_bt_tracker!" << std::endl;
		}
		query = conn.query("UPDATE bb_bt_tracker_snap SET Seeders = 0, Leechers = 0;");
		if (!query.exec()) {
			std::cerr << "Unable to reset seeder and leecher count!" << std::endl;
		}
	} catch (const mysqlpp::BadQuery &er) {
		std::cerr << "Query error: " << er.what() << " in clear_peer_data" << std::endl;
	} catch (const mysqlpp::Exception &er) {
		std::cerr << "Query error: " << er.what() << " in clear_peer_data" << std::endl;
	}
}

void mysql::load_torrents(torrent_list &torrents) {
	mysqlpp::Query query = conn.query("SELECT topic_id, info_hash, tor_type, complete_count, poster_id FROM bb_bt_torrents ORDER BY topic_id;");
	if (mysqlpp::StoreQueryResult res = query.store()) {
		size_t num_rows = res.num_rows();
		for (size_t i = 0; i < num_rows; i++) {
			std::string info_hash;
			res[i][1].to_string(info_hash);

			torrent t;
			t.id = res[i][0];
			if ((int)res[i][2] == 1) {
				t.free_torrent = FREE;
			} else if ((int)res[i][2] == 2) {
				t.free_torrent = NEUTRAL;
			} else {
				t.free_torrent = NORMAL;
			}
			t.balance = 0;
			t.completed = res[i][3];
			t.poster_id = res[i][4];
			t.last_selected_seeder = "";
			torrents[info_hash] = t;
		}
	}
}

void mysql::load_users(user_list &users) {
	mysqlpp::Query query = conn.query("SELECT user_id, auth_key FROM bb_bt_users;");
	if (mysqlpp::StoreQueryResult res = query.store()) {
		size_t num_rows = res.num_rows();
		for (size_t i = 0; i < num_rows; i++) {
			std::string passkey;
			res[i][1].to_string(passkey);
			//bool protect_ip = res[i][3].compare("1") != 0;
			user_ptr u(new user(res[i][0], true, false));
			users.insert(std::pair<std::string, user_ptr>(passkey, u));
		}
	}
}

void mysql::load_whitelist(std::vector<std::string> &whitelist) {
	mysqlpp::Query query = conn.query("SELECT peer_id FROM xbt_client_whitelist;");
	if (mysqlpp::StoreQueryResult res = query.store()) {
		size_t num_rows = res.num_rows();
		for (size_t i = 0; i<num_rows; i++) {
			whitelist.push_back(res[i][0].c_str());
		}
	}
}

void mysql::record_user(std::string &record) {
	if (update_user_buffer != "") {
		update_user_buffer += ",";
	}
	update_user_buffer += record;
}

void mysql::record_torrent(std::string &record) {
	std::unique_lock<std::mutex> tb_lock(torrent_buffer_lock);
	if (update_torrent_buffer != "") {
		update_torrent_buffer += ",";
	}
	update_torrent_buffer += record;
}

void mysql::record_peer(std::string &record, std::string &ip, std::string &peer_id, std::string &useragent, std::string &hash) {
	if (update_heavy_peer_buffer != "") {
		update_heavy_peer_buffer += ",";
	}
	mysqlpp::Query q = conn.query();
	q << record << mysqlpp::quote << hash << ',' << mysqlpp::quote << ip << ',' << mysqlpp::quote << peer_id << ',' << mysqlpp::quote << useragent << ","  << time(NULL)  << ')';

	update_heavy_peer_buffer += q.str();
}
void mysql::record_peer(std::string &record, std::string &ip, std::string &peer_id, std::string &hash) {
	if (update_light_peer_buffer != "") {
		update_light_peer_buffer += ",";
	}
	mysqlpp::Query q = conn.query();
	q << record << mysqlpp::quote << hash << ','  << mysqlpp::quote << peer_id << ',' << mysqlpp::quote << ip << ',' << time(NULL) << ')';

	update_light_peer_buffer += q.str();
}

void mysql::record_snatch(std::string &record, std::string &ip) {
	if (update_snatch_buffer != "") {
		update_snatch_buffer += ",";
	}
	mysqlpp::Query q = conn.query();
	q << record << ',' << mysqlpp::quote << ip << ')';
	update_snatch_buffer += q.str();
}

bool mysql::all_clear() {
	return (user_queue.size() == 0 && torrent_queue.size() == 0 && peer_queue.size() == 0 && snatch_queue.size() == 0);
}

void mysql::flush() {
	flush_users();
	flush_torrents();
//	flush_snatches();
	flush_peers();
}

void mysql::flush_users() {
	std::string sql;
	std::unique_lock<std::mutex> uq_lock(user_queue_lock);
	size_t qsize = user_queue.size();
	if (verbose_flush || qsize > 0) {
		std::cout << "User flush queue size: " << qsize << std::endl;
	}
	if (update_user_buffer == "") {
		return;
	}
	sql = "INSERT INTO bb_bt_users (user_id, up_today, down_today, up_bonus_today, up_release_today) VALUES " + update_user_buffer +
		" ON DUPLICATE KEY UPDATE up_today = up_today + VALUES(up_today), down_today = down_today + VALUES(down_today), up_bonus_today=VALUES(up_bonus_today), "
               +"up_release_today=VALUES(up_release_today), u_down_total = u_down_total + VALUES(down_today), u_up_total=u_up_total+VALUES(up_today), u_up_bonus = VALUES(up_bonus_today), u_up_release=VALUES(up_release_today)";
	user_queue.push(sql);
	update_user_buffer.clear();
	if (u_active == false) {
		std::thread thread(&mysql::do_flush_users, this);
		thread.detach();
	}
}

void mysql::flush_torrents() {
	std::string sql;
	std::unique_lock<std::mutex> tq_lock(torrent_queue_lock);
	std::unique_lock<std::mutex> tb_lock(torrent_buffer_lock);
	size_t qsize = torrent_queue.size();
	if (verbose_flush || qsize > 0) {
		std::cout << "Torrent flush queue size: " << qsize << std::endl;
	}
	if (update_torrent_buffer == "") {
		return;
	}
	sql = "INSERT INTO bb_bt_tracker_snap (topic_id,seeders,leechers,complete) VALUES " + update_torrent_buffer +
		" ON DUPLICATE KEY UPDATE seeders=VALUES(seeders), leechers=VALUES(leechers), " +
		"complete=complete+VALUES(complete)" ;
	torrent_queue.push(sql);
	update_torrent_buffer.clear();
	sql.clear();
	sql = "DELETE FROM bb_bt_torrents WHERE info_hash = ''";
	torrent_queue.push(sql);
	if (t_active == false) {
		std::thread thread(&mysql::do_flush_torrents, this);
		thread.detach();
	}
}


void mysql::flush_snatches() {
	std::string sql;
	std::unique_lock<std::mutex> sq_lock(snatch_queue_lock);
	size_t qsize = snatch_queue.size();
	if (verbose_flush || qsize > 0) {
		std::cout << "Snatch flush queue size: " << qsize << std::endl;
	}
	if (update_snatch_buffer == "" ) {
		return;
	}
	sql = "INSERT INTO xbt_snatched (uid, fid, tstamp, IP) VALUES " + update_snatch_buffer;
	snatch_queue.push(sql);
	update_snatch_buffer.clear();
	if (s_active == false) {
		std::thread thread(&mysql::do_flush_snatches, this);
		thread.detach();
	}
}

void mysql::flush_peers() {
	std::string sql;
	std::unique_lock<std::mutex> pq_lock(peer_queue_lock);
	size_t qsize = peer_queue.size();
	if (verbose_flush || qsize > 0) {
		std::cout << "Peer flush queue size: " << qsize << std::endl;
	}

	// Nothing to do
	if (update_light_peer_buffer == "" && update_heavy_peer_buffer == "") {
		return;
	}
	if (qsize == 0) {
		sql = "SET session sql_log_bin = 0";
		peer_queue.push(sql);
		sql.clear();
	}
	if (update_heavy_peer_buffer != "") {
		// Because xfu inserts are slow and ram is not infinite we need to
		// limit this queue's size
		// xfu will be messed up if the light query inserts a new row,
		// but that's better than an oom crash
		if (qsize >= 1000) {
			peer_queue.pop();
		}
		sql = "INSERT INTO bb_bt_tracker (user_id,topic_id,uploaded,downloaded,speed_up,speed_down,remain," +
			std::string("seeder,port,peer_hash,ip,peer_id,client,update_time) VALUES ") + update_heavy_peer_buffer +
					" ON DUPLICATE KEY UPDATE uploaded=VALUES(uploaded), up_add=VALUES(uploaded)," +
					"downloaded=VALUES(downloaded), speed_up=VALUES(speed_up), down_add=VALUES(downloaded), " +
					"speed_down=VALUES(speed_down), remain=VALUES(remain), " +
					"update_time=VALUES(update_time), peer_id=VALUES(peer_id), seeder=VALUES(seeder)" ;
		peer_queue.push(sql);
		update_heavy_peer_buffer.clear();
		sql.clear();
	}
	if (update_light_peer_buffer != "") {
		// See comment above
		if (qsize >= 1000) {
			peer_queue.pop();
		}
		sql = "INSERT INTO bb_bt_tracker (topic_id,user_id,port,peer_hash,peer_id,ip,update_time) VALUES " +
					update_light_peer_buffer +
					" ON DUPLICATE KEY UPDATE speed_up=0, speed_down=0";
		peer_queue.push(sql);
		update_light_peer_buffer.clear();
		sql.clear();
	}

	if (p_active == false) {
		std::thread thread(&mysql::do_flush_peers, this);
		thread.detach();
	}
}


void mysql::do_flush_users() {
	u_active = true;
	try {
		mysqlpp::Connection c(db.c_str(), server.c_str(), db_user.c_str(), pw.c_str(), 0);
		while (user_queue.size() > 0) {
			try {
				std::string sql = user_queue.front();
				mysqlpp::Query query = c.query(sql);
				if (!query.exec()) {
					std::cout << "User flush failed (" << user_queue.size() << " remain)" << std::endl;
					sleep(3);
					continue;
				} else {
					std::unique_lock<std::mutex> uq_lock(user_queue_lock);
					user_queue.pop();
				}
			}
			catch (const mysqlpp::BadQuery &er) {
				std::cerr << "Query error: " << er.what() << " in flush users with a qlength: " << user_queue.front().size() << " queue size: " << user_queue.size() << std::endl;
				sleep(3);
				continue;
			} catch (const mysqlpp::Exception &er) {
				std::cerr << "Query error: " << er.what() << " in flush users with a qlength: " << user_queue.front().size() <<  " queue size: " << user_queue.size() << std::endl;
				sleep(3);
				continue;
			}
		}
	}
	catch (const mysqlpp::Exception &er) {
		std::cerr << "MySQL error in flush_users: " << er.what() << std::endl;
		u_active = false;
		return;
	}
	u_active = false;
}

void mysql::do_flush_torrents() {
	t_active = true;
	try {
		mysqlpp::Connection c(db.c_str(), server.c_str(), db_user.c_str(), pw.c_str(), 0);
		while (torrent_queue.size() > 0) {
			try {
				std::string sql = torrent_queue.front();
				if (sql == "") {
					torrent_queue.pop();
					continue;
				}
				mysqlpp::Query query = c.query(sql);
				if (!query.exec()) {
					std::cout << "Torrent flush failed (" << torrent_queue.size() << " remain)" << std::endl;
					sleep(3);
					continue;
				} else {
					std::unique_lock<std::mutex> tq_lock(torrent_queue_lock);
					torrent_queue.pop();
				}
			}
			catch (const mysqlpp::BadQuery &er) {
				std::cerr << "Query error: " << er.what() << " in flush torrents with a qlength: " << torrent_queue.front().size() << " queue size: " << torrent_queue.size() << std::endl;
				std::cerr << torrent_queue.front() << std::endl;
				sleep(3);
				continue;
			} catch (const mysqlpp::Exception &er) {
				std::cerr << "Query error: " << er.what() << " in flush torrents with a qlength: " << torrent_queue.front().size() << " queue size: " << torrent_queue.size() << std::endl;
				std::cerr << torrent_queue.front() << std::endl;
				sleep(3);
				continue;
			}
		}
	}
	catch (const mysqlpp::Exception &er) {
		std::cerr << "MySQL error in flush_torrents: " << er.what() << std::endl;
		t_active = false;
		return;
	}
	t_active = false;
}

void mysql::do_flush_peers() {
	p_active = true;
	try {
		mysqlpp::Connection c(db.c_str(), server.c_str(), db_user.c_str(), pw.c_str(), 0);
		while (peer_queue.size() > 0) {
			try {
				std::string sql = peer_queue.front();
				mysqlpp::Query query = c.query(sql);
				if (!query.exec()) {
					std::cout << "Peer flush failed (" << peer_queue.size() << " remain)" << std::endl;
					sleep(3);
					continue;
				} else {
					std::unique_lock<std::mutex> pq_lock(peer_queue_lock);
					peer_queue.pop();
				}
			}
			catch (const mysqlpp::BadQuery &er) {
				std::cerr << "Query error: " << er.what() << " in flush peers with a qlength: " << peer_queue.front().size() << " queue size: " << peer_queue.size() << std::endl;
				sleep(3);
				continue;
			} catch (const mysqlpp::Exception &er) {
				std::cerr << "Query error: " << er.what() << " in flush peers with a qlength: " << peer_queue.front().size() << " queue size: " << peer_queue.size() << std::endl;
				sleep(3);
				continue;
			}
		}
	}
	catch (const mysqlpp::Exception &er) {
		std::cerr << "MySQL error in flush_peers: " << er.what() << std::endl;
		p_active = false;
		return;
	}
	p_active = false;
}

void mysql::do_flush_snatches() {
	s_active = true;
	try {
		mysqlpp::Connection c(db.c_str(), server.c_str(), db_user.c_str(), pw.c_str(), 0);
		while (snatch_queue.size() > 0) {
			try {
				std::string sql = snatch_queue.front();
				mysqlpp::Query query = c.query(sql);
				if (!query.exec()) {
					std::cout << "Snatch flush failed (" << snatch_queue.size() << " remain)" << std::endl;
					sleep(3);
					continue;
				} else {
					std::unique_lock<std::mutex> sq_lock(snatch_queue_lock);
					snatch_queue.pop();
				}
			}
			catch (const mysqlpp::BadQuery &er) {
				std::cerr << "Query error: " << er.what() << " in flush snatches with a qlength: " << snatch_queue.front().size() << " queue size: " << snatch_queue.size() << std::endl;
				sleep(3);
				continue;
			} catch (const mysqlpp::Exception &er) {
				std::cerr << "Query error: " << er.what() << " in flush snatches with a qlength: " << snatch_queue.front().size() << " queue size: " << snatch_queue.size() << std::endl;
				sleep(3);
				continue;
			}
		}
	}
	catch (const mysqlpp::Exception &er) {
		std::cerr << "MySQL error in flush_snatches: " << er.what() << std::endl;
		s_active = false;
		return;
	}
	s_active = false;
}

