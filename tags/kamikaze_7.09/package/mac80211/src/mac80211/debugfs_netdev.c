/*
 * Copyright (c) 2006	Jiri Benc <jbenc@suse.cz>
 * Copyright 2007	Johannes Berg <johannes@sipsolutions.net>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/kernel.h>
#include <linux/device.h>
#include <linux/if.h>
#include <linux/interrupt.h>
#include <linux/netdevice.h>
#include <linux/rtnetlink.h>
#include <linux/notifier.h>
#include <net/mac80211.h>
#include <net/cfg80211.h>
#include "ieee80211_i.h"
#include "ieee80211_rate.h"
#include "debugfs.h"
#include "debugfs_netdev.h"

static ssize_t ieee80211_if_read(
	struct ieee80211_sub_if_data *sdata,
	char __user *userbuf,
	size_t count, loff_t *ppos,
	ssize_t (*format)(const struct ieee80211_sub_if_data *, char *, int))
{
	char buf[70];
	ssize_t ret = -EINVAL;

	read_lock(&dev_base_lock);
	if (sdata->dev->reg_state == NETREG_REGISTERED) {
		ret = (*format)(sdata, buf, sizeof(buf));
		ret = simple_read_from_buffer(userbuf, count, ppos, buf, ret);
	}
	read_unlock(&dev_base_lock);
	return ret;
}

#define IEEE80211_IF_FMT(name, field, format_string)			\
static ssize_t ieee80211_if_fmt_##name(					\
	const struct ieee80211_sub_if_data *sdata, char *buf,		\
	int buflen)							\
{									\
	return scnprintf(buf, buflen, format_string, sdata->field);	\
}
#define IEEE80211_IF_FMT_DEC(name, field)				\
		IEEE80211_IF_FMT(name, field, "%d\n")
#define IEEE80211_IF_FMT_HEX(name, field)				\
		IEEE80211_IF_FMT(name, field, "%#x\n")
#define IEEE80211_IF_FMT_SIZE(name, field)				\
		IEEE80211_IF_FMT(name, field, "%zd\n")

#define IEEE80211_IF_FMT_ATOMIC(name, field)				\
static ssize_t ieee80211_if_fmt_##name(					\
	const struct ieee80211_sub_if_data *sdata,			\
	char *buf, int buflen)						\
{									\
	return scnprintf(buf, buflen, "%d\n", atomic_read(&sdata->field));\
}

#define IEEE80211_IF_FMT_MAC(name, field)				\
static ssize_t ieee80211_if_fmt_##name(					\
	const struct ieee80211_sub_if_data *sdata, char *buf,		\
	int buflen)							\
{									\
	return scnprintf(buf, buflen, MAC_FMT "\n", MAC_ARG(sdata->field));\
}

#define __IEEE80211_IF_FILE(name)					\
static ssize_t ieee80211_if_read_##name(struct file *file,		\
					char __user *userbuf,		\
					size_t count, loff_t *ppos)	\
{									\
	return ieee80211_if_read(file->private_data,			\
				 userbuf, count, ppos,			\
				 ieee80211_if_fmt_##name);		\
}									\
static const struct file_operations name##_ops = {			\
	.read = ieee80211_if_read_##name,				\
	.open = mac80211_open_file_generic,				\
}

#define IEEE80211_IF_FILE(name, field, format)				\
		IEEE80211_IF_FMT_##format(name, field)			\
		__IEEE80211_IF_FILE(name)

#define DEBUGFS_QOS_FILE(name, f)					\
static ssize_t qos_ ##name## _write(struct file *file,			\
				    const char __user *userbuf,		\
				    size_t count, loff_t *ppos)		\
{									\
	struct ieee80211_sub_if_data *sdata = file->private_data;	\
									\
	f(sdata->dev, &sdata->u.sta, &sdata->u.sta.tspec);		\
									\
	return count;							\
}									\
									\
static const struct file_operations qos_ ##name## _ops = {		\
	.write = qos_ ##name## _write,					\
	.open = mac80211_open_file_generic,				\
};

#define DEBUGFS_QOS_ADD(name)						\
	sdata->debugfs.sta.qos.name = debugfs_create_file(#name, 0444, qosd,\
		sdata, &qos_ ##name## _ops);

#define DEBUGFS_QOS_DEL(name)						\
	do {								\
		debugfs_remove(sdata->debugfs.sta.qos.name);		\
		sdata->debugfs.sta.qos.name = NULL;			\
	} while (0)

DEBUGFS_QOS_FILE(addts_11e, ieee80211_send_addts);
DEBUGFS_QOS_FILE(addts_wmm, wmm_send_addts);
DEBUGFS_QOS_FILE(delts_11e, ieee80211_send_delts);
DEBUGFS_QOS_FILE(delts_wmm, wmm_send_delts);

static ssize_t qos_if_dls_mac(const struct ieee80211_sub_if_data *sdata,
			      char *buf, int buflen)
{
	return scnprintf(buf, buflen, MAC_FMT "\n",
			 MAC_ARG(sdata->u.sta.dls_mac));
}

static ssize_t qos_dls_mac_read(struct file *file,
				char __user *userbuf,
				size_t count, loff_t *ppos)
{
	return ieee80211_if_read(file->private_data,
				 userbuf, count, ppos,
				 qos_if_dls_mac);
}

static ssize_t qos_dls_mac_write(struct file *file, const char __user *userbuf,
				 size_t count, loff_t *ppos)
{
	struct ieee80211_sub_if_data *sdata = file->private_data;
	char buf[20];
	size_t size;
	u8 m[ETH_ALEN];

	size = min(sizeof(buf) - 1, count);
	buf[size] = '\0';
	if (copy_from_user(buf, userbuf, size))
		return -EFAULT;

	if (sscanf(buf, "%02hhx:%02hhx:%02hhx:%02hhx:%02hhx:%02hhx",
		   &((u8*)(m))[0], &((u8*)(m))[1], &((u8*)(m))[2],
		   &((u8*)(m))[3], &((u8*)(m))[4], &((u8*)(m))[5]) != ETH_ALEN){
		printk(KERN_ERR "%s: sscanf input error\n", sdata->dev->name);
		return -EINVAL;
	}
	memcpy(sdata->u.sta.dls_mac, m, ETH_ALEN);
	return count;
}

static const struct file_operations qos_dls_mac_ops = {
	.read = qos_dls_mac_read,
	.write = qos_dls_mac_write,
	.open = mac80211_open_file_generic,
};

static ssize_t qos_if_dls_op(const struct ieee80211_sub_if_data *sdata,
			     char *buf, int buflen)
{
	return scnprintf(buf, buflen,
			 "DLS Operation: Setup = 1; Teardown = 2\n");
}

static ssize_t qos_dls_op_read(struct file *file, char __user *userbuf,
			       size_t count, loff_t *ppos)
{
	return ieee80211_if_read(file->private_data,
				 userbuf, count, ppos,
				 qos_if_dls_op);
}

static ssize_t qos_dls_op_write(struct file *file, const char __user *userbuf,
				 size_t count, loff_t *ppos)
{
	struct ieee80211_sub_if_data *sdata = file->private_data;
	char buf[20];
	size_t size;
	unsigned int opt;

	size = min(sizeof(buf) - 1, count);
	buf[size] = '\0';
	if (copy_from_user(buf, userbuf, size))
		return -EFAULT;

	if (sscanf(buf, "%u", &opt) != 1) {
		printk(KERN_ERR "%s: sscanf input error\n", sdata->dev->name);
		return -EINVAL;
	}
	switch (opt) {
	case 1:
		ieee80211_send_dls_req(sdata->dev, &sdata->u.sta,
				       sdata->u.sta.dls_mac, 0);
		break;
	case 2:
		ieee80211_send_dls_teardown(sdata->dev, &sdata->u.sta,
					    sdata->u.sta.dls_mac,
					    WLAN_REASON_QSTA_NOT_USE);
		break;
	default:
		printk(KERN_ERR "Unknown DLS Operation: %d\n", opt);
		break;
	}
	return count;
}

static const struct file_operations qos_dls_op_ops = {
	.read = qos_dls_op_read,
	.write = qos_dls_op_write,
	.open = mac80211_open_file_generic,
};

#define DEBUGFS_TSINFO_FILE(_name, min_val, max_val)			\
static ssize_t tsinfo_ ##_name## _read(struct file *file,		\
				       char __user *userbuf,		\
				       size_t count, loff_t *ppos)	\
{									\
	char buf[20];							\
	struct ieee80211_sub_if_data *sdata = file->private_data;	\
	int res = scnprintf(buf, count, "%u\n",				\
		IEEE80211_TSINFO_## _name (sdata->u.sta.tspec.ts_info));\
	return simple_read_from_buffer(userbuf, count, ppos, buf, res);	\
}									\
									\
static ssize_t tsinfo_ ##_name## _write(struct file *file,		\
				        const char __user *userbuf,	\
				        size_t count, loff_t *ppos)	\
{									\
	char buf[20];							\
	size_t size;							\
	int val;							\
	struct ieee80211_sub_if_data *sdata = file->private_data;	\
									\
	size = min(sizeof(buf) - 1, count);				\
	buf[size] = '\0';						\
	if (copy_from_user(buf, userbuf, size))				\
		return -EFAULT;						\
									\
	val = simple_strtoul(buf, NULL, 0);				\
	if ((val < min_val) || (val > max_val)) {			\
		printk(KERN_ERR "%s: set value (%u) out of range "	\
		       "[%u, %u]\n",sdata->dev->name,val,min_val,max_val);\
		return -EINVAL;						\
	}								\
	IEEE80211_SET_TSINFO_ ##_name (sdata->u.sta.tspec.ts_info, val);\
	return count;							\
}									\
									\
static const struct file_operations tsinfo_ ##_name## _ops = {		\
	.read = tsinfo_ ##_name## _read,				\
	.write = tsinfo_ ##_name## _write,				\
	.open = mac80211_open_file_generic,				\
};

#define DEBUGFS_TSINFO_ADD_TSID						\
	sdata->debugfs.sta.tsinfo.tsid =				\
		debugfs_create_file("tsid", 0444, tsinfod,		\
				    sdata, &tsinfo_TSID_ops);

#define DEBUGFS_TSINFO_ADD_DIR						\
	sdata->debugfs.sta.tsinfo.direction =				\
		debugfs_create_file("direction", 0444, tsinfod,		\
				    sdata, &tsinfo_DIR_ops);

#define DEBUGFS_TSINFO_ADD_UP						\
	sdata->debugfs.sta.tsinfo.up =					\
		debugfs_create_file("up", 0444, tsinfod,		\
				    sdata, &tsinfo_UP_ops);

#define DEBUGFS_TSINFO_DEL(name)					\
	do {								\
		debugfs_remove(sdata->debugfs.sta.tsinfo.name);		\
		sdata->debugfs.sta.tsinfo.name = NULL;			\
	} while (0)

DEBUGFS_TSINFO_FILE(TSID, 8, 15);
DEBUGFS_TSINFO_FILE(DIR, 0, 3);
DEBUGFS_TSINFO_FILE(UP, 0, 7);

#define DEBUGFS_TSPEC_FILE(name, format_string, endian_f1, endian_f2)	\
static ssize_t tspec_ ##name## _read(struct file *file,			\
				      char __user *userbuf,		\
				      size_t count, loff_t *ppos)	\
{									\
	char buf[20];							\
	struct ieee80211_sub_if_data *sdata = file->private_data;	\
	int res = scnprintf(buf, count, format_string "\n",		\
			    endian_f1(sdata->u.sta.tspec.name));	\
	return simple_read_from_buffer(userbuf, count, ppos, buf, res);	\
}									\
									\
static ssize_t tspec_ ##name## _write(struct file *file,		\
				       const char __user *userbuf,	\
				       size_t count, loff_t *ppos)	\
{									\
	char buf[20];							\
	size_t size;							\
	struct ieee80211_sub_if_data *sdata = file->private_data;	\
									\
	size = min(sizeof(buf) - 1, count);				\
	buf[size] = '\0';						\
	if (copy_from_user(buf, userbuf, size))				\
		return -EFAULT;						\
									\
	sdata->u.sta.tspec.name = endian_f2(simple_strtoul(buf, NULL, 0));\
	return count;							\
}									\
									\
static const struct file_operations tspec_ ##name## _ops = {		\
	.read = tspec_ ##name## _read,					\
	.write = tspec_ ##name## _write,				\
	.open = mac80211_open_file_generic,				\
};

#define DEBUGFS_TSPEC_ADD(name)						\
	sdata->debugfs.sta.tspec.name = debugfs_create_file(#name,	\
		0444, tspecd, sdata, &tspec_ ##name## _ops);

#define DEBUGFS_TSPEC_DEL(name)						\
	do {								\
		debugfs_remove(sdata->debugfs.sta.tspec.name);		\
		sdata->debugfs.sta.tspec.name = NULL;			\
	} while (0)

DEBUGFS_TSPEC_FILE(nominal_msdu_size, "%hu", le16_to_cpu, cpu_to_le16);
DEBUGFS_TSPEC_FILE(max_msdu_size, "%hu", le16_to_cpu, cpu_to_le16);
DEBUGFS_TSPEC_FILE(min_service_interval, "%u", le32_to_cpu, cpu_to_le32);
DEBUGFS_TSPEC_FILE(max_service_interval, "%u", le32_to_cpu, cpu_to_le32);
DEBUGFS_TSPEC_FILE(inactivity_interval, "%u", le32_to_cpu, cpu_to_le32);
DEBUGFS_TSPEC_FILE(suspension_interval, "%u", le32_to_cpu, cpu_to_le32);
DEBUGFS_TSPEC_FILE(service_start_time, "%u", le32_to_cpu, cpu_to_le32);
DEBUGFS_TSPEC_FILE(min_data_rate, "%u", le32_to_cpu, cpu_to_le32);
DEBUGFS_TSPEC_FILE(mean_data_rate, "%u", le32_to_cpu, cpu_to_le32);
DEBUGFS_TSPEC_FILE(peak_data_rate, "%u", le32_to_cpu, cpu_to_le32);
DEBUGFS_TSPEC_FILE(burst_size, "%u", le32_to_cpu, cpu_to_le32);
DEBUGFS_TSPEC_FILE(delay_bound, "%u", le32_to_cpu, cpu_to_le32);
DEBUGFS_TSPEC_FILE(min_phy_rate, "%u", le32_to_cpu, cpu_to_le32);
DEBUGFS_TSPEC_FILE(surplus_band_allow, "%hu", le16_to_cpu, cpu_to_le16);
DEBUGFS_TSPEC_FILE(medium_time, "%hu", le16_to_cpu, cpu_to_le16);


/* common attributes */
IEEE80211_IF_FILE(channel_use, channel_use, DEC);
IEEE80211_IF_FILE(drop_unencrypted, drop_unencrypted, DEC);
IEEE80211_IF_FILE(eapol, eapol, DEC);
IEEE80211_IF_FILE(ieee8021_x, ieee802_1x, DEC);

/* STA/IBSS attributes */
IEEE80211_IF_FILE(state, u.sta.state, DEC);
IEEE80211_IF_FILE(bssid, u.sta.bssid, MAC);
IEEE80211_IF_FILE(prev_bssid, u.sta.prev_bssid, MAC);
IEEE80211_IF_FILE(ssid_len, u.sta.ssid_len, SIZE);
IEEE80211_IF_FILE(aid, u.sta.aid, DEC);
IEEE80211_IF_FILE(ap_capab, u.sta.ap_capab, HEX);
IEEE80211_IF_FILE(capab, u.sta.capab, HEX);
IEEE80211_IF_FILE(extra_ie_len, u.sta.extra_ie_len, SIZE);
IEEE80211_IF_FILE(auth_tries, u.sta.auth_tries, DEC);
IEEE80211_IF_FILE(assoc_tries, u.sta.assoc_tries, DEC);
IEEE80211_IF_FILE(auth_algs, u.sta.auth_algs, HEX);
IEEE80211_IF_FILE(auth_alg, u.sta.auth_alg, DEC);
IEEE80211_IF_FILE(auth_transaction, u.sta.auth_transaction, DEC);

static ssize_t ieee80211_if_fmt_flags(
	const struct ieee80211_sub_if_data *sdata, char *buf, int buflen)
{
	return scnprintf(buf, buflen, "%s%s%s%s%s%s%s\n",
			 sdata->u.sta.ssid_set ? "SSID\n" : "",
			 sdata->u.sta.bssid_set ? "BSSID\n" : "",
			 sdata->u.sta.prev_bssid_set ? "prev BSSID\n" : "",
			 sdata->u.sta.authenticated ? "AUTH\n" : "",
			 sdata->u.sta.associated ? "ASSOC\n" : "",
			 sdata->u.sta.probereq_poll ? "PROBEREQ POLL\n" : "",
			 sdata->u.sta.use_protection ? "CTS prot\n" : "");
}
__IEEE80211_IF_FILE(flags);

/* AP attributes */
IEEE80211_IF_FILE(num_sta_ps, u.ap.num_sta_ps, ATOMIC);
IEEE80211_IF_FILE(dtim_period, u.ap.dtim_period, DEC);
IEEE80211_IF_FILE(dtim_count, u.ap.dtim_count, DEC);
IEEE80211_IF_FILE(num_beacons, u.ap.num_beacons, DEC);
IEEE80211_IF_FILE(force_unicast_rateidx, u.ap.force_unicast_rateidx, DEC);
IEEE80211_IF_FILE(max_ratectrl_rateidx, u.ap.max_ratectrl_rateidx, DEC);

static ssize_t ieee80211_if_fmt_num_buffered_multicast(
	const struct ieee80211_sub_if_data *sdata, char *buf, int buflen)
{
	return scnprintf(buf, buflen, "%u\n",
			 skb_queue_len(&sdata->u.ap.ps_bc_buf));
}
__IEEE80211_IF_FILE(num_buffered_multicast);

static ssize_t ieee80211_if_fmt_beacon_head_len(
	const struct ieee80211_sub_if_data *sdata, char *buf, int buflen)
{
	if (sdata->u.ap.beacon_head)
		return scnprintf(buf, buflen, "%d\n",
				 sdata->u.ap.beacon_head_len);
	return scnprintf(buf, buflen, "\n");
}
__IEEE80211_IF_FILE(beacon_head_len);

static ssize_t ieee80211_if_fmt_beacon_tail_len(
	const struct ieee80211_sub_if_data *sdata, char *buf, int buflen)
{
	if (sdata->u.ap.beacon_tail)
		return scnprintf(buf, buflen, "%d\n",
				 sdata->u.ap.beacon_tail_len);
	return scnprintf(buf, buflen, "\n");
}
__IEEE80211_IF_FILE(beacon_tail_len);

/* WDS attributes */
IEEE80211_IF_FILE(peer, u.wds.remote_addr, MAC);

/* VLAN attributes */
IEEE80211_IF_FILE(vlan_id, u.vlan.id, DEC);

/* MONITOR attributes */
static ssize_t ieee80211_if_fmt_mode(
	const struct ieee80211_sub_if_data *sdata, char *buf, int buflen)
{
	struct ieee80211_local *local = sdata->local;

	return scnprintf(buf, buflen, "%s\n",
			 ((local->hw.flags & IEEE80211_HW_MONITOR_DURING_OPER) ||
			  local->open_count == local->monitors) ?
			 "hard" : "soft");
}
__IEEE80211_IF_FILE(mode);


#define DEBUGFS_ADD(name, type)\
	sdata->debugfs.type.name = debugfs_create_file(#name, 0444,\
		sdata->debugfsdir, sdata, &name##_ops);

static void add_sta_files(struct ieee80211_sub_if_data *sdata)
{
	struct dentry *qosd;
	struct dentry *tsinfod;
	struct dentry *tspecd;

	DEBUGFS_ADD(channel_use, sta);
	DEBUGFS_ADD(drop_unencrypted, sta);
	DEBUGFS_ADD(eapol, sta);
	DEBUGFS_ADD(ieee8021_x, sta);
	DEBUGFS_ADD(state, sta);
	DEBUGFS_ADD(bssid, sta);
	DEBUGFS_ADD(prev_bssid, sta);
	DEBUGFS_ADD(ssid_len, sta);
	DEBUGFS_ADD(aid, sta);
	DEBUGFS_ADD(ap_capab, sta);
	DEBUGFS_ADD(capab, sta);
	DEBUGFS_ADD(extra_ie_len, sta);
	DEBUGFS_ADD(auth_tries, sta);
	DEBUGFS_ADD(assoc_tries, sta);
	DEBUGFS_ADD(auth_algs, sta);
	DEBUGFS_ADD(auth_alg, sta);
	DEBUGFS_ADD(auth_transaction, sta);
	DEBUGFS_ADD(flags, sta);

	qosd = debugfs_create_dir("qos", sdata->debugfsdir);
	sdata->debugfs.sta.qos_dir = qosd;

	DEBUGFS_QOS_ADD(addts_11e);
	DEBUGFS_QOS_ADD(addts_wmm);
	DEBUGFS_QOS_ADD(delts_11e);
	DEBUGFS_QOS_ADD(delts_wmm);
	DEBUGFS_QOS_ADD(dls_mac);
	DEBUGFS_QOS_ADD(dls_op);

	tsinfod = debugfs_create_dir("ts_info", qosd);
	sdata->debugfs.sta.tsinfo_dir = tsinfod;

	DEBUGFS_TSINFO_ADD_TSID;
	DEBUGFS_TSINFO_ADD_DIR;
	DEBUGFS_TSINFO_ADD_UP;

	tspecd = debugfs_create_dir("tspec", qosd);
	sdata->debugfs.sta.tspec_dir = tspecd;

	DEBUGFS_TSPEC_ADD(nominal_msdu_size);
	DEBUGFS_TSPEC_ADD(max_msdu_size);
	DEBUGFS_TSPEC_ADD(min_service_interval);
	DEBUGFS_TSPEC_ADD(max_service_interval);
	DEBUGFS_TSPEC_ADD(inactivity_interval);
	DEBUGFS_TSPEC_ADD(suspension_interval);
	DEBUGFS_TSPEC_ADD(service_start_time);
	DEBUGFS_TSPEC_ADD(min_data_rate);
	DEBUGFS_TSPEC_ADD(mean_data_rate);
	DEBUGFS_TSPEC_ADD(peak_data_rate);
	DEBUGFS_TSPEC_ADD(burst_size);
	DEBUGFS_TSPEC_ADD(delay_bound);
	DEBUGFS_TSPEC_ADD(min_phy_rate);
	DEBUGFS_TSPEC_ADD(surplus_band_allow);
	DEBUGFS_TSPEC_ADD(medium_time);
}

static void add_ap_files(struct ieee80211_sub_if_data *sdata)
{
	DEBUGFS_ADD(channel_use, ap);
	DEBUGFS_ADD(drop_unencrypted, ap);
	DEBUGFS_ADD(eapol, ap);
	DEBUGFS_ADD(ieee8021_x, ap);
	DEBUGFS_ADD(num_sta_ps, ap);
	DEBUGFS_ADD(dtim_period, ap);
	DEBUGFS_ADD(dtim_count, ap);
	DEBUGFS_ADD(num_beacons, ap);
	DEBUGFS_ADD(force_unicast_rateidx, ap);
	DEBUGFS_ADD(max_ratectrl_rateidx, ap);
	DEBUGFS_ADD(num_buffered_multicast, ap);
	DEBUGFS_ADD(beacon_head_len, ap);
	DEBUGFS_ADD(beacon_tail_len, ap);
}

static void add_wds_files(struct ieee80211_sub_if_data *sdata)
{
	DEBUGFS_ADD(channel_use, wds);
	DEBUGFS_ADD(drop_unencrypted, wds);
	DEBUGFS_ADD(eapol, wds);
	DEBUGFS_ADD(ieee8021_x, wds);
	DEBUGFS_ADD(peer, wds);
}

static void add_vlan_files(struct ieee80211_sub_if_data *sdata)
{
	DEBUGFS_ADD(channel_use, vlan);
	DEBUGFS_ADD(drop_unencrypted, vlan);
	DEBUGFS_ADD(eapol, vlan);
	DEBUGFS_ADD(ieee8021_x, vlan);
	DEBUGFS_ADD(vlan_id, vlan);
}

static void add_monitor_files(struct ieee80211_sub_if_data *sdata)
{
	DEBUGFS_ADD(mode, monitor);
}

static void add_files(struct ieee80211_sub_if_data *sdata)
{
	if (!sdata->debugfsdir)
		return;

	switch (sdata->type) {
	case IEEE80211_IF_TYPE_STA:
	case IEEE80211_IF_TYPE_IBSS:
		add_sta_files(sdata);
		break;
	case IEEE80211_IF_TYPE_AP:
		add_ap_files(sdata);
		break;
	case IEEE80211_IF_TYPE_WDS:
		add_wds_files(sdata);
		break;
	case IEEE80211_IF_TYPE_MNTR:
		add_monitor_files(sdata);
		break;
	case IEEE80211_IF_TYPE_VLAN:
		add_vlan_files(sdata);
		break;
	default:
		break;
	}
}

#define DEBUGFS_DEL(name, type)					\
	do {							\
		debugfs_remove(sdata->debugfs.type.name);	\
		sdata->debugfs.type.name = NULL;		\
	} while (0)

static void del_sta_files(struct ieee80211_sub_if_data *sdata)
{
	DEBUGFS_DEL(channel_use, sta);
	DEBUGFS_DEL(drop_unencrypted, sta);
	DEBUGFS_DEL(eapol, sta);
	DEBUGFS_DEL(ieee8021_x, sta);
	DEBUGFS_DEL(state, sta);
	DEBUGFS_DEL(bssid, sta);
	DEBUGFS_DEL(prev_bssid, sta);
	DEBUGFS_DEL(ssid_len, sta);
	DEBUGFS_DEL(aid, sta);
	DEBUGFS_DEL(ap_capab, sta);
	DEBUGFS_DEL(capab, sta);
	DEBUGFS_DEL(extra_ie_len, sta);
	DEBUGFS_DEL(auth_tries, sta);
	DEBUGFS_DEL(assoc_tries, sta);
	DEBUGFS_DEL(auth_algs, sta);
	DEBUGFS_DEL(auth_alg, sta);
	DEBUGFS_DEL(auth_transaction, sta);
	DEBUGFS_DEL(flags, sta);

	DEBUGFS_TSINFO_DEL(tsid);
	DEBUGFS_TSINFO_DEL(direction);
	DEBUGFS_TSINFO_DEL(up);

	DEBUGFS_TSPEC_DEL(nominal_msdu_size);
	DEBUGFS_TSPEC_DEL(max_msdu_size);
	DEBUGFS_TSPEC_DEL(min_service_interval);
	DEBUGFS_TSPEC_DEL(max_service_interval);
	DEBUGFS_TSPEC_DEL(inactivity_interval);
	DEBUGFS_TSPEC_DEL(suspension_interval);
	DEBUGFS_TSPEC_DEL(service_start_time);
	DEBUGFS_TSPEC_DEL(min_data_rate);
	DEBUGFS_TSPEC_DEL(mean_data_rate);
	DEBUGFS_TSPEC_DEL(peak_data_rate);
	DEBUGFS_TSPEC_DEL(burst_size);
	DEBUGFS_TSPEC_DEL(delay_bound);
	DEBUGFS_TSPEC_DEL(min_phy_rate);
	DEBUGFS_TSPEC_DEL(surplus_band_allow);
	DEBUGFS_TSPEC_DEL(medium_time);

	DEBUGFS_QOS_DEL(addts_11e);
	DEBUGFS_QOS_DEL(addts_wmm);
	DEBUGFS_QOS_DEL(delts_11e);
	DEBUGFS_QOS_DEL(delts_wmm);
	DEBUGFS_QOS_DEL(dls_mac);
	DEBUGFS_QOS_DEL(dls_op);

	debugfs_remove(sdata->debugfs.sta.tspec_dir);
	sdata->debugfs.sta.tspec_dir = NULL;
	debugfs_remove(sdata->debugfs.sta.tsinfo_dir);
	sdata->debugfs.sta.tsinfo_dir = NULL;
	debugfs_remove(sdata->debugfs.sta.qos_dir);
	sdata->debugfs.sta.qos_dir = NULL;
}

static void del_ap_files(struct ieee80211_sub_if_data *sdata)
{
	DEBUGFS_DEL(channel_use, ap);
	DEBUGFS_DEL(drop_unencrypted, ap);
	DEBUGFS_DEL(eapol, ap);
	DEBUGFS_DEL(ieee8021_x, ap);
	DEBUGFS_DEL(num_sta_ps, ap);
	DEBUGFS_DEL(dtim_period, ap);
	DEBUGFS_DEL(dtim_count, ap);
	DEBUGFS_DEL(num_beacons, ap);
	DEBUGFS_DEL(force_unicast_rateidx, ap);
	DEBUGFS_DEL(max_ratectrl_rateidx, ap);
	DEBUGFS_DEL(num_buffered_multicast, ap);
	DEBUGFS_DEL(beacon_head_len, ap);
	DEBUGFS_DEL(beacon_tail_len, ap);
}

static void del_wds_files(struct ieee80211_sub_if_data *sdata)
{
	DEBUGFS_DEL(channel_use, wds);
	DEBUGFS_DEL(drop_unencrypted, wds);
	DEBUGFS_DEL(eapol, wds);
	DEBUGFS_DEL(ieee8021_x, wds);
	DEBUGFS_DEL(peer, wds);
}

static void del_vlan_files(struct ieee80211_sub_if_data *sdata)
{
	DEBUGFS_DEL(channel_use, vlan);
	DEBUGFS_DEL(drop_unencrypted, vlan);
	DEBUGFS_DEL(eapol, vlan);
	DEBUGFS_DEL(ieee8021_x, vlan);
	DEBUGFS_DEL(vlan_id, vlan);
}

static void del_monitor_files(struct ieee80211_sub_if_data *sdata)
{
	DEBUGFS_DEL(mode, monitor);
}

static void del_files(struct ieee80211_sub_if_data *sdata, int type)
{
	if (!sdata->debugfsdir)
		return;

	switch (type) {
	case IEEE80211_IF_TYPE_STA:
	case IEEE80211_IF_TYPE_IBSS:
		del_sta_files(sdata);
		break;
	case IEEE80211_IF_TYPE_AP:
		del_ap_files(sdata);
		break;
	case IEEE80211_IF_TYPE_WDS:
		del_wds_files(sdata);
		break;
	case IEEE80211_IF_TYPE_MNTR:
		del_monitor_files(sdata);
		break;
	case IEEE80211_IF_TYPE_VLAN:
		del_vlan_files(sdata);
		break;
	default:
		break;
	}
}

static int notif_registered;

void ieee80211_debugfs_add_netdev(struct ieee80211_sub_if_data *sdata)
{
	char buf[10+IFNAMSIZ];

	if (!notif_registered)
		return;

	sprintf(buf, "netdev:%s", sdata->dev->name);
	sdata->debugfsdir = debugfs_create_dir(buf,
		sdata->local->hw.wiphy->debugfsdir);
}

void ieee80211_debugfs_remove_netdev(struct ieee80211_sub_if_data *sdata)
{
	del_files(sdata, sdata->type);
	debugfs_remove(sdata->debugfsdir);
	sdata->debugfsdir = NULL;
}

void ieee80211_debugfs_change_if_type(struct ieee80211_sub_if_data *sdata,
				      int oldtype)
{
	del_files(sdata, oldtype);
	add_files(sdata);
}

static int netdev_notify(struct notifier_block * nb,
			 unsigned long state,
			 void *ndev)
{
	struct net_device *dev = ndev;
	/* TODO
	char buf[10+IFNAMSIZ];
	*/

	if (state != NETDEV_CHANGENAME)
		return 0;

	if (!dev->ieee80211_ptr || !dev->ieee80211_ptr->wiphy)
		return 0;

	if (dev->ieee80211_ptr->wiphy->privid != mac80211_wiphy_privid)
		return 0;

	/* TODO
	sprintf(buf, "netdev:%s", dev->name);
	debugfs_rename(IEEE80211_DEV_TO_SUB_IF(dev)->debugfsdir, buf);
	*/

	return 0;
}

static struct notifier_block mac80211_debugfs_netdev_notifier = {
	.notifier_call = netdev_notify,
};

void ieee80211_debugfs_netdev_init(void)
{
	int err;

	err = register_netdevice_notifier(&mac80211_debugfs_netdev_notifier);
	if (err) {
		printk(KERN_ERR
		       "mac80211: failed to install netdev notifier,"
		       " disabling per-netdev debugfs!\n");
	} else
		notif_registered = 1;
}

void ieee80211_debugfs_netdev_exit(void)
{
	unregister_netdevice_notifier(&mac80211_debugfs_netdev_notifier);
	notif_registered = 0;
}
