/* Copyright (C) 2000-2002 Joakim Axelsson <gozem@linux.nu>
 *                         Patrick Schaaf <bof@bof.de>
 *                         Martin Josefsson <gandalf@wlug.westbo.se>
 * Copyright (C) 2003-2011 Jozsef Kadlecsik <kadlec@blackhole.kfki.hu>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

/* Kernel module which implements the set match and SET target
 * for netfilter/iptables. */

#include <linux/module.h>
#include <linux/skbuff.h>
#include <linux/version.h>

#include <linux/netfilter/x_tables.h>
#include <linux/netfilter/xt_set.h>

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Jozsef Kadlecsik <kadlec@blackhole.kfki.hu>");
MODULE_DESCRIPTION("Xtables: IP set match and target module");
MODULE_ALIAS("xt_SET");
MODULE_ALIAS("ipt_set");
MODULE_ALIAS("ip6t_set");
MODULE_ALIAS("ipt_SET");
MODULE_ALIAS("ip6t_SET");

static inline int
match_set(ip_set_id_t index, const struct sk_buff *skb,
	  u8 pf, u8 dim, u8 flags, int inv)
{
	if (ip_set_test(index, skb, pf, dim, flags))
		inv = !inv;
	return inv;
}

/* Revision 0 interface: backward compatible with netfilter/iptables */

/* Backward compatibility constrains (incomplete):
 *  2.6.24: [NETLINK]: Introduce nested and byteorder flag to netlink attribute
 *  2.6.25: is_vmalloc_addr(): Check if an address is within the vmalloc
 *	    boundaries
 *  2.6.27: rcu: split list.h and move rcu-protected lists into rculist.h
 *  2.6.28: netfilter: ctnetlink: remove bogus module dependency between
 *	    ctnetlink and nf_nat (nfnl_lock/nfnl_unlock)
 *  2.6.29: generic swap(): introduce global macro swap(a, b)
 *  2.6.31: netfilter: passive OS fingerprint xtables match
 *  2.6.34: rcu: Add lockdep-enabled variants of rcu_dereference()
 */

#if LINUX_VERSION_CODE < KERNEL_VERSION(2, 6, 34)
#error "Linux kernel version too old: must be >= 2.6.34"
#endif

#if LINUX_VERSION_CODE < KERNEL_VERSION(2, 6, 35)
#define CHECK_OK	1
#define CHECK_FAIL	0
#else /* LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,35) */
#define	CHECK_OK	0
#define CHECK_FAIL	(-EINVAL)
#endif

#if LINUX_VERSION_CODE < KERNEL_VERSION(2, 6, 35)
static bool
set_match_v0(const struct sk_buff *skb, const struct xt_match_param *par)
#else /* LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,35) */
static bool
set_match_v0(const struct sk_buff *skb, struct xt_action_param *par)
#endif
{
	const struct xt_set_info_match_v0 *info = par->matchinfo;

	return match_set(info->match_set.index, skb, par->family,
			 info->match_set.u.compat.dim,
			 info->match_set.u.compat.flags,
			 info->match_set.u.compat.flags & IPSET_INV_MATCH);
}

static void
compat_flags(struct xt_set_info_v0 *info)
{
	u_int8_t i;

	/* Fill out compatibility data according to enum ip_set_kopt */
	info->u.compat.dim = IPSET_DIM_ZERO;
	if (info->u.flags[0] & IPSET_MATCH_INV)
		info->u.compat.flags |= IPSET_INV_MATCH;
	for (i = 0; i < IPSET_DIM_MAX-1 && info->u.flags[i]; i++) {
		info->u.compat.dim++;
		if (info->u.flags[i] & IPSET_SRC)
			info->u.compat.flags |= (1<<info->u.compat.dim);
	}
}

#if LINUX_VERSION_CODE < KERNEL_VERSION(2, 6, 35)
static bool
set_match_v0_checkentry(const struct xt_mtchk_param *par)
#else /* LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,35) */
static int
set_match_v0_checkentry(const struct xt_mtchk_param *par)
#endif
{
	struct xt_set_info_match_v0 *info = par->matchinfo;
	ip_set_id_t index;

	index = ip_set_nfnl_get_byindex(info->match_set.index);

	if (index == IPSET_INVALID_ID) {
		pr_warning("Cannot find set indentified by id %u to match",
			   info->match_set.index);
		return CHECK_FAIL;	/* error */
	}
	if (info->match_set.u.flags[IPSET_DIM_MAX-1] != 0) {
		pr_warning("That's nasty!");
		return CHECK_FAIL;	/* error */
	}

	/* Fill out compatibility data */
	compat_flags(&info->match_set);

	return CHECK_OK;
}

static void
set_match_v0_destroy(const struct xt_mtdtor_param *par)
{
	struct xt_set_info_match_v0 *info = par->matchinfo;

	ip_set_nfnl_put(info->match_set.index);
}

#if LINUX_VERSION_CODE < KERNEL_VERSION(2, 6, 35)
static unsigned int
set_target_v0(struct sk_buff *skb, const struct xt_target_param *par)
#else /* LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,35) */
static unsigned int
set_target_v0(struct sk_buff *skb, const struct xt_action_param *par)
#endif
{
	const struct xt_set_info_target_v0 *info = par->targinfo;

	if (info->add_set.index != IPSET_INVALID_ID)
		ip_set_add(info->add_set.index, skb, par->family,
			   info->add_set.u.compat.dim,
			   info->add_set.u.compat.flags);
	if (info->del_set.index != IPSET_INVALID_ID)
		ip_set_del(info->del_set.index, skb, par->family,
			   info->del_set.u.compat.dim,
			   info->del_set.u.compat.flags);

	return XT_CONTINUE;
}

#if LINUX_VERSION_CODE < KERNEL_VERSION(2, 6, 35)
static bool
set_target_v0_checkentry(const struct xt_tgchk_param *par)
#else /* LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,35) */
static int
set_target_v0_checkentry(const struct xt_tgchk_param *par)
#endif
{
	struct xt_set_info_target_v0 *info = par->targinfo;
	ip_set_id_t index;

	if (info->add_set.index != IPSET_INVALID_ID) {
		index = ip_set_nfnl_get_byindex(info->add_set.index);
		if (index == IPSET_INVALID_ID) {
			pr_warning("cannot find add_set index %u as target",
				   info->add_set.index);
			return CHECK_FAIL;	/* error */
		}
	}

	if (info->del_set.index != IPSET_INVALID_ID) {
		index = ip_set_nfnl_get_byindex(info->del_set.index);
		if (index == IPSET_INVALID_ID) {
			pr_warning("cannot find del_set index %u as target",
				   info->del_set.index);
			return CHECK_FAIL;	/* error */
		}
	}
	if (info->add_set.u.flags[IPSET_DIM_MAX-1] != 0 ||
	    info->del_set.u.flags[IPSET_DIM_MAX-1] != 0) {
		pr_warning("That's nasty!");
		return CHECK_FAIL;	/* error */
	}

	/* Fill out compatibility data */
	compat_flags(&info->add_set);
	compat_flags(&info->del_set);

	return CHECK_OK;
}

static void
set_target_v0_destroy(const struct xt_tgdtor_param *par)
{
	const struct xt_set_info_target_v0 *info = par->targinfo;

	if (info->add_set.index != IPSET_INVALID_ID)
		ip_set_nfnl_put(info->add_set.index);
	if (info->del_set.index != IPSET_INVALID_ID)
		ip_set_nfnl_put(info->del_set.index);
}

/* Revision 1: current interface to netfilter/iptables */

#if LINUX_VERSION_CODE < KERNEL_VERSION(2, 6, 35)
static bool
set_match(const struct sk_buff *skb, const struct xt_match_param *par)
#else /* LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,35) */
static bool
set_match(const struct sk_buff *skb, struct xt_action_param *par)
#endif
{
	const struct xt_set_info_match *info = par->matchinfo;

	return match_set(info->match_set.index, skb, par->family,
			 info->match_set.dim,
			 info->match_set.flags,
			 info->match_set.flags & IPSET_INV_MATCH);
}

#if LINUX_VERSION_CODE < KERNEL_VERSION(2, 6, 35)
static bool
set_match_checkentry(const struct xt_mtchk_param *par)
#else /* LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,35) */
static int
set_match_checkentry(const struct xt_mtchk_param *par)
#endif
{
	struct xt_set_info_match *info = par->matchinfo;
	ip_set_id_t index;

	index = ip_set_nfnl_get_byindex(info->match_set.index);

	if (index == IPSET_INVALID_ID) {
		pr_warning("Cannot find set indentified by id %u to match",
			   info->match_set.index);
		return CHECK_FAIL;	/* error */
	}
	if (info->match_set.dim > IPSET_DIM_MAX) {
		pr_warning("That's nasty!");
		return CHECK_FAIL;	/* error */
	}

	return CHECK_OK;
}

static void
set_match_destroy(const struct xt_mtdtor_param *par)
{
	struct xt_set_info_match *info = par->matchinfo;

	ip_set_nfnl_put(info->match_set.index);
}

#if LINUX_VERSION_CODE < KERNEL_VERSION(2, 6, 35)
static unsigned int
set_target(struct sk_buff *skb, const struct xt_target_param *par)
#else /* LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,35) */
static unsigned int
set_target(struct sk_buff *skb, const struct xt_action_param *par)
#endif
{
	const struct xt_set_info_target *info = par->targinfo;

	if (info->add_set.index != IPSET_INVALID_ID)
		ip_set_add(info->add_set.index,
			   skb, par->family,
			   info->add_set.dim,
			   info->add_set.flags);
	if (info->del_set.index != IPSET_INVALID_ID)
		ip_set_del(info->del_set.index,
			   skb, par->family,
			   info->add_set.dim,
			   info->del_set.flags);

	return XT_CONTINUE;
}

#if LINUX_VERSION_CODE < KERNEL_VERSION(2, 6, 35)
static bool
set_target_checkentry(const struct xt_tgchk_param *par)
#else /* LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,35) */
static int
set_target_checkentry(const struct xt_tgchk_param *par)
#endif
{
	const struct xt_set_info_target *info = par->targinfo;
	ip_set_id_t index;

	if (info->add_set.index != IPSET_INVALID_ID) {
		index = ip_set_nfnl_get_byindex(info->add_set.index);
		if (index == IPSET_INVALID_ID) {
			pr_warning("cannot find add_set index %u as target",
				   info->add_set.index);
			return CHECK_FAIL;	/* error */
		}
	}

	if (info->del_set.index != IPSET_INVALID_ID) {
		index = ip_set_nfnl_get_byindex(info->del_set.index);
		if (index == IPSET_INVALID_ID) {
			pr_warning("cannot find del_set index %u as target",
				   info->del_set.index);
			return CHECK_FAIL;	/* error */
		}
	}
	if (info->add_set.dim > IPSET_DIM_MAX ||
	    info->del_set.flags > IPSET_DIM_MAX) {
		pr_warning("That's nasty!");
		return CHECK_FAIL;	/* error */
	}

	return CHECK_OK;
}

static void
set_target_destroy(const struct xt_tgdtor_param *par)
{
	const struct xt_set_info_target *info = par->targinfo;

	if (info->add_set.index != IPSET_INVALID_ID)
		ip_set_nfnl_put(info->add_set.index);
	if (info->del_set.index != IPSET_INVALID_ID)
		ip_set_nfnl_put(info->del_set.index);
}

static struct xt_match set_matches[] __read_mostly = {
	{
		.name		= "set",
		.family		= NFPROTO_IPV4,
		.revision	= 0,
		.match		= set_match_v0,
		.matchsize	= sizeof(struct xt_set_info_match_v0),
		.checkentry	= set_match_v0_checkentry,
		.destroy	= set_match_v0_destroy,
		.me		= THIS_MODULE
	},
	{
		.name		= "set",
		.family		= NFPROTO_IPV4,
		.revision	= 1,
		.match		= set_match,
		.matchsize	= sizeof(struct xt_set_info_match),
		.checkentry	= set_match_checkentry,
		.destroy	= set_match_destroy,
		.me		= THIS_MODULE
	},
	{
		.name		= "set",
		.family		= NFPROTO_IPV6,
		.revision	= 1,
		.match		= set_match,
		.matchsize	= sizeof(struct xt_set_info_match),
		.checkentry	= set_match_checkentry,
		.destroy	= set_match_destroy,
		.me		= THIS_MODULE
	},
};

static struct xt_target set_targets[] __read_mostly = {
	{
		.name		= "SET",
		.revision	= 0,
		.family		= NFPROTO_IPV4,
		.target		= set_target_v0,
		.targetsize	= sizeof(struct xt_set_info_target_v0),
		.checkentry	= set_target_v0_checkentry,
		.destroy	= set_target_v0_destroy,
		.me		= THIS_MODULE
	},
	{
		.name		= "SET",
		.revision	= 1,
		.family		= NFPROTO_IPV4,
		.target		= set_target,
		.targetsize	= sizeof(struct xt_set_info_target),
		.checkentry	= set_target_checkentry,
		.destroy	= set_target_destroy,
		.me		= THIS_MODULE
	},
	{
		.name		= "SET",
		.revision	= 1,
		.family		= NFPROTO_IPV6,
		.target		= set_target,
		.targetsize	= sizeof(struct xt_set_info_target),
		.checkentry	= set_target_checkentry,
		.destroy	= set_target_destroy,
		.me		= THIS_MODULE
	},
};

static int __init xt_set_init(void)
{
	int ret = xt_register_matches(set_matches, ARRAY_SIZE(set_matches));

	if (!ret) {
		ret = xt_register_targets(set_targets,
					  ARRAY_SIZE(set_targets));
		if (ret)
			xt_unregister_matches(set_matches,
					      ARRAY_SIZE(set_matches));
	}
	return ret;
}

static void __exit xt_set_fini(void)
{
	xt_unregister_matches(set_matches, ARRAY_SIZE(set_matches));
	xt_unregister_targets(set_targets, ARRAY_SIZE(set_targets));
}

module_init(xt_set_init);
module_exit(xt_set_fini);
