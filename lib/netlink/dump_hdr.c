#include <printf.h>

#include "base.h"
#include "dump.h"
#include "attr.h"
#include "rtnl/addr.h"
#include "rtnl/link.h"
#include "rtnl/route.h"

static void nl_dump_msg_hdr(struct nlmsg* msg)
{
	tracef("MSG len=%i type=%i flags=%X seq=%i pid=%i\n",
		msg->len, msg->type, msg->flags, msg->seq, msg->pid);
}

void nl_dump_msg(struct nlmsg* msg)
{
	nl_dump_msg_hdr(msg);

	int paylen = msg->len - sizeof(*msg);

	if(paylen > 0)
		nl_hexdump(msg->payload, paylen);
}

void nl_dump_gen(struct nlgen* gen)
{
	nl_dump_msg_hdr(&gen->nlm);

	tracef(" GENL cmd=%i version=%i\n", gen->cmd, gen->version);

	nl_dump_attrs_in(NLPAYLOAD(gen));
}

void nl_dump_err(struct nlerr* msg)
{
	struct nlmsg* nlm = &msg->nlm;

	if(msg->errno)
		tracef("ERR len=%i type=%i flags=%X seq=%i pid=%i errno=%i\n",
			nlm->len, nlm->type, nlm->flags, nlm->seq, nlm->pid,
			msg->errno);
	else
		tracef("ACK len=%i type=%i flags=%X seq=%i pid=%i\n",
			nlm->len, nlm->type, nlm->flags, nlm->seq, nlm->pid);

	tracef("  > len=%i type=%i flags=%X seq=%i pid=%i\n",
		msg->len, msg->type, msg->flags, msg->seq, msg->pid);
}

void nl_dump_ifinfo(struct ifinfomsg* msg)
{
	nl_dump_msg_hdr(&msg->nlm);

	tracef(" IFINFO family=%i type=%i index=%i flags=%04X change=%04X\n",
			msg->family, msg->type, msg->index,
			msg->flags, msg->change);

	nl_dump_attrs_in(NLPAYLOAD(msg));
}

void nl_dump_ifaddr(struct ifaddrmsg* msg)
{
	nl_dump_msg_hdr(&msg->nlm);

	tracef(" IFADDR family=%i prefix=%i flags=%i scope=%i index=%i\n",
			msg->family, msg->prefixlen, msg->flags, msg->scope,
			msg->index);

	nl_dump_attrs_in(NLPAYLOAD(msg));
}

void nl_dump_rtmsg(struct rtmsg* msg)
{
	nl_dump_msg_hdr(&msg->nlm);

	tracef(" RTMSG family=%i dst_len=%i src_len=%i tos=%i\n",
		msg->family, msg->dst_len, msg->src_len, msg->tos);
	tracef("       table=%i protocol=%i scope=%i type=%i flags=%X\n",
		msg->table, msg->protocol, msg->scope, msg->type, msg->flags);

	nl_dump_attrs_in(NLPAYLOAD(msg));
}

#define trycast(msg, tt, ff) \
	if(msg->len >= sizeof(tt)) \
		return ff((tt*)msg); \
	else \
		break

void nl_dump_rtnl(struct nlmsg* msg)
{
	switch(msg->type) {
		case RTM_NEWLINK:
		case RTM_DELLINK:
		case RTM_GETLINK:
			trycast(msg, struct ifinfomsg, nl_dump_ifinfo);
		case RTM_NEWADDR:
		case RTM_DELADDR:
		case RTM_GETADDR:
			trycast(msg, struct ifaddrmsg, nl_dump_ifaddr);
		case RTM_NEWROUTE:
		case RTM_DELROUTE:
		case RTM_GETROUTE:
			trycast(msg, struct rtmsg, nl_dump_rtmsg);
		default:
			nl_dump_msg(msg);
	}
}

void nl_dump_genl(struct nlmsg* msg)
{
	struct nlerr* err;
	struct nlgen* gen;

	if((err = nl_err(msg)))
		nl_dump_err(err);
	else if((gen = nl_gen(msg)))
		nl_dump_gen(gen);
	else
		nl_dump_msg(msg);
}
