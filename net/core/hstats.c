// SPDX-License-Identifier: (GPL-2.0-only OR BSD-2-Clause)
/* Copyright (C) 2019 Netronome Systems, Inc. */

#include <linux/bitmap.h>
#include <linux/err.h>
#include <linux/if_link.h>
#include <linux/kernel.h>
#include <linux/netdevice.h>
#include <net/hstats.h>
#include <net/netlink.h>

/* We deploy a simple stack-based dumper to walk the hierarchies.
 * This is the documentation format for quick analysis of the state machine:
 *
 *   Header (in case there are move than one possibility):
 *
 * o |  direct action 1 \ __ these are performed by the code
 * r |  direct action 2 /
 * d |  --------------
 * e | |  STACK CMD 1 | \ __  these are popped from the stack and run
 * r v |  STACK CMD 2 | /     in order after current handler completes
 *      ============== <---- top of the stack before current handler
 */
enum hstat_dumper_cmd {
	/*      open grp
	 *   put const quals
	 *   ---------------
	 *  |   DUMP STATS  |
	 *  |    CLOSE grp  |
	 *   ===============
	 */
	HSTAT_DCMD_GRP_LOAD,
	/* dump all statitics
	 *   ---------------
	 *  |  LOAD child0  |
	 *  |  LOAD child1  |
	 *   ===============
	 */
	HSTAT_DCMD_GRP_DUMP,
	/* close grp */
	HSTAT_DCMD_GRP_CLOSE,
	/* count root group (netlink restart index) */
	HSTAT_DCMD_ROOT_GRP_DONE,
};

struct hstat_dumper {
	struct sk_buff *skb;
	struct net_device *dev;
	/* For sizing we only have a const pointer to dev */
	const struct net_device *const_dev;
	int err;

	/* For calculating skb size */
	bool sizing;
	size_t size;

	int current_root_grp;
	int last_completed_root_grp;

	u8 *cmd_stack;
	size_t cmd_stack_top;
	size_t cmd_stack_len;
};

struct hstat_dumper_cmd_simple {
	u64 cmd;
};

struct hstat_dumper_cmd_grp_load {
	const struct rtnl_hstat_group *grp;
	u64 cmd;
};

struct hstat_dumper_cmd_grp_dump {
	const struct rtnl_hstat_group *grp;
	u64 cmd;
};

struct hstat_dumper_cmd_grp_close {
	struct nlattr *nl_attr;
	u64 cmd;
};

/* RTNL helpers */
static const int rtnl_qual2ifla[RTNL_HSTATS_QUAL_CNT] = {
	[RTNL_HSTATS_QUAL_TYPE]		= IFLA_HSTATS_QUAL_TYPE,
	[RTNL_HSTATS_QUAL_DIRECTION]	= IFLA_HSTATS_QUAL_DIRECTION,
};

static bool rtnl_hstat_qualifier_present(const struct rtnl_hstat_qualifier *q)
{
	return q->constant;
}

/* Dumper basics */
static u64 hstat_dumper_peek_cmd(struct hstat_dumper *dumper)
{
	return *(u64 *)(dumper->cmd_stack + dumper->cmd_stack_top - 8);
}

static int hstat_dumper_discard(struct hstat_dumper *dumper, size_t len)
{
	if (WARN_ON_ONCE(dumper->cmd_stack_top < len))
		return -EINVAL;
	dumper->cmd_stack_top -= len;
	return 0;
}

static int hstat_dumper_pop(struct hstat_dumper *dumper, void *dst, size_t len)
{
	if (WARN_ON_ONCE(dumper->cmd_stack_top < len))
		return -EINVAL;
	dumper->cmd_stack_top -= len;
	memcpy(dst, dumper->cmd_stack + dumper->cmd_stack_top, len);
	return 0;
}

static bool hstat_dumper_done(struct hstat_dumper *dumper)
{
	return !dumper->cmd_stack_top;
}

static int hstat_dumper_error(struct hstat_dumper *dumper)
{
	if (WARN_ON_ONCE(dumper->cmd_stack_top && dumper->cmd_stack_top < 8))
		return -EINVAL;
	return 0;
}

static struct hstat_dumper *
hstat_dumper_init(struct sk_buff *skb, const struct net_device *const_dev,
		  struct net_device *dev, int *prividx)
{
	struct hstat_dumper *dumper;

	dumper = kzalloc(sizeof(*dumper), GFP_KERNEL);
	if (!dumper)
		return NULL;
	dumper->cmd_stack = kmalloc(8096, GFP_KERNEL);
	if (!dumper->cmd_stack) {
		kfree(dumper);
		return NULL;
	}
	dumper->cmd_stack_len = 8096;

	dumper->skb = skb;
	dumper->dev = dev;
	dumper->const_dev = const_dev;
	if (prividx)
		dumper->last_completed_root_grp = *prividx;
	else
		dumper->sizing = true;

	return dumper;
}

static void hstat_dumper_destroy(struct hstat_dumper *dumper)
{
	kfree(dumper->cmd_stack);
	kfree(dumper);
}

/* Dumper pushers */
static int
__hstat_dumper_push_cmd(struct hstat_dumper *dumper, void *data, size_t len)
{
	/* All structures pushed must be multiple of 8 w/ cmd as last member */
	if (WARN_ON_ONCE(len % 8))
		return -EINVAL;

	while (dumper->cmd_stack_len - dumper->cmd_stack_top < len) {
		void *st;

		st = krealloc(dumper->cmd_stack, dumper->cmd_stack_len * 2,
			      GFP_KERNEL);
		if (!st)
			return -ENOMEM;

		dumper->cmd_stack = st;
		dumper->cmd_stack_len *= 2;
	}

	memcpy(dumper->cmd_stack + dumper->cmd_stack_top, data, len);
	dumper->cmd_stack_top += len;
	return 0;
}

static int
hstat_dumper_push_grp_load(struct hstat_dumper *dumper,
			   const struct rtnl_hstat_group *grp)
{
	struct hstat_dumper_cmd_grp_load cmd = {
		.cmd =		HSTAT_DCMD_GRP_LOAD,
		.grp =		grp,
	};

	return __hstat_dumper_push_cmd(dumper, &cmd, sizeof(cmd));
}

static int
hstat_dumper_push_dump(struct hstat_dumper *dumper,
		       const struct rtnl_hstat_group *grp)
{
	struct hstat_dumper_cmd_grp_dump cmd = {
		.cmd =		HSTAT_DCMD_GRP_DUMP,
		.grp =		grp,
	};

	return __hstat_dumper_push_cmd(dumper, &cmd, sizeof(cmd));
}

static int
hstat_dumper_push_grp_close(struct hstat_dumper *dumper, struct nlattr *nl_grp)
{
	struct hstat_dumper_cmd_grp_close cmd = {
		.cmd =		HSTAT_DCMD_GRP_CLOSE,
		.nl_attr =	nl_grp,
	};

	return __hstat_dumper_push_cmd(dumper, &cmd, sizeof(cmd));
}

static int
hstat_dumper_push_root_grp_done(struct hstat_dumper *dumper)
{
	struct hstat_dumper_cmd_simple cmd = { HSTAT_DCMD_ROOT_GRP_DONE };

	return __hstat_dumper_push_cmd(dumper, &cmd, sizeof(cmd));
}

/* Dumper actions */
static int hstat_dumper_open_grp(struct hstat_dumper *dumper)
{
	struct nlattr *nl_grp;
	int err;

	if (dumper->sizing) {
		dumper->size += nla_total_size(0); /* IFLA_HSTATS_GROUP */
		return 0;
	}

	/* Open group nlattr and push onto the stack a close command */
	nl_grp = nla_nest_start(dumper->skb, IFLA_HSTATS_GROUP);
	if (!nl_grp)
		return -EMSGSIZE;

	err = hstat_dumper_push_grp_close(dumper, nl_grp);
	if (err) {
		nla_nest_cancel(dumper->skb, nl_grp);
		return err;
	}

	return 0;
}

static int
hstat_dumper_grp_put_stats(struct hstat_dumper *dumper,
			   const struct rtnl_hstat_group *grp)
{
	struct rtnl_hstat_req dump_req;
	struct nlattr *nl_stats;
	int err;

	WARN_ON_ONCE(!grp->stats_cnt != !grp->get_stats);

	if (!grp->stats_cnt)
		return 0;

	if (dumper->sizing) {
		dumper->size += nla_total_size(0);
		dumper->size += grp->stats_cnt * nla_total_size(8);
		return 0;
	}

	nl_stats = nla_nest_start(dumper->skb, IFLA_HSTATS_STATS);
	if (!nl_stats)
		return -EMSGSIZE;

	memset(&dump_req, 0, sizeof(dump_req));
	dump_req.dumper = dumper;
	dump_req.skb = dumper->skb;

	err = grp->get_stats(dumper->dev, &dump_req, grp);
	if (err)
		goto err_cancel_stats;
	err = dump_req.err;
	if (err)
		goto err_cancel_stats;

	nla_nest_end(dumper->skb, nl_stats);
	return 0;

err_cancel_stats:
	nla_nest_cancel(dumper->skb, nl_stats);
	return err;
}

static int
hstat_dumper_put_qual(struct hstat_dumper *dumper, int i, u32 val)
{
	if (dumper->sizing) {
		dumper->size += nla_total_size(sizeof(u32));
		return 0;
	}

	return nla_put_u32(dumper->skb, rtnl_qual2ifla[i], val);
}

/* Dumper handlers */
static int hstat_dumper_grp_load(struct hstat_dumper *dumper)
{
	struct hstat_dumper_cmd_grp_load cmd;
	int i, err;

	err = hstat_dumper_pop(dumper, &cmd, sizeof(cmd));
	if (err)
		return err;
	if (dumper->err)
		return 0;

	if (dumper->current_root_grp < dumper->last_completed_root_grp)
		return 0;

	err = hstat_dumper_open_grp(dumper);
	if (err)
		return err;

	for (i = 0; i < RTNL_HSTATS_QUAL_CNT; i++) {
		const struct rtnl_hstat_qualifier *q;

		q = &cmd.grp->qualifiers[i];
		if (!rtnl_hstat_qualifier_present(q))
			continue;

		if (q->constant) {
			err = hstat_dumper_put_qual(dumper, i, q->constant);
			if (err)
				return err;
		}
	}

	return hstat_dumper_push_dump(dumper, cmd.grp);
}

static int hstat_dumper_grp_dump(struct hstat_dumper *dumper)
{
	struct hstat_dumper_cmd_grp_dump cmd;
	int err;

	err = hstat_dumper_pop(dumper, &cmd, sizeof(cmd));
	if (err)
		return err;
	if (dumper->err)
		return 0;

	err = hstat_dumper_grp_put_stats(dumper, cmd.grp);
	if (err)
		return err;

	if (cmd.grp->has_children) {
		const struct rtnl_hstat_group *const *grp;

		for (grp = cmd.grp->children; *grp; grp++) {
			err = hstat_dumper_push_grp_load(dumper, *grp);
			if (err)
				return err;
		}
	}

	return 0;
}

static int hstat_dumper_grp_close(struct hstat_dumper *dumper)
{
	struct hstat_dumper_cmd_grp_close cmd;
	int err;

	err = hstat_dumper_pop(dumper, &cmd, sizeof(cmd));
	if (err)
		return err;

	if (!dumper->err)
		nla_nest_end(dumper->skb, cmd.nl_attr);
	else
		nla_nest_cancel(dumper->skb, cmd.nl_attr);
	return 0;
}

static int hstat_dumper_root_grp_done(struct hstat_dumper *dumper)
{
	int err;

	err = hstat_dumper_discard(dumper, sizeof(u64));
	if (err)
		return err;
	if (dumper->err)
		return 0;

	dumper->current_root_grp++;
	return 0;
}

static int hstat_dumper_run(struct hstat_dumper *dumper)
{
	do {
		int err;
		u64 cmd;

		err = hstat_dumper_error(dumper);
		if (err)
			return err;
		if (hstat_dumper_done(dumper))
			return 0;

		cmd = hstat_dumper_peek_cmd(dumper);
		switch (cmd) {
		case HSTAT_DCMD_ROOT_GRP_DONE:
			err = hstat_dumper_root_grp_done(dumper);
			break;
		case HSTAT_DCMD_GRP_LOAD:
			err = hstat_dumper_grp_load(dumper);
			break;
		case HSTAT_DCMD_GRP_CLOSE:
			err = hstat_dumper_grp_close(dumper);
			break;
		case HSTAT_DCMD_GRP_DUMP:
			err = hstat_dumper_grp_dump(dumper);
			break;
		}
		if (err && !dumper->err)
			/* Record the errror hand keep invoking handlers,
			 * handlers will see the error and only do clean up.
			 */
			dumper->err = err;
	} while (true);

	return dumper->err;
}

/* Driver helpers */
void
rtnl_hstat_add_grp(struct rtnl_hstat_req *req,
		   const struct rtnl_hstat_group *grp)
{
	if (!req->err)
		req->err = hstat_dumper_push_root_grp_done(req->dumper);
	if (!req->err)
		req->err = hstat_dumper_push_grp_load(req->dumper, grp);
}
EXPORT_SYMBOL(rtnl_hstat_add_grp);

/* Stack call points */
static size_t
__rtnl_get_link_hstats(struct sk_buff *skb, const struct net_device *const_dev,
		       struct net_device *dev, int *prividx)
{
	struct hstat_dumper *dumper;
	struct rtnl_hstat_req req;
	ssize_t ret;

	if (!dev->netdev_ops || !dev->netdev_ops->ndo_hstat_get_groups)
		return -ENODATA;

	dumper = hstat_dumper_init(skb, const_dev, dev, prividx);
	if (!dumper)
		return -ENOMEM;

	memset(&req, 0, sizeof(req));
	req.dumper = dumper;

	ret = dev->netdev_ops->ndo_hstat_get_groups(dev, &req);
	if (ret < 0)
		goto exit_dumper_destroy;
	ret = req.err;
	if (ret)
		goto exit_dumper_destroy;

	if (hstat_dumper_done(dumper))
		return -ENODATA;

	ret = hstat_dumper_run(dumper);
	if (prividx) {
		if (ret)
			*prividx = dumper->current_root_grp;
		else
			*prividx = 0;
	} else if (ret >= 0) {
		ret = dumper->size;
	}

exit_dumper_destroy:
	hstat_dumper_destroy(dumper);
	return ret;
}

size_t rtnl_get_link_hstats_size(const struct net_device *dev)
{
	ssize_t ret;

	ret = __rtnl_get_link_hstats(NULL, dev, NULL, NULL);
	return ret;
}

size_t
rtnl_get_link_hstats(struct sk_buff *skb, struct net_device *dev, int *prividx)
{
	ssize_t ret;

	ret = __rtnl_get_link_hstats(skb, dev, dev, prividx);
	return ret;
}
