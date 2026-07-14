#pragma once
#include <noxx/int.hpp>

#include "dot11.hpp"

// firmware command interface (ref common/morse_commands.h, semver 57.0.0).
// request structs cover the payload after the command header; response
// structs cover the payload after the header and status word (see
// send_command in command.hpp). commands without a payload have no struct.
namespace halow {
using dot11::MacAddr;

// command ids (enum morse_cmd_id); the stats-log ids carry opaque payloads
struct CommandId {
    enum : u16 {
        SetChannel           = 0x0001,
        GetVersion           = 0x0002,
        SetTxPower           = 0x0003,
        AddInterface         = 0x0004,
        RemoveInterface      = 0x0005,
        BssConfig            = 0x0006,
        InstallKey           = 0x000A,
        DisableKey           = 0x000B,
        ScanConfig           = 0x0010,
        SetQosParams         = 0x0011,
        GetChannelFull       = 0x0013,
        SetStaState          = 0x0014,
        SetBssColor          = 0x0015,
        ConfigPs             = 0x0016,
        TurboMode            = 0x0018,
        HealthCheck          = 0x0019,
        GetChannelDtim       = 0x001C,
        GetChannel           = 0x001D,
        ArpOffload           = 0x0020,
        SetLongSleepConfig   = 0x0021,
        SetDutyCycle         = 0x0022,
        GetDutyCycle         = 0x0023,
        GetMaxTxPower        = 0x0024,
        GetCapabilities      = 0x0025,
        TwtAgreementInstall  = 0x0026,
        TwtAgreementRemove   = 0x0027,
        MpswConfig           = 0x0030,
        StandbyMode          = 0x0031,
        DhcpOffload          = 0x0032,
        UpdateOuiFilter      = 0x0034,
        TwtAgreementValidate = 0x0036,
        GetSetGenericParam   = 0x003E,
        HwScan               = 0x0044,
        SetWhitelist         = 0x0045,
        ArpPeriodicRefresh   = 0x0046,
        SetTcpKeepalive      = 0x0047,
        LiSleep              = 0x0049,
        SequenceNumberSpaces = 0x004B,
        SetCqmRssi           = 0x004F,

        SetControlResponse = 0x1009,

        HostStatsLog   = 0x2007,
        HostStatsReset = 0x2008,
        MacStatsLog    = 0x200C,
        MacStatsReset  = 0x200D,
        UphyStatsLog   = 0x200E,
        UphyStatsReset = 0x200F,

        SetResponseIndication = 0x8007,
        SetTransmissionRate   = 0x8009,
        SetNdpProbeSupport    = 0x800C,
        ForceAssert           = 0x800E,

        IotConfigureInterop = 0xB000,
        IotSendAddba        = 0xB001,
        IotStaReassoc       = 0xB002,
        IotDumpStats        = 0xBF00,
        IotReadStats        = 0xBF01,
    };
};

// event ids, arriving unsolicited on the command channel (MORSE_CMD_ID_EVT_*)
struct EventId {
    enum : u16 {
        BeaconLoss         = 0x4002,
        UmacTrafficControl = 0x4004,
        DhcpLeaseUpdate    = 0x4005,
        HwScanDone         = 0x4011,
        ConnectionLoss     = 0x4013,
        CqmRssiNotify      = 0x4015,
    };
};

// header flags field (MORSE_CMD_TYPE_*)
struct CommandFlag {
    enum : u16 {
        Req   = 1 << 0,
        Resp  = 1 << 1,
        Event = 1 << 2,
    };
};

// every command, response and event starts with this (struct morse_cmd_header)
struct CommandHeader {
    u16 flags; // CommandFlag
    u16 message_id;
    u16 len; // payload bytes after this header
    u16 host_id;
    u16 vif_id;
    u16 pad;
} __attribute__((packed));
static_assert(sizeof(CommandHeader) == 12);

// responses carry a status word before their payload (struct morse_cmd_resp)
struct CommandResponse {
    CommandHeader header;
    u32           status;
    u8            data[];
} __attribute__((packed));

// SET_CHANNEL / GET_CHANNEL

constexpr auto channel_bw_not_set   = u8(0xff);        // MORSE_CMD_CHANNEL_BW_NOT_SET
constexpr auto channel_idx_not_set  = u8(0xff);        // MORSE_CMD_CHANNEL_IDX_NOT_SET
constexpr auto channel_freq_not_set = u32(0xffffffff); // MORSE_CMD_CHANNEL_FREQ_NOT_SET

// enum morse_cmd_dot11_proto_mode
struct Dot11ProtoMode {
    enum : u8 {
        Ah  = 0,
        B   = 1,
        Bg  = 2,
        Gn  = 3,
        Bgn = 4,
    };
};

// struct morse_cmd_req_set_channel
struct SetChannelReq {
    u32 op_chan_freq_hz;
    u8  op_bw_mhz;
    u8  pri_bw_mhz;
    u8  pri_1mhz_chan_idx;
    u8  dot11_mode; // Dot11ProtoMode
    u8  deprecated_reg_tx_power_set;
    u8  is_off_channel;
} __attribute__((packed));
static_assert(sizeof(SetChannelReq) == 10);

// struct morse_cmd_resp_set_channel
struct SetChannelResp {
    i32 power_qdbm;
} __attribute__((packed));

// struct morse_cmd_resp_get_channel
struct GetChannelResp {
    u32 op_chan_freq_hz;
    u8  op_chan_bw_mhz;
    u8  pri_chan_bw_mhz;
    u8  pri_1mhz_chan_idx;
} __attribute__((packed));

// GET_VERSION

constexpr auto max_version_len = usize(128); // MORSE_CMD_MAX_VERSION_LEN

// struct morse_cmd_resp_get_version
struct GetVersionResp {
    i32 length;
    u8  version[];
} __attribute__((packed));

// SET_TXPOWER / GET_MAX_TXPOWER

// struct morse_cmd_req_set_txpower
struct SetTxPowerReq {
    i32 power_qdbm;
} __attribute__((packed));

// struct morse_cmd_resp_set_txpower, morse_cmd_resp_get_max_txpower
struct TxPowerResp {
    i32 power_qdbm;
} __attribute__((packed));

// ADD_INTERFACE / REMOVE_INTERFACE

// enum morse_cmd_interface_type
struct InterfaceType {
    enum : u32 {
        Invalid = 0,
        Sta     = 1,
        Ap      = 2,
        Mon     = 3,
        Adhoc   = 4,
        Mesh    = 5,
    };
};

// struct morse_cmd_req_add_interface
struct AddInterfaceReq {
    MacAddr addr;
    u32     interface_type; // InterfaceType
} __attribute__((packed));
static_assert(sizeof(AddInterfaceReq) == 10);

// BSS_CONFIG

// struct morse_cmd_req_bss_config
struct BssConfigReq {
    u16 beacon_interval_tu;
    u16 dtim_period;
    u8  padding[2];
    u32 cssid;
} __attribute__((packed));

// SCAN_CONFIG

// struct morse_cmd_req_scan_config
struct ScanConfigReq {
    u8 enabled;
    u8 is_survey;
} __attribute__((packed));

// SET_QOS_PARAMS

// struct morse_cmd_req_set_qos_params
struct SetQosParamsReq {
    u8  uapsd;
    u8  queue_idx;
    u8  aifs_slot_count;
    u16 contention_window_min;
    u16 contention_window_max;
    u32 max_txop_usec;
} __attribute__((packed));

// SET_STA_STATE

// enum morse_cmd_ieee80211_sta_state
struct StaState {
    enum : u16 {
        NotExist         = 0,
        None             = 1,
        Authenticated    = 2,
        Associated       = 3,
        Authorized       = 4,
        AuthorizedAsleep = 5,
    };
};

// MORSE_CMD_STA_FLAG_*
struct StaFlag {
    enum : u32 {
        S1gPv1 = 1 << 0,
    };
};

// struct morse_cmd_req_set_sta_state
struct SetStaStateReq {
    MacAddr sta_addr;
    u16     aid;
    u16     state; // StaState
    u8      uapsd_queues;
    u32     flags; // StaFlag
} __attribute__((packed));
static_assert(sizeof(SetStaStateReq) == 15);

// SET_BSS_COLOR

// struct morse_cmd_req_set_bss_color
struct SetBssColorReq {
    u8 bss_color;
} __attribute__((packed));

// CONFIG_PS

// struct morse_cmd_req_config_ps
struct ConfigPsReq {
    u8 enabled;
    u8 dynamic_ps_offload;
} __attribute__((packed));

// ARP_OFFLOAD

constexpr auto arp_offload_max_ips = usize(4); // MORSE_CMD_ARP_OFFLOAD_MAX_IP_ADDRESSES

// struct morse_cmd_req_arp_offload
struct ArpOffloadReq {
    u32 ip_table[arp_offload_max_ips];
} __attribute__((packed));

// SET_LONG_SLEEP_CONFIG

// struct morse_cmd_req_set_long_sleep_config
struct SetLongSleepConfigReq {
    u8 enabled;
} __attribute__((packed));

// SET_DUTY_CYCLE / GET_DUTY_CYCLE

// MORSE_CMD_DUTY_CYCLE_SET_CFG_*, which config fields to apply
struct DutyCycleSetCfg {
    enum : u8 {
        DutyCycle       = 1 << 0,
        OmitControlResp = 1 << 1,
        Ext             = 1 << 2,
        BurstRecordUnit = 1 << 3,
    };
};

// enum morse_cmd_duty_cycle_mode
struct DutyCycleMode {
    enum : u8 {
        Spread = 0,
        Burst  = 1,
    };
};

// struct morse_cmd_duty_cycle_configuration
struct DutyCycleConfig {
    u8  omit_control_responses;
    u32 duty_cycle;
} __attribute__((packed));

// struct morse_cmd_duty_cycle_set_configuration_ext
struct DutyCycleSetConfigExt {
    u32 burst_record_unit_us;
    u8  mode; // DutyCycleMode
} __attribute__((packed));

// struct morse_cmd_duty_cycle_configuration_ext
struct DutyCycleConfigExt {
    u32                   airtime_remaining_us;
    u32                   burst_window_duration_us;
    DutyCycleSetConfigExt set;
} __attribute__((packed));

// struct morse_cmd_req_set_duty_cycle
struct SetDutyCycleReq {
    DutyCycleConfig       config;
    u8                    set_cfgs; // DutyCycleSetCfg
    DutyCycleSetConfigExt config_ext;
} __attribute__((packed));

// struct morse_cmd_resp_get_duty_cycle
struct GetDutyCycleResp {
    DutyCycleConfig    config;
    DutyCycleConfigExt config_ext;
} __attribute__((packed));

// GET_CAPABILITIES

// MORSE_CMD_SET_S1G_CAP_*, which capability fields the host overrides
struct SetS1gCap {
    enum : u32 {
        Flags        = 1 << 0,
        AmpduMss     = 1 << 1,
        BeamSts      = 1 << 2,
        NumSoundDims = 1 << 3,
        MaxAmpduLexp = 1 << 4,
        MmssOffset   = 1 << 5,
    };
};

constexpr auto capability_flags_width = usize(4); // MORSE_CMD_S1G_CAPABILITY_FLAGS_WIDTH

// struct morse_cmd_mm_capabilities
struct Capabilities {
    u32 flags[capability_flags_width];
    u8  ampdu_mss;
    u8  beamformee_sts_capability;
    u8  number_sounding_dimensions;
    u8  maximum_ampdu_length_exponent;
} __attribute__((packed));

// struct morse_cmd_resp_get_capabilities
struct GetCapabilitiesResp {
    Capabilities capabilities;
    u8           morse_mmss_offset;
} __attribute__((packed));

// TWT_AGREEMENT_*

constexpr auto twt_agreement_max_len = usize(20); // MORSE_CMD_DOT11_TWT_AGREEMENT_MAX_LEN

// struct morse_cmd_req_twt_agreement_install, morse_cmd_req_twt_agreement_validate
struct TwtAgreementReq {
    u8 flow_id;
    u8 agreement_len;
    u8 agreement[twt_agreement_max_len];
} __attribute__((packed));

// struct morse_cmd_req_twt_agreement_remove
struct TwtAgreementRemoveReq {
    u8 flow_id;
} __attribute__((packed));

// MPSW_CONFIG

// MORSE_CMD_SET_MPSW_CFG_*, which config fields to apply
struct MpswSetCfg {
    enum : u8 {
        AirtimeBounds   = 1 << 0,
        PktSpcWinLen    = 1 << 1,
        Enabled         = 1 << 2,
    };
};

// struct morse_cmd_mpsw_configuration
struct MpswConfig {
    u32 airtime_max_us;
    u32 airtime_min_us;
    u32 packet_space_window_length_us;
    u8  enable;
} __attribute__((packed));

// struct morse_cmd_req_mpsw_config
struct MpswConfigReq {
    MpswConfig config;
    u8         set_cfgs; // MpswSetCfg
} __attribute__((packed));

// struct morse_cmd_resp_mpsw_config
struct MpswConfigResp {
    MpswConfig config;
} __attribute__((packed));

// INSTALL_KEY / DISABLE_KEY

constexpr auto max_key_len = usize(32); // MORSE_CMD_MAX_KEY_LEN

// enum morse_cmd_key_cipher
struct KeyCipher {
    enum : u8 {
        Invalid = 0,
        AesCcm  = 1,
        AesGcm  = 2,
        AesCmac = 3,
        AesGmac = 4,
    };
};

// enum morse_cmd_aes_key_len
struct AesKeyLen {
    enum : u8 {
        Invalid = 0,
        Bits128 = 1,
        Bits256 = 2,
    };
};

// enum morse_cmd_temporal_key_type
struct KeyType {
    enum : u8 {
        Invalid = 0,
        Gtk     = 1,
        Ptk     = 2,
        Igtk    = 3,
    };
};

// struct morse_cmd_req_install_key
struct InstallKeyReq {
    u64 pn;
    u32 aid;
    u8  key_idx;
    u8  cipher;     // KeyCipher
    u8  key_length; // AesKeyLen
    u8  key_type;   // KeyType
    u8  padding[2];
    u8  key[max_key_len];
} __attribute__((packed));

// struct morse_cmd_resp_install_key
struct InstallKeyResp {
    u8 key_idx;
} __attribute__((packed));

// struct morse_cmd_req_disable_key
struct DisableKeyReq {
    u32 key_type; // KeyType
    u32 aid;
    u8  key_idx;
} __attribute__((packed));

// STANDBY_MODE

constexpr auto standby_status_payload_max_len = usize(64); // MORSE_CMD_STANDBY_STATUS_FRAME_USER_PAYLOAD_MAX_LEN
constexpr auto standby_wake_filter_max_len    = usize(64); // MORSE_CMD_STANDBY_WAKE_FRAME_USER_FILTER_MAX_LEN

// enum morse_cmd_standby_mode
struct StandbyMode {
    enum : u32 {
        Exit             = 0,
        Enter            = 1,
        SetConfigV1      = 2,
        SetStatusPayload = 3,
        SetWakeFilter    = 4,
        SetConfigV2      = 5,
        SetConfigV3      = 6,
        SetConfigV4      = 7,
    };
};

// enum morse_cmd_standby_mode_exit_reason
struct StandbyExitReason {
    enum : u8 {
        None                = 0,
        WakeupFrame         = 1,
        Associate           = 2,
        ExtInput            = 3,
        WhitelistPkt        = 4,
        TcpConnectionLost   = 5,
        HwScanNotEnabled    = 6,
        HwScanFailedToStart = 7,
    };
};

// struct morse_cmd_standby_set_config
struct StandbySetConfig {
    u32 notify_period_s;
    u32 bss_inactivity_before_deep_sleep_s;
    u32 deep_sleep_period_s;
    u32 src_ip;
    u32 dst_ip;
    u16 dst_port;
    u8  pad[1];
    u8  disassoc_on_wake;
    u32 deep_sleep_increment_s;
    u32 deep_sleep_max_s;
    u32 deep_sleep_scan_iterations;
} __attribute__((packed));

// struct morse_cmd_standby_set_status_payload
struct StandbySetStatusPayload {
    u32 len;
    u8  payload[standby_status_payload_max_len];
} __attribute__((packed));

// struct morse_cmd_standby_enter
struct StandbyEnter {
    MacAddr monitor_bssid;
    u8      is_umac_controlled;
} __attribute__((packed));

// struct morse_cmd_standby_set_wake_filter
struct StandbySetWakeFilter {
    u32 len;
    u32 offset;
    u8  filter[standby_wake_filter_max_len];
} __attribute__((packed));

// struct morse_cmd_standby_mode_exit
struct StandbyExit {
    u8 reason;    // StandbyExitReason
    u8 sta_state; // StaState
} __attribute__((packed));

// struct morse_cmd_req_standby_mode
struct StandbyModeReq {
    u32 cmd; // StandbyMode
    union {
        StandbySetConfig        config;
        StandbySetStatusPayload set_payload;
        StandbyEnter            enter;
        StandbySetWakeFilter    set_filter;
    };
} __attribute__((packed));

// struct morse_cmd_resp_standby_mode
struct StandbyModeResp {
    StandbyExit info;
} __attribute__((packed));

// DHCP_OFFLOAD

// enum morse_cmd_dhcp_opcode
struct DhcpOpcode {
    enum : u32 {
        Enable          = 0,
        DoDiscovery     = 1,
        GetLease        = 2,
        ClearLease      = 3,
        RenewLease      = 4,
        RebindLease     = 5,
        SendLeaseUpdate = 6,
    };
};

// enum morse_cmd_dhcp_retcode
struct DhcpRetcode {
    enum : u32 {
        Success        = 0,
        NotEnabled     = 1,
        AlreadyEnabled = 2,
        NoLease        = 3,
        HaveLease      = 4,
        Busy           = 5,
        BadVif         = 6,
    };
};

// struct morse_cmd_req_dhcp_offload
struct DhcpOffloadReq {
    u32 opcode; // DhcpOpcode
} __attribute__((packed));

// struct morse_cmd_resp_dhcp_offload
struct DhcpOffloadResp {
    u32 retcode; // DhcpRetcode
    u32 my_ip;
    u32 netmask;
    u32 router;
    u32 dns;
} __attribute__((packed));

// UPDATE_OUI_FILTER

constexpr auto max_oui_filters = usize(5); // MORSE_CMD_MAX_OUI_FILTERS
constexpr auto oui_size        = usize(3); // MORSE_CMD_OUI_SIZE

// struct morse_cmd_req_update_oui_filter
struct UpdateOuiFilterReq {
    u8 n_ouis;
    u8 ouis[max_oui_filters][oui_size];
} __attribute__((packed));

// HW_SCAN

// MORSE_CMD_HW_SCAN_FLAGS_*
struct HwScanFlag {
    enum : u32 {
        Start             = 1 << 0,
        Abort             = 1 << 1,
        Survey            = 1 << 2,
        Store             = 1 << 3,
        Probes1Mhz        = 1 << 4,
        SchedStart        = 1 << 5,
        SchedStop         = 1 << 6,
        ProbeOnDozeBeacon = 1 << 7,
    };
};

// enum morse_cmd_hw_scan_tlv_tag
struct HwScanTlvTag {
    enum : u16 {
        Pad         = 0,
        ProbeReq    = 1,
        ChanList    = 2,
        PowerList   = 3,
        DwellOnHome = 4,
        Sched       = 5,
        Filter      = 6,
        SchedParams = 7,
    };
};

// struct morse_cmd_hw_scan_tlv; the value follows
struct HwScanTlvHeader {
    u16 tag; // HwScanTlvTag
    u16 len;
} __attribute__((packed));

// struct morse_cmd_req_hw_scan; tlvs follow
struct HwScanReq {
    u32 flags; // HwScanFlag
    u32 dwell_time_ms;
} __attribute__((packed));

// SET_WHITELIST

// MORSE_CMD_WHITELIST_FLAGS_*
struct WhitelistFlag {
    enum : u8 {
        Clear = 1 << 0,
    };
};

// struct morse_cmd_req_set_whitelist
struct SetWhitelistReq {
    u8  flags; // WhitelistFlag
    u8  ip_protocol;
    u16 llc_protocol;
    u32 src_ip;
    u32 dest_ip;
    u32 netmask;
    u16 src_port;
    u16 dest_port;
} __attribute__((packed));

// ARP_PERIODIC_REFRESH

// struct morse_cmd_arp_periodic_params / morse_cmd_req_arp_periodic_refresh
struct ArpPeriodicRefreshReq {
    u32 refresh_period_s;
    u32 destination_ip;
    u8  send_as_garp;
} __attribute__((packed));

// SET_TCP_KEEPALIVE

// MORSE_CMD_TCP_KEEPALIVE_SET_CFG_*, which config fields to apply
struct TcpKeepaliveSetCfg {
    enum : u8 {
        Period        = 1 << 0,
        RetryCount    = 1 << 1,
        RetryInterval = 1 << 2,
        SrcIpAddr     = 1 << 3,
        DestIpAddr    = 1 << 4,
        SrcPort       = 1 << 5,
        DestPort      = 1 << 6,
    };
};

// struct morse_cmd_req_set_tcp_keepalive
struct SetTcpKeepaliveReq {
    u8  enabled;
    u8  retry_count;
    u8  retry_interval_s;
    u8  set_cfgs; // TcpKeepaliveSetCfg
    u32 src_ip;
    u32 dest_ip;
    u16 src_port;
    u16 dest_port;
    u16 period_s;
} __attribute__((packed));

// LI_SLEEP

// struct morse_cmd_req_li_sleep
struct LiSleepReq {
    u32 listen_interval;
} __attribute__((packed));

// SEQUENCE_NUMBER_SPACES

constexpr auto sns_max_tids = usize(16); // MORSE_CMD_SNS_MAX_TIDS

// enum morse_cmd_sns_flag
struct SnsFlag {
    enum : u32 {
        Set                = 1 << 0,
        Baseline           = 1 << 1,
        IndivAddrQosData   = 1 << 2,
        QosNull            = 1 << 3,
    };
};

// struct morse_cmd_sequence_number_spaces
struct SequenceNumberSpaces {
    u16 baseline;
    u16 individually_addr_qos_data[sns_max_tids];
    u16 qos_null;
} __attribute__((packed));

// struct morse_cmd_req_sequence_number_spaces
struct SequenceNumberSpacesReq {
    u32                  flags; // SnsFlag
    MacAddr              addr;
    SequenceNumberSpaces spaces;
} __attribute__((packed));

// struct morse_cmd_resp_sequence_number_spaces
struct SequenceNumberSpacesResp {
    u32                  flags; // SnsFlag
    SequenceNumberSpaces spaces;
} __attribute__((packed));

// SET_CQM_RSSI

// struct morse_cmd_req_set_cqm_rssi
struct SetCqmRssiReq {
    i32 threshold;
    u32 hysteresis;
} __attribute__((packed));

// SET_CONTROL_RESPONSE

// struct morse_cmd_req_set_control_response
struct SetControlResponseReq {
    u8 direction;
    u8 control_response_1mhz_en;
} __attribute__((packed));

// events

// struct morse_cmd_evt_beacon_loss
struct BeaconLossEvent {
    u32 num_bcns;
} __attribute__((packed));

// MORSE_CMD_UMAC_TRAFFIC_CONTROL_SOURCE_*
struct TrafficControlSource {
    enum : u32 {
        Twt       = 1 << 0,
        DutyCycle = 1 << 1,
    };
};

// struct morse_cmd_evt_umac_traffic_control
struct UmacTrafficControlEvent {
    u8  pause_data_traffic;
    u32 sources; // TrafficControlSource
} __attribute__((packed));

// struct morse_cmd_evt_dhcp_lease_update
struct DhcpLeaseUpdateEvent {
    u32 my_ip;
    u32 netmask;
    u32 router;
    u32 dns;
} __attribute__((packed));

// struct morse_cmd_evt_hw_scan_done
struct HwScanDoneEvent {
    u8 aborted;
} __attribute__((packed));

// enum morse_cmd_connection_loss_reason
struct ConnectionLossReason {
    enum : u32 {
        TsfReset = 0,
    };
};

// struct morse_cmd_evt_connection_loss
struct ConnectionLossEvent {
    u32 reason; // ConnectionLossReason
} __attribute__((packed));

// enum morse_cmd_cqm_rssi_threshold_event
struct CqmRssiThreshold {
    enum : u16 {
        Low  = 0,
        High = 1,
    };
};

// struct morse_cmd_evt_cqm_rssi_notify
struct CqmRssiNotifyEvent {
    i16 rssi;
    u16 event; // CqmRssiThreshold
} __attribute__((packed));

// SET_RESPONSE_INDICATION

// struct morse_cmd_req_set_response_indication
struct SetResponseIndicationReq {
    i8 response_indication;
} __attribute__((packed));

// SET_TRANSMISSION_RATE

// struct morse_cmd_req_set_transmission_rate
struct SetTransmissionRateReq {
    i32 mcs_index;
    i32 bandwidth_mhz;
    i32 tx_80211ah_format;
    i8  use_traveling_pilots;
    i8  use_sgi;
    u8  enabled;
    i8  nss_idx;
    i8  use_ldpc;
    i8  use_stbc;
} __attribute__((packed));

// SET_NDP_PROBE_SUPPORT

// struct morse_cmd_req_set_ndp_probe_support
struct SetNdpProbeSupportReq {
    u8 enabled;
    u8 requested_response_is_pv1;
    i8 tx_bw_mhz;
} __attribute__((packed));

// FORCE_ASSERT

// enum morse_cmd_hart_id
struct HartId {
    enum : u32 {
        Host = 0,
        Mac  = 1,
        Uphy = 2,
        Lphy = 3,
    };
};

// struct morse_cmd_req_force_assert
struct ForceAssertReq {
    u32 hart_id; // HartId
    u32 delay;
} __attribute__((packed));

// GET_SET_GENERIC_PARAM

// MORSE_CMD_HOST_BLOCK_*, values for ParamId::TxBlock / HostTxBlock
struct HostBlock {
    enum : u32 {
        TxFrames = 1 << 0,
        TxCmd    = 1 << 1,
    };
};

// enum morse_cmd_param_action
struct ParamAction {
    enum : u32 {
        Set = 0,
        Get = 1,
    };
};

// enum morse_cmd_slow_clock_mode
struct SlowClockMode {
    enum : u32 {
        Auto     = 0,
        Internal = 1,
    };
};

// enum morse_cmd_param_id
struct ParamId {
    enum : u32 {
        MaxTrafficDeliveryWaitUs  = 0,
        ExtraAckTimeoutAdjustUs   = 1,
        TxStatusFlushWatermark    = 2,
        TxStatusFlushMinAmpduSize = 3,
        PowersaveType             = 4,
        SnoozeDurationAdjustUs    = 5,
        TxBlock                   = 6,
        ForcedSnoozePeriodUs      = 7,
        WakeActionGpio            = 8,
        WakeActionGpioPulseMs     = 9,
        ConnectionMonitorGpio     = 10,
        InputTriggerGpio          = 11,
        InputTriggerMode          = 12,
        Country                   = 13,
        RtsThreshold              = 14,
        HostTxBlock               = 15,
        MemRetentionCode          = 16,
        NonTimMode                = 17,
        DynamicPsTimeoutMs        = 18,
        HomeChannelDwellMs        = 19,
        SlowClockMode             = 20,
        FragmentThreshold         = 21,
        BeaconLossCount           = 22,
        ApPowerSave               = 23,
        BeaconOffload             = 24,
        ProbeRespOffload          = 25,
        BssMaxAwayDuration        = 26,
        DefaultActiveScanDwellMs  = 27,
        CtsToSelf                 = 28,
        Channelization            = 29,
        CryptoInHost              = 30,
        Autoconnect               = 31,
        HostPwrOffGpio            = 32,
        HostPwrOffGpioPulseMs     = 33,
    };
};

// struct morse_cmd_req_get_set_generic_param
struct GenericParamReq {
    u32 param_id; // ParamId
    u32 action;   // ParamAction
    u32 flags;
    u32 value;
} __attribute__((packed));

// struct morse_cmd_resp_get_set_generic_param
struct GenericParamResp {
    u32 flags;
    u32 value;
} __attribute__((packed));

// TURBO_MODE

// struct morse_cmd_req_turbo_mode
struct TurboModeReq {
    u32 aid;
    u8  enabled;
} __attribute__((packed));

// iot firmware extras

// struct morse_cmd_req_iot_configure_interop
struct IotConfigureInteropReq {
    u8 disable_op_class_checking;
    u8 enable_channel_width_workaround;
} __attribute__((packed));

// struct morse_cmd_req_iot_send_addba
struct IotSendAddbaReq {
    MacAddr mac_addr;
    u8      tid;
} __attribute__((packed));
} // namespace halow
