#include <bits/ints.h>

#define NLINKS 8
#define NSCANS 64
#define NCHILDREN 10
#define NTASKS 4
#define NCONNS 8
#define NADDRS 16

#define NAMELEN 16
#define SSIDLEN 32
#define PSKLEN 32

/* Netdev state tracking, and some low key per-device configuration.
   One of these is kept for each non-loopback kernel device. */

/* link.flags */
#define S_NL80211  (1<<0)
#define S_ENABLED  (1<<1)
#define S_CARRIER  (1<<2)
#define S_IPADDR   (1<<3)
#define S_UPLINK   (1<<4)
#define S_CHILDREN (1<<5)
#define S_SIGSENT  (1<<6)
#define S_DELSENT  (1<<7)
#define S_PROBE    (1<<8)

/* link.scan */
#define SC_NONE        0
#define SC_REQUEST     1
#define SC_ONGOING     2
#define SC_RESULTS     3
#define SC_DUMPING     4

/* link.mode */
#define LM_FREE        0   /* no idea what this device is, leave it be */
#define LM_NOT         1   /* do not touch this device                 */
#define LM_OFF         2   /* keep this device disabled                */
#define LM_DHCP        3   /* run dhcp on this device when appropriate */
#define LM_LOCAL       4   /* run dhcp in local mode on this device    */
#define LM_STATIC      5   /* set pre-defined ip for this device       */

/* link.state */
#define LS_DOWN        0
#define LS_STARTING    1
#define LS_ACTIVE      2
#define LS_STOPPING    3

struct link {
	int ifi;
	int seq;
	char name[NAMELEN+2];
	short flags;
	short mode;
	short state;
	short rfk;
};

/* Persistent scan list entry. Wimon caches short-lived scan results
   for later usage. */

/* scan.type */
#define ST_WPS         (1<<0)
#define ST_WPA         (1<<1)
#define ST_RSN         (1<<3)
#define ST_RSN_PSK     (1<<4)
#define ST_RSN_P_TKIP  (1<<5) /* pairwise */
#define ST_RSN_P_CCMP  (1<<6)
#define ST_RSN_G_TKIP  (1<<7) /* group */
#define ST_RSN_G_CCMP  (1<<8)

#define SF_SEEN        (1<<0)
#define SF_GOOD        (1<<1)
#define SF_STALE       (1<<2)

struct scan {
	short freq;
	short signal;
	short type;
	short prio;
	short tries;
	short flags;
	uint8_t bssid[6];
	short slen;
	uint8_t ssid[SSIDLEN];
};

/* Child process tracking. Each child is bound to a specific link. */

struct child {
	int ifi;
	int pid;
};

/* Wireless config automation. Singular and generally *not* bound
   to a specific device. */

/* wifi.mode */
#define WM_DISABLED    0
#define WM_ROAMING     1
#define WM_FIXEDAP     2
/* wifi.state */
#define WS_NONE        0
#define WS_IDLE        1
#define WS_SCANNING    2
#define WS_RETRYING    3
#define WS_STARTING    4
#define WS_CHANGING    5
#define WS_CONNECTED   6
/* wifi.flags */
#define WF_UNSAVED    (1<<0)
#define WF_NEWPSK     (1<<1)

struct wifi {
	int ifi;
	short mode;
	short state;
	short flags;
	/* current AP */
	short freq;
	short prio;
	short slen;
	short type;
	uint8_t ssid[SSIDLEN];
	uint8_t bssid[6];
	char psk[2*32+1];
};

/* Tracked ip addresses */

#define ADDR_IFACE  1
#define ADDR_UPLINK 2
#define ADDR_STATIC 3

struct addr {
	int ifi;
	uint8_t ip[4];
	uint8_t mask;
	uint8_t type;
};

/* client (wictl) connections */

struct conn {
	int fd;
	int evt;
	int ifi;
	int hup;
};

/* config file parts */

struct line {
	char* start;
	char* end;
};

struct chunk {
	char* start;
	char* end;
};

extern struct link links[];
extern struct scan scans[];
extern struct conn conns[];
extern int nlinks;
extern int nscans;
extern int nconns;
extern struct gate gateway;
extern struct child children[];
extern int nchildren;

extern struct uplink uplink;
extern struct wifi wifi;

struct netlink;
struct nlmsg;
struct nlgen;

extern char** environ;
extern int envcount;

/* wimon_rtnl.c and wimon_genl.c */

extern struct netlink rtnl;
extern struct netlink genl;
extern int nl80211;
extern int ctrlfd;
extern int sigchld;
extern int rfkillfd;

/* wimon.c */

void schedule(int secs, void (*call)(int), int ifi);
void cancel_scheduled(int ifi);
void update_killfd(void);

/* wimon_rtnl.c and wimon_genl.c */

void setup_rtnl(void);
void setup_genl(void);
void setup_ctrl(void);
void accept_ctrl(int sfd);
void unlink_ctrl(void);
void handle_rtnl(struct nlmsg* msg);
void handle_genl(struct nlmsg* msg);
void add_link_address(int ifi, uint8_t* ip, int mask);
void del_link_addresses(int ifi);

void trigger_scan(int ifi, int freq);
void trigger_disconnect(int ifi);
void parse_station_ies(struct scan* sc, char* buf, int len);

void enable_iface(int ifi);
void disable_iface(int ifi);

/* wimon_kill.c */

void retry_rfkill(void);
void reset_rfkill(void);
void handle_rfkill(void);

/* wimon_slot.c */

struct link* find_link_slot(int ifi);
struct link* grab_link_slot(int ifi);
void free_link_slot(struct link* ls);

struct scan* grab_scan_slot(uint8_t* bssid);
void drop_scan_slots(void);
void free_scan_slot(struct scan* sc);

struct child* grab_child_slot(void);
struct child* find_child_slot(int pid);
void free_child_slot(struct child* ch);

struct conn* grab_conn_slot(void);
struct conn* find_conn_slot(int fd);
void free_conn_slot(struct conn* cn);

void add_addr(int ifi, int type, uint8_t* ip, int mask);
void del_addr(int ifi, int type, uint8_t* ip, int mask);
struct addr* get_addr(int ifi, int type, struct addr* prev);
void del_all_addrs(int ifi, int type);

/* wimon_link.c */

void link_new(struct link* ls);
void link_gone(struct link* ls);
void link_down(struct link* ls);
void link_wifi(struct link* ls);
void link_nl80211(struct link* ls);
void link_enabled(struct link* ls);
void link_carrier(struct link* ls);
void link_ipaddr(struct link* ls);
void link_ipgone(struct link* ls);
void link_child_exit(struct link* ls, int status);
void link_rfback(struct link* ls);
void recheck_alldown_latches(void);

int any_pids_left(void);
void terminate_link(struct link* ls);
void finalize_links(void);
int stop_links_except(int ifi);
int switch_uplink(int ifi);
void stop_link(struct link* ls);
int start_wired_link(struct link* ls);
void set_link_mode(struct link* ls, int mode);
void mark_starting(struct link* ls, int secs);

/* wimon_wifi.c */

void wifi_ready(struct link* ls);
void wifi_gone(struct link* ls);
void wifi_connected(struct link* ls);
void wifi_conn_fail(struct link* ls);

void wifi_scan_done(void);
void wifi_scan_fail(int err);

int grab_wifi_device(int rifi);
void start_wifi_scan(void);

void wifi_mode_disabled(void);
int wifi_mode_roaming(void);
int wifi_mode_fixedap(uint8_t* ssid, int slen, char* psk);

/* wimon_proc.c */

void spawn_dhcp(struct link* ls, char* opts);
void spawn_wpa(struct link* ls, char* mode);
void stop_link_procs(struct link* ls, int drop);
void stop_all_procs(void);
void waitpids(void);

/* wimon_conf_file.c */

int load_config(void);
void save_config(void);

int chunklen(struct chunk* ck);
int chunkis(struct chunk* ck, const char* str);
int find_line(struct line* ln, char* pref, int i, char* val);
int split_line(struct line* ln, struct chunk* ck, int nc);
void save_line(struct line* ls, char* buf, int len);
void drop_line(struct line* ln);
void change_chunk(struct chunk* ck, char* str);

/* wimon_conf_data.c */

void load_link(struct link* ls);
void save_link(struct link* ls);

int saved_psk_prio(uint8_t* ssid, int slen);

int load_psk(uint8_t* ssid, int slen, char* buf, int len);
void save_psk(uint8_t* ssid, int slen, char* psk);
int drop_psk(uint8_t* ssid, int slen);
int set_psk_prio(uint8_t* ssid, int slen, int prio);

void load_wifi(struct link* ls);
void save_wifi(void);

/* wimon_ctrl.c, wimon_ctrl_rep.c */
void handle_conn(struct conn* cn);
void rep_status(struct conn* cn);
void rep_linkconf(struct conn* cn);
void rep_scanlist(struct conn* cn);
void reply(struct conn* cn, int err);

/* latch.ifi */
#define NONE 0
#define WIFI -1
/* latch.evt */
#define ANY  0
#define CONF 1
#define DOWN 2
#define SCAN 3

void unlatch(int ifi, int evt, int err);
