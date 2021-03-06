#include <sys/file.h>
#include <sys/stat.h>
#include <sys/mmap.h>

#include <format.h>
#include <string.h>
#include <fail.h>
#include <util.h>

#include "config.h"
#include "wimon.h"

static const struct kwd {
	char key[12];
	int val;
} linkmodes[] = {
	{ "off",    LM_OFF    },
	{ "not",    LM_NOT    },
	{ "dhcp",   LM_DHCP   },
	{ "local",  LM_LOCAL  },
	{ "static", LM_STATIC },
	{ "",       0         }
}, wifimodes[] = {
	{ "off",     WM_DISABLED },
	{ "roaming", WM_ROAMING  },
	{ "fixedap", WM_FIXEDAP  },
	{ "",        0           }
};

static int lookup(const struct kwd* dict, struct chunk* ck)
{
	const struct kwd* kw;

	for(kw = dict; kw->key[0]; kw++)
		if(chunkis(ck, kw->key))
			return kw->val;

	return 0;
}

static const char* nameof(const struct kwd* dict, int val)
{
	const struct kwd* kw;

	for(kw = dict; kw->key[0]; kw++)
		if(kw->val == val)
			return kw->key;

	return NULL;
}

static void parse_link_extra(struct link* ls, struct chunk* ck, int cn)
{
	int i;
	char* p;
	uint8_t ip[5];

	for(i = 0; i < cn; i++) {
		/* parseip wants a string not a chunk */
		int len = ck->end - ck->start;
		char str[len+2];
		memcpy(str, ck->start, len);
		str[len] = '\0';

		if((p = parseipmask(str, ip, ip+4)) && !*p)
			add_addr(ls->ifi, ADDR_STATIC, ip, ip[4]);
		else
			warn("unknows link keyword", str, 0);
	}
}

void load_link(struct link* ls)
{
	struct line ln;
	int cn = 10;
	struct chunk ck[cn];

	if(load_config())
		return;
	if(find_line(&ln, "link", 1, ls->name))
		return;
	if((cn = split_line(&ln, ck, cn)) < 3)
		return;

	ls->mode = lookup(linkmodes, &ck[2]);

	parse_link_extra(ls, &ck[3], cn - 3);
}

static char* fmt_link_mode(char* p, char* e, struct link* ls)
{
	const char* mode;

	if((mode = nameof(linkmodes, ls->mode)))
		p = fmtstr(p, e, mode);
	else
		p = fmtint(p, e, ls->mode);

	return p;
}

static char* put_link_addrs(char* p, char* e, struct link* ls)
{
	struct addr* ad = NULL;
	int ifi = ls->ifi;

	while((ad = get_addr(ifi, ADDR_STATIC, ad))) {
		p = fmtstr(p, e, " ");
		p = fmtip(p, e, ad->ip);
		p = fmtstr(p, e, "/");
		p = fmtint(p, e, ad->mask);
	}

	return p;
}

void save_link(struct link* ls)
{
	struct line ln;

	char buf[150];
	char* p = buf;
	char* e = buf + sizeof(buf) - 1;

	if(load_config())
		return;

	find_line(&ln, "link", 1, ls->name);

	if(ls->mode == LM_FREE) {
		drop_line(&ln);
		return;
	}

	p = fmtstr(p, e, "link");
	p = fmtstr(p, e, " ");
	p = fmtstr(p, e, ls->name);
	p = fmtstr(p, e, " ");
	p = fmt_link_mode(p, e, ls);

	if(ls->mode == LM_STATIC)
		p = put_link_addrs(p, e, ls);

	save_line(&ln, buf, p - buf);
}

static void prep_ssid(char* buf, int len, uint8_t* ssid, int slen)
{
	char* p = buf;
	char* e = buf + len - 1;
	int i;

	for(i = 0; i < slen; i++) {
		if(ssid[i] == '\\') {
			p = fmtchar(p, e, '\\');
			p = fmtchar(p, e, '\\');
		} else if(ssid[i] == ' ') {
			p = fmtchar(p, e, '\\');
			p = fmtchar(p, e, ' ');
		} else if(ssid[i] <= 0x20) {
			p = fmtchar(p, e, '\\');
			p = fmtchar(p, e, 'x');
			p = fmtbyte(p, e, ssid[i]);
		} else {
			p = fmtchar(p, e, ssid[i]);
		}
	}

	*p = '\0';
}

int saved_psk_prio(uint8_t* ssid, int slen)
{
	struct line ln;
	struct chunk ck[4];

	char ssidstr[3*32+4];
	prep_ssid(ssidstr, sizeof(ssidstr), ssid, slen);

	char* p;
	int prio;

	if(load_config())
		return -1;
	if(find_line(&ln, "psk", 3, ssidstr))
		return -1;
	if(split_line(&ln, ck, 4) < 4)
		return -1;
	if(!(p = parseint(ck[2].start, &prio)))
		return -1;
	if(p < ck[2].end)
		return -1;

	return prio;
}

int load_psk(uint8_t* ssid, int slen, char* psk, int plen)
{
	struct line ln;
	struct chunk ck[4];
	int ret = -ENOKEY;

	char ssidstr[3*32+4];
	prep_ssid(ssidstr, sizeof(ssidstr), ssid, slen);

	if(load_config())
		return ret;
	if(find_line(&ln, "psk", 3, ssidstr))
		return ret;
	if(split_line(&ln, ck, 4) < 4)
		return ret;

	struct chunk* cpsk = &ck[1];
	int clen = chunklen(cpsk);

	if(plen < clen + 1)
		return ret;

	memcpy(psk, cpsk->start, clen);
	psk[clen] = '\0';

	return 0;
}

void save_psk(uint8_t* ssid, int slen, char* psk)
{
	struct line ln;

	char buf[100];
	char* p = buf;
	char* e = buf + sizeof(buf) - 1;

	char ssidstr[3*32+4];
	prep_ssid(ssidstr, sizeof(ssidstr), ssid, slen);

	p = fmtstr(p, e, "psk");
	p = fmtstr(p, e, " ");
	p = fmtstr(p, e, psk); /* PSK is in hex */
	p = fmtstr(p, e, " ");
	p = fmtint(p, e, 1);   /* default priority */
	p = fmtstr(p, e, " ");
	p = fmtstr(p, e, ssidstr);

	if(load_config()) return;

	find_line(&ln, "psk", 3, ssidstr);
	save_line(&ln, buf, p - buf);
}

int set_psk_prio(uint8_t* ssid, int slen, int prio)
{
	struct line ln;
	char ssidstr[3*32+4];
	prep_ssid(ssidstr, sizeof(ssidstr), ssid, slen);
	struct chunk ck[4];

	if(load_config())
		return -ENOENT;
	if(find_line(&ln, "psk", 3, ssidstr))
		return -ENOENT;

	if(prio >= 10)
		return -EINVAL;
	if(prio < 0)
		drop_line(&ln);
	else if(split_line(&ln, ck, 3) < 3)
		return -EBADSLT;
	else {
		char buf[2] = { '0' + prio, '\0' };
		change_chunk(&ck[2], buf);
	}

	return 0;
}

static char* get_ifname(int ifi)
{
	struct link* ls;

	if(ifi <= 0)
		return NULL;

	for(ls = links; ls < links + nlinks; ls++)
		if(ls->ifi == ifi)
			return ls->name;

	return NULL;
}

/* Undo prep_ssid escaping */

static void read_ssid(struct chunk* ck)
{
	uint8_t* sp = wifi.ssid;
	uint8_t* se = wifi.ssid + sizeof(wifi.ssid);

	char* cp = ck->start;
	char* ce = ck->end;
	char* p;

	while(cp < ce && sp < se) {
		if(*cp != '\\') { /* non-escaped character */
			*sp++ = *cp++;
		} else {
			cp++; /* skip backslash */

			if(*cp == 'x') { /* \xAB style escape */
				if(cp + 3 > ce)
					goto err;
				if(!(p = parsebyte(cp + 1, sp)))
					goto err;
				cp = p; sp++;
			} else { /* single-character escape */
				*sp++ = *cp++;
			}
		}
	}

	wifi.slen = sp - wifi.ssid;
	return;
err:
	memzero(wifi.ssid, sizeof(wifi.ssid));
	wifi.slen = 0;
}

void load_wifi(struct link* ls)
{
	struct line ln;
	int cn = 10;
	struct chunk ck[cn];

	if(load_config())
		return;
	if(find_line(&ln, "wifi", 0, NULL))
		return;
	if((cn = split_line(&ln, ck, cn)) < 2)
		return;

	if(!(chunkis(&ck[1], ls->name)))
		return;

	wifi.ifi = ls->ifi;

	if(cn >= 3)
		wifi.mode = lookup(wifimodes, &ck[2]);
	if(cn >= 4 && wifi.mode == WM_FIXEDAP)
		read_ssid(&ck[3]);
}

void save_wifi(void)
{
	struct line ln;

	char buf[200];
	char* p = buf;
	char* e = buf + sizeof(buf) - 1;
	const char *mode, *name;

	if(load_config())
		return;

	if(!(name = get_ifname(wifi.ifi))) {
		find_line(&ln, "wifi", 0, NULL);
		drop_line(&ln);
		return;
	}

	p = fmtstr(p, e, "wifi");
	p = fmtstr(p, e, " ");

	p = fmtstr(p, e, name);
	p = fmtstr(p, e, " ");

	if((mode = nameof(wifimodes, wifi.mode)))
		p = fmtstr(p, e, mode);
	else
		p = fmtint(p, e, wifi.mode);

	if(wifi.mode == WM_FIXEDAP) {
		char ssidstr[3*32+4];
		prep_ssid(ssidstr, sizeof(ssidstr), wifi.ssid, wifi.slen);
		p = fmtstr(p, e, " ");
		p = fmtstr(p, e, ssidstr);
	}

	find_line(&ln, "wifi", 0, NULL);
	save_line(&ln, buf, p - buf);
}
