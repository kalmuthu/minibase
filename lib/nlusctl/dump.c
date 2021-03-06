#include <printf.h>
#include <string.h>

#include <netlink.h>
#include <netlink/dump.h>
#include <nlusctl.h>

/* This assumes that (struct nlattr == struct ucattr) */

void uc_dump(struct ucmsg* msg)
{
	int paylen = msg->len - sizeof(*msg);

	if(paylen > 0)
		tracef("NLUS cmd=%i payload %i\n", msg->cmd, paylen);
	else
		tracef("NLUS cmd=%i\n", msg->cmd);

	nl_dump_attrs_in(msg->payload, paylen);
}
