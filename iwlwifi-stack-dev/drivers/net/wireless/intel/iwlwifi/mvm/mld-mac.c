// SPDX-License-Identifier: GPL-2.0 OR BSD-3-Clause
/*
 * Copyright (C) 2022 Intel Corporation
 */
#include "mvm.h"

static void iwl_mvm_mld_mac_ctxt_cmd_common(struct iwl_mvm *mvm,
					    struct ieee80211_vif *vif,
					    struct iwl_mac_config_cmd *cmd,
					    u32 action)
{
	struct iwl_mvm_vif *mvmvif = iwl_mvm_vif_from_mac80211(vif);

	cmd->id_and_color = cpu_to_le32(mvmvif->id);
	cmd->action = cpu_to_le32(action);

	cmd->mac_type = cpu_to_le32(iwl_mvm_get_mac_type(vif));

	memcpy(cmd->local_mld_addr, vif->addr, ETH_ALEN);

	cmd->filter_flags = cpu_to_le32(0);
	cmd->he_support = cpu_to_le32(0);
	cmd->eht_support = cpu_to_le32(0);

	cmd->nic_not_ack_enabled =
		cpu_to_le32(!iwl_mvm_is_nic_ack_enabled(mvm, vif));

	if (iwlwifi_mod_params.disable_11ax)
		return;
	cmd->he_support = cpu_to_le32(vif->bss_conf.he_support);

	if (!iwlwifi_mod_params.disable_11be && cmd->he_support)
		cmd->eht_support = cpu_to_le32(vif->bss_conf.eht_support);
}

static int iwl_mvm_mld_mac_ctxt_send_cmd(struct iwl_mvm *mvm,
					 struct iwl_mac_config_cmd *cmd)
{
	int ret = iwl_mvm_send_cmd_pdu(mvm,
				       WIDE_ID(MAC_CONF_GROUP, MAC_CONFIG_CMD),
				       0, sizeof(*cmd), cmd);
	if (ret)
		IWL_ERR(mvm, "Failed to send MAC_CONFIG_CMD (action:%d): %d\n",
			le32_to_cpu(cmd->action), ret);
	return ret;
}

static int iwl_mvm_mld_mac_ctxt_cmd_sta(struct iwl_mvm *mvm,
					struct ieee80211_vif *vif,
					u32 action, bool force_assoc_off)
{
	struct iwl_mac_config_cmd cmd = {};

	WARN_ON(vif->type != NL80211_IFTYPE_STATION);

	/* Fill the common data for all mac context types */
	iwl_mvm_mld_mac_ctxt_cmd_common(mvm, vif, &cmd, action);

	/*
	 * We always want to hear MCAST frames, if we're not authorized yet,
	 * we'll drop them.
	 */
	cmd.filter_flags |= cpu_to_le32(MAC_CFG_FILTER_ACCEPT_GRP);

	if (vif->p2p)
		cmd.client.ctwin =
			iwl_mvm_mac_ctxt_cmd_p2p_sta_get_oppps_ctwin(mvm, vif);

	if (vif->cfg.assoc && vif->bss_conf.dtim_period &&
	    !force_assoc_off) {
		struct iwl_mvm_vif *mvmvif = iwl_mvm_vif_from_mac80211(vif);

		cmd.client.is_assoc = cpu_to_le32(1);

		if (!mvmvif->authorized &&
		    fw_has_capa(&mvm->fw->ucode_capa,
				IWL_UCODE_TLV_CAPA_COEX_HIGH_PRIO))
			cmd.client.data_policy |=
				cpu_to_le32(COEX_HIGH_PRIORITY_ENABLE);

	} else {
		cmd.client.is_assoc = cpu_to_le32(0);

		/* Allow beacons to pass through as long as we are not
		 * associated, or we do not have dtim period information.
		 */
		cmd.filter_flags |= cpu_to_le32(MAC_CFG_FILTER_ACCEPT_BEACON);
	}

	cmd.client.assoc_id = cpu_to_le32(vif->cfg.aid);

	if (vif->probe_req_reg && vif->cfg.assoc && vif->p2p)
		cmd.filter_flags |= cpu_to_le32(MAC_CFG_FILTER_ACCEPT_PROBE_REQ);

	if (vif->bss_conf.he_support && !iwlwifi_mod_params.disable_11ax)
		cmd.client.data_policy |=
			iwl_mvm_mac_ctxt_cmd_sta_get_twt_policy(mvm, vif);

	return iwl_mvm_mld_mac_ctxt_send_cmd(mvm, &cmd);
}

static int iwl_mvm_mld_mac_ctxt_cmd_listener(struct iwl_mvm *mvm,
					     struct ieee80211_vif *vif,
					     u32 action)
{
	struct iwl_mac_config_cmd cmd = {};

	WARN_ON(vif->type != NL80211_IFTYPE_MONITOR);

	iwl_mvm_mld_mac_ctxt_cmd_common(mvm, vif, &cmd, action);

	cmd.filter_flags = cpu_to_le32(MAC_CFG_FILTER_PROMISC |
				       MAC_FILTER_IN_CONTROL_AND_MGMT |
				       MAC_CFG_FILTER_ACCEPT_BEACON |
				       MAC_CFG_FILTER_ACCEPT_PROBE_REQ |
				       MAC_CFG_FILTER_ACCEPT_GRP);

	return iwl_mvm_mld_mac_ctxt_send_cmd(mvm, &cmd);
}

static int iwl_mvm_mld_mac_ctxt_cmd_ibss(struct iwl_mvm *mvm,
					 struct ieee80211_vif *vif,
					 u32 action)
{
	struct iwl_mvm_vif *mvmvif = iwl_mvm_vif_from_mac80211(vif);
	struct iwl_mac_config_cmd cmd = {};

	WARN_ON(vif->type != NL80211_IFTYPE_ADHOC);

	iwl_mvm_mld_mac_ctxt_cmd_common(mvm, vif, &cmd, action);

	cmd.filter_flags = cpu_to_le32(MAC_CFG_FILTER_ACCEPT_BEACON |
				       MAC_CFG_FILTER_ACCEPT_PROBE_REQ |
				       MAC_CFG_FILTER_ACCEPT_GRP);

	/* TODO: Assumes that the beacon id == mac context id */
	cmd.go_ibss.beacon_template = cpu_to_le32(mvmvif->id);

	return iwl_mvm_mld_mac_ctxt_send_cmd(mvm, &cmd);
}

static int iwl_mvm_mld_mac_ctxt_cmd_p2p_device(struct iwl_mvm *mvm,
					       struct ieee80211_vif *vif,
					       u32 action)
{
	struct iwl_mac_config_cmd cmd = {};

	WARN_ON(vif->type != NL80211_IFTYPE_P2P_DEVICE);

	iwl_mvm_mld_mac_ctxt_cmd_common(mvm, vif, &cmd, action);

	cmd.p2p_dev.is_disc_extended =
		iwl_mac_ctxt_p2p_dev_has_extended_disc(mvm, vif);

	/* Override the filter flags to accept only probe requests */
	cmd.filter_flags = cpu_to_le32(MAC_CFG_FILTER_ACCEPT_PROBE_REQ);

	return iwl_mvm_mld_mac_ctxt_send_cmd(mvm, &cmd);
}

static int iwl_mvm_mld_mac_ctxt_cmd_ap_go(struct iwl_mvm *mvm,
					  struct ieee80211_vif *vif,
					  u32 action)
{
	struct iwl_mvm_vif *mvmvif = iwl_mvm_vif_from_mac80211(vif);
	struct iwl_mac_config_cmd cmd = {};

	WARN_ON(vif->type != NL80211_IFTYPE_AP);

	/* Fill the common data for all mac context types */
	iwl_mvm_mld_mac_ctxt_cmd_common(mvm, vif, &cmd, action);

	iwl_mvm_mac_ctxt_cmd_ap_set_filter_flags(mvm, mvmvif,
						 &cmd.filter_flags,
						 MAC_CFG_FILTER_ACCEPT_PROBE_REQ,
						 MAC_CFG_FILTER_ACCEPT_BEACON);

	/* TODO: Assume that the beacon id == mac context id */
	cmd.go_ibss.beacon_template = cpu_to_le32(mvmvif->id);

	return iwl_mvm_mld_mac_ctxt_send_cmd(mvm, &cmd);
}

static int iwl_mvm_mld_mac_ctx_send(struct iwl_mvm *mvm,
				    struct ieee80211_vif *vif,
				    u32 action, bool force_assoc_off)
{
	switch (vif->type) {
	case NL80211_IFTYPE_STATION:
		return iwl_mvm_mld_mac_ctxt_cmd_sta(mvm, vif, action,
						    force_assoc_off);
	case NL80211_IFTYPE_AP:
		return iwl_mvm_mld_mac_ctxt_cmd_ap_go(mvm, vif, action);
	case NL80211_IFTYPE_MONITOR:
		return iwl_mvm_mld_mac_ctxt_cmd_listener(mvm, vif, action);
	case NL80211_IFTYPE_P2P_DEVICE:
		return iwl_mvm_mld_mac_ctxt_cmd_p2p_device(mvm, vif, action);
	case NL80211_IFTYPE_ADHOC:
		return iwl_mvm_mld_mac_ctxt_cmd_ibss(mvm, vif, action);
	default:
		break;
	}

	return -EOPNOTSUPP;
}

int iwl_mvm_mld_mac_ctxt_add(struct iwl_mvm *mvm, struct ieee80211_vif *vif)
{
	struct iwl_mvm_vif *mvmvif = iwl_mvm_vif_from_mac80211(vif);
	int ret;

	if (WARN_ON_ONCE(vif->type == NL80211_IFTYPE_NAN))
		return -EOPNOTSUPP;

	if (WARN_ONCE(mvmvif->uploaded, "Adding active MAC %pM/%d\n",
		      vif->addr, ieee80211_vif_type_p2p(vif)))
		return -EIO;

	ret = iwl_mvm_mld_mac_ctx_send(mvm, vif, FW_CTXT_ACTION_ADD,
				       true);
	if (ret)
		return ret;

	/* will only do anything at resume from D3 time */
	iwl_mvm_set_last_nonqos_seq(mvm, vif);

	mvmvif->uploaded = true;
	return 0;
}

int iwl_mvm_mld_mac_ctxt_changed(struct iwl_mvm *mvm,
				 struct ieee80211_vif *vif,
				 bool force_assoc_off)
{
	struct iwl_mvm_vif *mvmvif = iwl_mvm_vif_from_mac80211(vif);

	if (WARN_ON_ONCE(vif->type == NL80211_IFTYPE_NAN))
		return -EOPNOTSUPP;

	if (WARN_ONCE(!mvmvif->uploaded, "Changing inactive MAC %pM/%d\n",
		      vif->addr, ieee80211_vif_type_p2p(vif)))
		return -EIO;

	return iwl_mvm_mld_mac_ctx_send(mvm, vif, FW_CTXT_ACTION_MODIFY,
					force_assoc_off);
}

int iwl_mvm_mld_mac_ctxt_remove(struct iwl_mvm *mvm, struct ieee80211_vif *vif)
{
	struct iwl_mvm_vif *mvmvif = iwl_mvm_vif_from_mac80211(vif);
	struct iwl_mac_config_cmd cmd = {
		.action = cpu_to_le32(FW_CTXT_ACTION_REMOVE),
		.id_and_color = cpu_to_le32(mvmvif->id),
	};
	int ret;

	if (WARN_ON_ONCE(vif->type == NL80211_IFTYPE_NAN))
		return -EOPNOTSUPP;

	if (WARN_ONCE(!mvmvif->uploaded, "Removing inactive MAC %pM/%d\n",
		      vif->addr, ieee80211_vif_type_p2p(vif)))
		return -EIO;

	ret = iwl_mvm_mld_mac_ctxt_send_cmd(mvm, &cmd);
	if (ret)
		return ret;

	mvmvif->uploaded = false;

	return 0;
}
