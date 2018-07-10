//
//  low_scan.c
//  WLAN Linux
//
//  Created by Carlos Duclos on 7/7/18.
//

#include "low_scan.h"

/************************ CONSTANTS & MACROS ************************/

/*
 * Constants fof WE-9->15
 */
#define IW15_MAX_FREQUENCIES    16
#define IW15_MAX_BITRATES    8
#define IW15_MAX_TXPOWER    8
#define IW15_MAX_ENCODING_SIZES    8
#define IW15_MAX_SPY        8
#define IW15_MAX_AP        8

/****************************** TYPES ******************************/

/*
 *    Struct iw_range up to WE-15
 */
struct    iw15_range
{
    __u32        throughput;
    __u32        min_nwid;
    __u32        max_nwid;
    __u16        num_channels;
    __u8        num_frequency;
    struct iw_freq    freq[IW15_MAX_FREQUENCIES];
    __s32        sensitivity;
    struct iw_quality    max_qual;
    __u8        num_bitrates;
    __s32        bitrate[IW15_MAX_BITRATES];
    __s32        min_rts;
    __s32        max_rts;
    __s32        min_frag;
    __s32        max_frag;
    __s32        min_pmp;
    __s32        max_pmp;
    __s32        min_pmt;
    __s32        max_pmt;
    __u16        pmp_flags;
    __u16        pmt_flags;
    __u16        pm_capa;
    __u16        encoding_size[IW15_MAX_ENCODING_SIZES];
    __u8        num_encoding_sizes;
    __u8        max_encoding_tokens;
    __u16        txpower_capa;
    __u8        num_txpower;
    __s32        txpower[IW15_MAX_TXPOWER];
    __u8        we_version_compiled;
    __u8        we_version_source;
    __u16        retry_capa;
    __u16        retry_flags;
    __u16        r_time_flags;
    __s32        min_retry;
    __s32        max_retry;
    __s32        min_r_time;
    __s32        max_r_time;
    struct iw_quality    avg_qual;
};

/*
 * Union for all the versions of iwrange.
 * Fortunately, I mostly only add fields at the end, and big-bang
 * reorganisations are few.
 */
union    iw_range_raw
{
    struct iw15_range    range15;    /* WE 9->15 */
    struct iw_range        range;        /* WE 16->current */
};

/*
 * Offsets in iw_range struct
 */
#define iwr15_off(f)    ( ((char *) &(((struct iw15_range *) NULL)->f)) - \
(char *) NULL)
#define iwr_off(f)    ( ((char *) &(((struct iw_range *) NULL)->f)) - \
(char *) NULL)

/**************************** VARIABLES ****************************/

/* Modes as human readable strings */
const char * const iw_operation_mode[] = { "Auto",
    "Ad-Hoc",
    "Managed",
    "Master",
    "Repeater",
    "Secondary",
    "Monitor",
    "Unknown/bug" };

/* Modulations as human readable strings */
const struct iw_modul_descr iw_modul_list[] = {
    /* Start with aggregate types, so that they display first */
    { IW_MODUL_11AG, "11ag",
        "IEEE 802.11a + 802.11g (2.4 & 5 GHz, up to 54 Mb/s)" },
    { IW_MODUL_11AB, "11ab",
        "IEEE 802.11a + 802.11b (2.4 & 5 GHz, up to 54 Mb/s)" },
    { IW_MODUL_11G, "11g", "IEEE 802.11g (2.4 GHz, up to 54 Mb/s)" },
    { IW_MODUL_11A, "11a", "IEEE 802.11a (5 GHz, up to 54 Mb/s)" },
    { IW_MODUL_11B, "11b", "IEEE 802.11b (2.4 GHz, up to 11 Mb/s)" },
    
    /* Proprietary aggregates */
    { IW_MODUL_TURBO | IW_MODUL_11A, "turboa",
        "Atheros turbo mode at 5 GHz (up to 108 Mb/s)" },
    { IW_MODUL_TURBO | IW_MODUL_11G, "turbog",
        "Atheros turbo mode at 2.4 GHz (up to 108 Mb/s)" },
    { IW_MODUL_PBCC | IW_MODUL_11B, "11+",
        "TI 802.11+ (2.4 GHz, up to 22 Mb/s)" },
    
    /* Individual modulations */
    { IW_MODUL_OFDM_G, "OFDMg",
        "802.11g higher rates, OFDM at 2.4 GHz (up to 54 Mb/s)" },
    { IW_MODUL_OFDM_A, "OFDMa", "802.11a, OFDM at 5 GHz (up to 54 Mb/s)" },
    { IW_MODUL_CCK, "CCK", "802.11b higher rates (2.4 GHz, up to 11 Mb/s)" },
    { IW_MODUL_DS, "DS", "802.11 Direct Sequence (2.4 GHz, up to 2 Mb/s)" },
    { IW_MODUL_FH, "FH", "802.11 Frequency Hopping (2,4 GHz, up to 2 Mb/s)" },
    
    /* Proprietary modulations */
    { IW_MODUL_TURBO, "turbo",
        "Atheros turbo mode, channel bonding (up to 108 Mb/s)" },
    { IW_MODUL_PBCC, "PBCC",
        "TI 802.11+ higher rates (2.4 GHz, up to 22 Mb/s)" },
    { IW_MODUL_CUSTOM, "custom",
        "Driver specific modulation (check driver documentation)" },
};

/* Disable runtime version warning in iw_get_range_info() */
int    iw_ignore_version = 0;

/************************ EVENT SUBROUTINES ************************/
/*
 * The Wireless Extension API 14 and greater define Wireless Events,
 * that are used for various events and scanning.
 * Those functions help the decoding of events, so are needed only in
 * this case.
 */

/* -------------------------- CONSTANTS -------------------------- */

/* Type of headers we know about (basically union iwreq_data) */
#define IW_HEADER_TYPE_NULL    0    /* Not available */
#define IW_HEADER_TYPE_CHAR    2    /* char [IFNAMSIZ] */
#define IW_HEADER_TYPE_UINT    4    /* __u32 */
#define IW_HEADER_TYPE_FREQ    5    /* struct iw_freq */
#define IW_HEADER_TYPE_ADDR    6    /* struct sockaddr */
#define IW_HEADER_TYPE_POINT    8    /* struct iw_point */
#define IW_HEADER_TYPE_PARAM    9    /* struct iw_param */
#define IW_HEADER_TYPE_QUAL    10    /* struct iw_quality */

/* Handling flags */
/* Most are not implemented. I just use them as a reminder of some
 * cool features we might need one day ;-) */
#define IW_DESCR_FLAG_NONE    0x0000    /* Obvious */
/* Wrapper level flags */
#define IW_DESCR_FLAG_DUMP    0x0001    /* Not part of the dump command */
#define IW_DESCR_FLAG_EVENT    0x0002    /* Generate an event on SET */
#define IW_DESCR_FLAG_RESTRICT    0x0004    /* GET : request is ROOT only */
/* SET : Omit payload from generated iwevent */
#define IW_DESCR_FLAG_NOMAX    0x0008    /* GET : no limit on request size */
/* Driver level flags */
#define IW_DESCR_FLAG_WAIT    0x0100    /* Wait for driver event */

/* ---------------------------- TYPES ---------------------------- */

/*
 * Describe how a standard IOCTL looks like.
 */
struct iw_ioctl_description
{
    __u8    header_type;        /* NULL, iw_point or other */
    __u8    token_type;        /* Future */
    __u16    token_size;        /* Granularity of payload */
    __u16    min_tokens;        /* Min acceptable token number */
    __u16    max_tokens;        /* Max acceptable token number */
    __u32    flags;            /* Special handling of the request */
};

/* -------------------------- VARIABLES -------------------------- */

/*
 * Meta-data about all the standard Wireless Extension request we
 * know about.
 */
static const struct iw_ioctl_description standard_ioctl_descr[] = {
    [SIOCSIWCOMMIT    - SIOCIWFIRST] = {
        .header_type    = IW_HEADER_TYPE_NULL,
    },
    [SIOCGIWNAME    - SIOCIWFIRST] = {
        .header_type    = IW_HEADER_TYPE_CHAR,
        .flags        = IW_DESCR_FLAG_DUMP,
    },
    [SIOCSIWNWID    - SIOCIWFIRST] = {
        .header_type    = IW_HEADER_TYPE_PARAM,
        .flags        = IW_DESCR_FLAG_EVENT,
    },
    [SIOCGIWNWID    - SIOCIWFIRST] = {
        .header_type    = IW_HEADER_TYPE_PARAM,
        .flags        = IW_DESCR_FLAG_DUMP,
    },
    [SIOCSIWFREQ    - SIOCIWFIRST] = {
        .header_type    = IW_HEADER_TYPE_FREQ,
        .flags        = IW_DESCR_FLAG_EVENT,
    },
    [SIOCGIWFREQ    - SIOCIWFIRST] = {
        .header_type    = IW_HEADER_TYPE_FREQ,
        .flags        = IW_DESCR_FLAG_DUMP,
    },
    [SIOCSIWMODE    - SIOCIWFIRST] = {
        .header_type    = IW_HEADER_TYPE_UINT,
        .flags        = IW_DESCR_FLAG_EVENT,
    },
    [SIOCGIWMODE    - SIOCIWFIRST] = {
        .header_type    = IW_HEADER_TYPE_UINT,
        .flags        = IW_DESCR_FLAG_DUMP,
    },
    [SIOCSIWSENS    - SIOCIWFIRST] = {
        .header_type    = IW_HEADER_TYPE_PARAM,
    },
    [SIOCGIWSENS    - SIOCIWFIRST] = {
        .header_type    = IW_HEADER_TYPE_PARAM,
    },
    [SIOCSIWRANGE    - SIOCIWFIRST] = {
        .header_type    = IW_HEADER_TYPE_NULL,
    },
    [SIOCGIWRANGE    - SIOCIWFIRST] = {
        .header_type    = IW_HEADER_TYPE_POINT,
        .token_size    = 1,
        .max_tokens    = sizeof(struct iw_range),
        .flags        = IW_DESCR_FLAG_DUMP,
    },
    [SIOCSIWPRIV    - SIOCIWFIRST] = {
        .header_type    = IW_HEADER_TYPE_NULL,
    },
    [SIOCGIWPRIV    - SIOCIWFIRST] = { /* (handled directly by us) */
        .header_type    = IW_HEADER_TYPE_NULL,
    },
    [SIOCSIWSTATS    - SIOCIWFIRST] = {
        .header_type    = IW_HEADER_TYPE_NULL,
    },
    [SIOCGIWSTATS    - SIOCIWFIRST] = { /* (handled directly by us) */
        .header_type    = IW_HEADER_TYPE_NULL,
        .flags        = IW_DESCR_FLAG_DUMP,
    },
    [SIOCSIWSPY    - SIOCIWFIRST] = {
        .header_type    = IW_HEADER_TYPE_POINT,
        .token_size    = sizeof(struct sockaddr),
        .max_tokens    = IW_MAX_SPY,
    },
    [SIOCGIWSPY    - SIOCIWFIRST] = {
        .header_type    = IW_HEADER_TYPE_POINT,
        .token_size    = sizeof(struct sockaddr) +
        sizeof(struct iw_quality),
        .max_tokens    = IW_MAX_SPY,
    },
    [SIOCSIWTHRSPY    - SIOCIWFIRST] = {
        .header_type    = IW_HEADER_TYPE_POINT,
        .token_size    = sizeof(struct iw_thrspy),
        .min_tokens    = 1,
        .max_tokens    = 1,
    },
    [SIOCGIWTHRSPY    - SIOCIWFIRST] = {
        .header_type    = IW_HEADER_TYPE_POINT,
        .token_size    = sizeof(struct iw_thrspy),
        .min_tokens    = 1,
        .max_tokens    = 1,
    },
    [SIOCSIWAP    - SIOCIWFIRST] = {
        .header_type    = IW_HEADER_TYPE_ADDR,
    },
    [SIOCGIWAP    - SIOCIWFIRST] = {
        .header_type    = IW_HEADER_TYPE_ADDR,
        .flags        = IW_DESCR_FLAG_DUMP,
    },
    [SIOCSIWMLME    - SIOCIWFIRST] = {
        .header_type    = IW_HEADER_TYPE_POINT,
        .token_size    = 1,
        .min_tokens    = sizeof(struct iw_mlme),
        .max_tokens    = sizeof(struct iw_mlme),
    },
    [SIOCGIWAPLIST    - SIOCIWFIRST] = {
        .header_type    = IW_HEADER_TYPE_POINT,
        .token_size    = sizeof(struct sockaddr) +
        sizeof(struct iw_quality),
        .max_tokens    = IW_MAX_AP,
        .flags        = IW_DESCR_FLAG_NOMAX,
    },
    [SIOCSIWSCAN    - SIOCIWFIRST] = {
        .header_type    = IW_HEADER_TYPE_POINT,
        .token_size    = 1,
        .min_tokens    = 0,
        .max_tokens    = sizeof(struct iw_scan_req),
    },
    [SIOCGIWSCAN    - SIOCIWFIRST] = {
        .header_type    = IW_HEADER_TYPE_POINT,
        .token_size    = 1,
        .max_tokens    = IW_SCAN_MAX_DATA,
        .flags        = IW_DESCR_FLAG_NOMAX,
    },
    [SIOCSIWESSID    - SIOCIWFIRST] = {
        .header_type    = IW_HEADER_TYPE_POINT,
        .token_size    = 1,
        .max_tokens    = IW_ESSID_MAX_SIZE + 1,
        .flags        = IW_DESCR_FLAG_EVENT,
    },
    [SIOCGIWESSID    - SIOCIWFIRST] = {
        .header_type    = IW_HEADER_TYPE_POINT,
        .token_size    = 1,
        .max_tokens    = IW_ESSID_MAX_SIZE + 1,
        .flags        = IW_DESCR_FLAG_DUMP,
    },
    [SIOCSIWNICKN    - SIOCIWFIRST] = {
        .header_type    = IW_HEADER_TYPE_POINT,
        .token_size    = 1,
        .max_tokens    = IW_ESSID_MAX_SIZE + 1,
    },
    [SIOCGIWNICKN    - SIOCIWFIRST] = {
        .header_type    = IW_HEADER_TYPE_POINT,
        .token_size    = 1,
        .max_tokens    = IW_ESSID_MAX_SIZE + 1,
    },
    [SIOCSIWRATE    - SIOCIWFIRST] = {
        .header_type    = IW_HEADER_TYPE_PARAM,
    },
    [SIOCGIWRATE    - SIOCIWFIRST] = {
        .header_type    = IW_HEADER_TYPE_PARAM,
    },
    [SIOCSIWRTS    - SIOCIWFIRST] = {
        .header_type    = IW_HEADER_TYPE_PARAM,
    },
    [SIOCGIWRTS    - SIOCIWFIRST] = {
        .header_type    = IW_HEADER_TYPE_PARAM,
    },
    [SIOCSIWFRAG    - SIOCIWFIRST] = {
        .header_type    = IW_HEADER_TYPE_PARAM,
    },
    [SIOCGIWFRAG    - SIOCIWFIRST] = {
        .header_type    = IW_HEADER_TYPE_PARAM,
    },
    [SIOCSIWTXPOW    - SIOCIWFIRST] = {
        .header_type    = IW_HEADER_TYPE_PARAM,
    },
    [SIOCGIWTXPOW    - SIOCIWFIRST] = {
        .header_type    = IW_HEADER_TYPE_PARAM,
    },
    [SIOCSIWRETRY    - SIOCIWFIRST] = {
        .header_type    = IW_HEADER_TYPE_PARAM,
    },
    [SIOCGIWRETRY    - SIOCIWFIRST] = {
        .header_type    = IW_HEADER_TYPE_PARAM,
    },
    [SIOCSIWENCODE    - SIOCIWFIRST] = {
        .header_type    = IW_HEADER_TYPE_POINT,
        .token_size    = 1,
        .max_tokens    = IW_ENCODING_TOKEN_MAX,
        .flags        = IW_DESCR_FLAG_EVENT | IW_DESCR_FLAG_RESTRICT,
    },
    [SIOCGIWENCODE    - SIOCIWFIRST] = {
        .header_type    = IW_HEADER_TYPE_POINT,
        .token_size    = 1,
        .max_tokens    = IW_ENCODING_TOKEN_MAX,
        .flags        = IW_DESCR_FLAG_DUMP | IW_DESCR_FLAG_RESTRICT,
    },
    [SIOCSIWPOWER    - SIOCIWFIRST] = {
        .header_type    = IW_HEADER_TYPE_PARAM,
    },
    [SIOCGIWPOWER    - SIOCIWFIRST] = {
        .header_type    = IW_HEADER_TYPE_PARAM,
    },
    [SIOCSIWMODUL    - SIOCIWFIRST] = {
        .header_type    = IW_HEADER_TYPE_PARAM,
    },
    [SIOCGIWMODUL    - SIOCIWFIRST] = {
        .header_type    = IW_HEADER_TYPE_PARAM,
    },
    [SIOCSIWGENIE    - SIOCIWFIRST] = {
        .header_type    = IW_HEADER_TYPE_POINT,
        .token_size    = 1,
        .max_tokens    = IW_GENERIC_IE_MAX,
    },
    [SIOCGIWGENIE    - SIOCIWFIRST] = {
        .header_type    = IW_HEADER_TYPE_POINT,
        .token_size    = 1,
        .max_tokens    = IW_GENERIC_IE_MAX,
    },
    [SIOCSIWAUTH    - SIOCIWFIRST] = {
        .header_type    = IW_HEADER_TYPE_PARAM,
    },
    [SIOCGIWAUTH    - SIOCIWFIRST] = {
        .header_type    = IW_HEADER_TYPE_PARAM,
    },
    [SIOCSIWENCODEEXT - SIOCIWFIRST] = {
        .header_type    = IW_HEADER_TYPE_POINT,
        .token_size    = 1,
        .min_tokens    = sizeof(struct iw_encode_ext),
        .max_tokens    = sizeof(struct iw_encode_ext) +
        IW_ENCODING_TOKEN_MAX,
    },
    [SIOCGIWENCODEEXT - SIOCIWFIRST] = {
        .header_type    = IW_HEADER_TYPE_POINT,
        .token_size    = 1,
        .min_tokens    = sizeof(struct iw_encode_ext),
        .max_tokens    = sizeof(struct iw_encode_ext) +
        IW_ENCODING_TOKEN_MAX,
    },
    [SIOCSIWPMKSA - SIOCIWFIRST] = {
        .header_type    = IW_HEADER_TYPE_POINT,
        .token_size    = 1,
        .min_tokens    = sizeof(struct iw_pmksa),
        .max_tokens    = sizeof(struct iw_pmksa),
    },
};
static const unsigned int standard_ioctl_num = (sizeof(standard_ioctl_descr) /
                                                sizeof(struct iw_ioctl_description));

/*
 * Meta-data about all the additional standard Wireless Extension events
 * we know about.
 */
static const struct iw_ioctl_description standard_event_descr[] = {
    [IWEVTXDROP    - IWEVFIRST] = {
        .header_type    = IW_HEADER_TYPE_ADDR,
    },
    [IWEVQUAL    - IWEVFIRST] = {
        .header_type    = IW_HEADER_TYPE_QUAL,
    },
    [IWEVCUSTOM    - IWEVFIRST] = {
        .header_type    = IW_HEADER_TYPE_POINT,
        .token_size    = 1,
        .max_tokens    = IW_CUSTOM_MAX,
    },
    [IWEVREGISTERED    - IWEVFIRST] = {
        .header_type    = IW_HEADER_TYPE_ADDR,
    },
    [IWEVEXPIRED    - IWEVFIRST] = {
        .header_type    = IW_HEADER_TYPE_ADDR,
    },
    [IWEVGENIE    - IWEVFIRST] = {
        .header_type    = IW_HEADER_TYPE_POINT,
        .token_size    = 1,
        .max_tokens    = IW_GENERIC_IE_MAX,
    },
    [IWEVMICHAELMICFAILURE    - IWEVFIRST] = {
        .header_type    = IW_HEADER_TYPE_POINT,
        .token_size    = 1,
        .max_tokens    = sizeof(struct iw_michaelmicfailure),
    },
    [IWEVASSOCREQIE    - IWEVFIRST] = {
        .header_type    = IW_HEADER_TYPE_POINT,
        .token_size    = 1,
        .max_tokens    = IW_GENERIC_IE_MAX,
    },
    [IWEVASSOCRESPIE    - IWEVFIRST] = {
        .header_type    = IW_HEADER_TYPE_POINT,
        .token_size    = 1,
        .max_tokens    = IW_GENERIC_IE_MAX,
    },
    [IWEVPMKIDCAND    - IWEVFIRST] = {
        .header_type    = IW_HEADER_TYPE_POINT,
        .token_size    = 1,
        .max_tokens    = sizeof(struct iw_pmkid_cand),
    },
};
static const unsigned int standard_event_num = (sizeof(standard_event_descr) /
                                                sizeof(struct iw_ioctl_description));

/* Size (in bytes) of various events */
static const int event_type_size[] = {
    IW_EV_LCP_PK_LEN,    /* IW_HEADER_TYPE_NULL */
    0,
    IW_EV_CHAR_PK_LEN,    /* IW_HEADER_TYPE_CHAR */
    0,
    IW_EV_UINT_PK_LEN,    /* IW_HEADER_TYPE_UINT */
    IW_EV_FREQ_PK_LEN,    /* IW_HEADER_TYPE_FREQ */
    IW_EV_ADDR_PK_LEN,    /* IW_HEADER_TYPE_ADDR */
    0,
    IW_EV_POINT_PK_LEN,    /* Without variable payload */
    IW_EV_PARAM_PK_LEN,    /* IW_HEADER_TYPE_PARAM */
    IW_EV_QUAL_PK_LEN,    /* IW_HEADER_TYPE_QUAL */
};

/*------------------------------------------------------------------*/
/*
 * Initialise the struct stream_descr so that we can extract
 * individual events from the event stream.
 */
void iw_init_event_stream(struct stream_descr *stream, /* Stream of events */
                          char *data,
                          int len) {
    /* Cleanup */
    memset((char *) stream, '\0', sizeof(struct stream_descr));
    
    /* Set things up */
    stream->current = data;
    stream->end = data + len;
}

/*------------------------------------------------------------------*/
/*
 * Process/store one element from the scanning results in wireless_scan
 */
static inline struct wireless_scan *iw_process_scanning_token(struct iw_event *event,
                                                              struct wireless_scan *wscan) {
    struct wireless_scan *oldwscan;
    
    /* Now, let's decode the event */
    switch(event->cmd) {
        case SIOCGIWAP:
            /* New cell description. Allocate new cell descriptor, zero it. */
            oldwscan = wscan;
            wscan = (struct wireless_scan *) malloc(sizeof(struct wireless_scan));
            if (wscan == NULL)
                return(wscan);
            /* Link at the end of the list */
            if (oldwscan != NULL)
                oldwscan->next = wscan;
        
            /* Reset it */
            bzero(wscan, sizeof(struct wireless_scan));
        
            /* Save cell identifier */
            wscan->has_ap_addr = 1;
            memcpy(&(wscan->ap_addr), &(event->u.ap_addr), sizeof (sockaddr));
            break;
        
        case SIOCGIWNWID:
            wscan->b.has_nwid = 1;
            memcpy(&(wscan->b.nwid), &(event->u.nwid), sizeof(iwparam));
            break;
        
        case SIOCGIWFREQ:
            wscan->b.has_freq = 1;
            wscan->b.freq = iw_freq2float(&(event->u.freq));
            wscan->b.freq_flags = event->u.freq.flags;
            break;
        
        case SIOCGIWMODE:
            wscan->b.mode = event->u.mode;
            if((wscan->b.mode < IW_NUM_OPER_MODE) && (wscan->b.mode >= 0))
            wscan->b.has_mode = 1;
            break;
        
        case SIOCGIWESSID:
            wscan->b.has_essid = 1;
            wscan->b.essid_on = event->u.data.flags;
            memset(wscan->b.essid, '\0', IW_ESSID_MAX_SIZE+1);
            if((event->u.essid.pointer) && (event->u.essid.length))
            memcpy(wscan->b.essid, event->u.essid.pointer, event->u.essid.length);
            break;
        
        case SIOCGIWENCODE:
            wscan->b.has_key = 1;
            wscan->b.key_size = event->u.data.length;
            wscan->b.key_flags = event->u.data.flags;
            if(event->u.data.pointer)
            memcpy(wscan->b.key, event->u.essid.pointer, event->u.data.length);
            else
            wscan->b.key_flags |= IW_ENCODE_NOKEY;
            break;
        case IWEVQUAL:
            /* We don't get complete stats, only qual */
            wscan->has_stats = 1;
            memcpy(&wscan->stats.qual, &event->u.qual, sizeof(struct iw_quality));
            break;
        case SIOCGIWRATE:
            /* Scan may return a list of bitrates. As we have space for only
             * a single bitrate, we only keep the largest one. */
            if((!wscan->has_maxbitrate) ||
               (event->u.bitrate.value > wscan->maxbitrate.value))
            {
                wscan->has_maxbitrate = 1;
                memcpy(&(wscan->maxbitrate), &(event->u.bitrate), sizeof(iwparam));
            }
        case IWEVCUSTOM:
            /* How can we deal with those sanely ? Jean II */
            default:
            break;
    }    /* switch(event->cmd) */
    
    return(wscan);
}

/*------------------------------------------------------------------*/
/*
 * Extract the next event from the event stream.
 */
int
iw_extract_event_stream(struct stream_descr *    stream,    /* Stream of events */
                        struct iw_event *    iwe,    /* Extracted event */
                        int            we_version)
{
    const struct iw_ioctl_description *    descr = NULL;
    int        event_type = 0;
    unsigned int    event_len = 1;        /* Invalid */
    char *    pointer;
    /* Don't "optimise" the following variable, it will crash */
    unsigned    cmd_index;        /* *MUST* be unsigned */
    
    /* Check for end of stream */
    if((stream->current + IW_EV_LCP_PK_LEN) > stream->end)
    return(0);
    
#ifdef DEBUG
    printf("DBG - stream->current = %p, stream->value = %p, stream->end = %p\n",
           stream->current, stream->value, stream->end);
#endif
    
    /* Extract the event header (to get the event id).
     * Note : the event may be unaligned, therefore copy... */
    memcpy((char *) iwe, stream->current, IW_EV_LCP_PK_LEN);
    
#ifdef DEBUG
    printf("DBG - iwe->cmd = 0x%X, iwe->len = %d\n",
           iwe->cmd, iwe->len);
#endif
    
    /* Check invalid events */
    if(iwe->len <= IW_EV_LCP_PK_LEN)
    return(-1);
    
    /* Get the type and length of that event */
    if(iwe->cmd <= SIOCIWLAST)
    {
        cmd_index = iwe->cmd - SIOCIWFIRST;
        if(cmd_index < standard_ioctl_num)
        descr = &(standard_ioctl_descr[cmd_index]);
    }
    else
    {
        cmd_index = iwe->cmd - IWEVFIRST;
        if(cmd_index < standard_event_num)
        descr = &(standard_event_descr[cmd_index]);
    }
    if(descr != NULL)
    event_type = descr->header_type;
    /* Unknown events -> event_type=0 => IW_EV_LCP_PK_LEN */
    event_len = event_type_size[event_type];
    /* Fixup for earlier version of WE */
    if((we_version <= 18) && (event_type == IW_HEADER_TYPE_POINT))
    event_len += IW_EV_POINT_OFF;
    
    /* Check if we know about this event */
    if(event_len <= IW_EV_LCP_PK_LEN)
    {
        /* Skip to next event */
        stream->current += iwe->len;
        return(2);
    }
    event_len -= IW_EV_LCP_PK_LEN;
    
    /* Set pointer on data */
    if(stream->value != NULL)
    pointer = stream->value;            /* Next value in event */
    else
    pointer = stream->current + IW_EV_LCP_PK_LEN;    /* First value in event */
    
#ifdef DEBUG
    printf("DBG - event_type = %d, event_len = %d, pointer = %p\n",
           event_type, event_len, pointer);
#endif
    
    /* Copy the rest of the event (at least, fixed part) */
    if((pointer + event_len) > stream->end)
    {
        /* Go to next event */
        stream->current += iwe->len;
        return(-2);
    }
    /* Fixup for WE-19 and later : pointer no longer in the stream */
    /* Beware of alignement. Dest has local alignement, not packed */
    if((we_version > 18) && (event_type == IW_HEADER_TYPE_POINT))
    memcpy((char *) iwe + IW_EV_LCP_LEN + IW_EV_POINT_OFF,
           pointer, event_len);
    else
    memcpy((char *) iwe + IW_EV_LCP_LEN, pointer, event_len);
    
    /* Skip event in the stream */
    pointer += event_len;
    
    /* Special processing for iw_point events */
    if(event_type == IW_HEADER_TYPE_POINT)
    {
        /* Check the length of the payload */
        unsigned int    extra_len = iwe->len - (event_len + IW_EV_LCP_PK_LEN);
        if(extra_len > 0)
        {
            /* Set pointer on variable part (warning : non aligned) */
            iwe->u.data.pointer = pointer;
            
            /* Check that we have a descriptor for the command */
            if(descr == NULL)
            /* Can't check payload -> unsafe... */
            iwe->u.data.pointer = NULL;    /* Discard paylod */
            else
            {
                /* Those checks are actually pretty hard to trigger,
                 * because of the checks done in the kernel... */
                
                unsigned int    token_len = iwe->u.data.length * descr->token_size;
                
                /* Ugly fixup for alignement issues.
                 * If the kernel is 64 bits and userspace 32 bits,
                 * we have an extra 4+4 bytes.
                 * Fixing that in the kernel would break 64 bits userspace. */
                if((token_len != extra_len) && (extra_len >= 4))
                {
                    __u16        alt_dlen = *((__u16 *) pointer);
                    unsigned int    alt_token_len = alt_dlen * descr->token_size;
                    if((alt_token_len + 8) == extra_len)
                    {
#ifdef DEBUG
                        printf("DBG - alt_token_len = %d\n", alt_token_len);
#endif
                        /* Ok, let's redo everything */
                        pointer -= event_len;
                        pointer += 4;
                        /* Dest has local alignement, not packed */
                        memcpy((char *) iwe + IW_EV_LCP_LEN + IW_EV_POINT_OFF,
                               pointer, event_len);
                        pointer += event_len + 4;
                        iwe->u.data.pointer = pointer;
                        token_len = alt_token_len;
                    }
                }
                
                /* Discard bogus events which advertise more tokens than
                 * what they carry... */
                if(token_len > extra_len)
                iwe->u.data.pointer = NULL;    /* Discard paylod */
                /* Check that the advertised token size is not going to
                 * produce buffer overflow to our caller... */
                if((iwe->u.data.length > descr->max_tokens)
                   && !(descr->flags & IW_DESCR_FLAG_NOMAX))
                iwe->u.data.pointer = NULL;    /* Discard paylod */
                /* Same for underflows... */
                if(iwe->u.data.length < descr->min_tokens)
                iwe->u.data.pointer = NULL;    /* Discard paylod */
#ifdef DEBUG
                printf("DBG - extra_len = %d, token_len = %d, token = %d, max = %d, min = %d\n",
                       extra_len, token_len, iwe->u.data.length, descr->max_tokens, descr->min_tokens);
#endif
            }
        }
        else
        /* No data */
        iwe->u.data.pointer = NULL;
        
        /* Go to next event */
        stream->current += iwe->len;
    }
    else
    {
        /* Ugly fixup for alignement issues.
         * If the kernel is 64 bits and userspace 32 bits,
         * we have an extra 4 bytes.
         * Fixing that in the kernel would break 64 bits userspace. */
        if((stream->value == NULL)
           && ((((iwe->len - IW_EV_LCP_PK_LEN) % event_len) == 4)
               || ((iwe->len == 12) && ((event_type == IW_HEADER_TYPE_UINT) ||
                                        (event_type == IW_HEADER_TYPE_QUAL))) ))
        {
#ifdef DEBUG
            printf("DBG - alt iwe->len = %d\n", iwe->len - 4);
#endif
            pointer -= event_len;
            pointer += 4;
            /* Beware of alignement. Dest has local alignement, not packed */
            memcpy((char *) iwe + IW_EV_LCP_LEN, pointer, event_len);
            pointer += event_len;
        }
        
        /* Is there more value in the event ? */
        if((pointer + event_len) <= (stream->current + iwe->len))
        /* Go to next value */
        stream->value = pointer;
        else
        {
            /* Go to next event */
            stream->value = NULL;
            stream->current += iwe->len;
        }
    }
    return(1);
}

/********************** FREQUENCY SUBROUTINES ***********************/
/*
 * Note : the two functions below are the cause of troubles on
 * various embeeded platforms, as they are the reason we require
 * libm (math library).
 * In this case, please use enable BUILD_NOLIBM in the makefile
 *
 * FIXME : check negative mantissa and exponent
 */

/*------------------------------------------------------------------*/
/*
 * Convert a floating point the our internal representation of
 * frequencies.
 * The kernel doesn't want to hear about floating point, so we use
 * this custom format instead.
 */
void
iw_float2freq(double    in,
              iwfreq *    out)
{
#ifdef WE_NOLIBM
    /* Version without libm : slower */
    out->e = 0;
    while(in > 1e9)
    {
        in /= 10;
        out->e++;
    }
    out->m = (long) in;
#else    /* WE_NOLIBM */
    /* Version with libm : faster */
    out->e = (short) (floor(log10(in)));
    if(out->e > 8)
    {
        out->m = ((long) (floor(in / pow(10,out->e - 6)))) * 100;
        out->e -= 8;
    }
    else
    {
        out->m = (long) in;
        out->e = 0;
    }
#endif    /* WE_NOLIBM */
}

/*------------------------------------------------------------------*/
/*
 * Convert our internal representation of frequencies to a floating point.
 */
double
iw_freq2float(const iwfreq *    in)
{
#ifdef WE_NOLIBM
    /* Version without libm : slower */
    int        i;
    double    res = (double) in->m;
    for(i = 0; i < in->e; i++)
    res *= 10;
    return(res);
#else    /* WE_NOLIBM */
    /* Version with libm : faster */
    return ((double) in->m) * pow(10,in->e);
#endif    /* WE_NOLIBM */
}

/*------------------------------------------------------------------*/
/*
 * Initiate the scan procedure, and process results.
 * This is a non-blocking procedure and it will return each time
 * it would block, returning the amount of time the caller should wait
 * before calling again.
 * Return -1 for error, delay to wait for (in ms), or 0 for success.
 * Error code is in errno
 */
int iw_process_scan(int skfd,
                    char *ifname,
                    int we_version,
                    wireless_scan_head *context) {
    struct iwreq wrq;
    unsigned char *buffer = NULL;        /* Results */
    int buflen = IW_SCAN_MAX_DATA; /* Min for compat WE<17 */
    unsigned char *newbuf;

    /* Don't waste too much time on interfaces (150 * 100 = 15s) */
    context->retry++;
    if (context->retry > 150) {
        errno = ETIME;
        return(-1);
    }
    
    printf("1\n");

    /* If we have not yet initiated scanning on the interface */
    if (context->retry == 1) {
        
        /* Initiate Scan */
        wrq.u.data.pointer = NULL; /* Later */
        wrq.u.data.flags = 0;
        wrq.u.data.length = 0;
        
        /* Remember that as non-root, we will get an EPERM here */
        if((iw_set_ext(skfd, ifname, SIOCSIWSCAN, &wrq) < 0) && (errno != EPERM))
            return(-1);
        
        /* Success: now, just wait for event or results */
        return(250); /* Wait 250 ms */
    }
    
    printf("2\n");

realloc:
    /* (Re)allocate the buffer - realloc(NULL, len) == malloc(len) */
    newbuf = realloc(buffer, buflen);
    if (newbuf == NULL) {
        /* man says : If realloc() fails the original block is left untouched */
        if (buffer)
            free(buffer);

        errno = ENOMEM;
        return(-1);
    }
    buffer = newbuf;
    
    printf("3\n");

    /* Try to read the results */
    wrq.u.data.pointer = buffer;
    wrq.u.data.flags = 0;
    wrq.u.data.length = buflen;
    if (iw_get_ext(skfd, ifname, SIOCGIWSCAN, &wrq) < 0) {
        /* Check if buffer was too small (WE-17 only) */
        if ((errno == E2BIG) && (we_version > 16)) {
            /* Some driver may return very large scan results, either
             * because there are many cells, or because they have many
             * large elements in cells (like IWEVCUSTOM). Most will
             * only need the regular sized buffer. We now use a dynamic
             * allocation of the buffer to satisfy everybody. Of course,
             * as we don't know in advance the size of the array, we try
             * various increasing sizes. Jean II */

            /* Check if the driver gave us any hints. */
            if(wrq.u.data.length > buflen)
                buflen = wrq.u.data.length;
            else
                buflen *= 2;

            /* Try again */
            goto realloc;
        }
        
        printf("4\n");

        /* Check if results not available yet */
        if(errno == EAGAIN) {
            free(buffer);
            /* Wait for only 100ms from now on */
            return(100);    /* Wait 100 ms */
        }

        free(buffer);
        printf("5\n");
        /* Bad error, please don't come back... */
        return(-1);
    }

    /* We have the results, process them */
    if(wrq.u.data.length) {
        struct iw_event iwe;
        struct stream_descr stream;
        struct wireless_scan *wscan = NULL;
        int ret;
        
#ifdef DEBUG
        /* Debugging code. In theory useless, because it's debugged ;-) */
        int i;
        printf("Scan result [%02X", buffer[0]);
        for(i = 1; i < wrq.u.data.length; i++)
            printf(":%02X", buffer[i]);
        printf("]\n");
#endif

        /* Init */
        iw_init_event_stream(&stream, (char *) buffer, wrq.u.data.length);
        /* This is dangerous, we may leak user data... */
        context->result = NULL;

        /* Look every token */
        do {
            /* Extract an event and print it */
            ret = iw_extract_event_stream(&stream, &iwe, we_version);
            if(ret > 0) {
                /* Convert to wireless_scan struct */
                wscan = iw_process_scanning_token(&iwe, wscan);
                /* Check problems */
                if(wscan == NULL) {
                    free(buffer);
                    errno = ENOMEM;
                    return(-1);
                }
                /* Save head of list */
                if(context->result == NULL)
                    context->result = wscan;
            }
        } while(ret > 0);
    }

    /* Done with this interface - return success */
    free(buffer);
    return(0);
}

/*------------------------------------------------------------------*/
/*
 * Get the range information out of the driver
 */
int iw_get_range_info(int skfd, const char *ifname, iwrange *range) {
    struct iwreq wrq;
    char buffer[sizeof(iwrange) * 2];    /* Large enough */
    union iw_range_raw *range_raw;
    
    /* Cleanup */
    bzero(buffer, sizeof(buffer));
    
    wrq.u.data.pointer = (caddr_t) buffer;
    wrq.u.data.length = sizeof(buffer);
    wrq.u.data.flags = 0;
    if (iw_get_ext(skfd, ifname, SIOCGIWRANGE, &wrq) < 0)
        return(-1);
    
    /* Point to the buffer */
    range_raw = (union iw_range_raw *) buffer;
    
    /* For new versions, we can check the version directly, for old versions
     * we use magic. 300 bytes is a also magic number, don't touch... */
    if (wrq.u.data.length < 300) {
        /* That's v10 or earlier. Ouch ! Let's make a guess...*/
        range_raw->range.we_version_compiled = 9;
    }
    
    /* Check how it needs to be processed */
    if (range_raw->range.we_version_compiled > 15) {
        /* This is our native format, that's easy... */
        /* Copy stuff at the right place, ignore extra */
        memcpy((char *) range, buffer, sizeof(iwrange));
    } else {
        /* Zero unknown fields */
        bzero((char *) range, sizeof(struct iw_range));
        
        /* Initial part unmoved */
        memcpy((char *) range,
               buffer,
               iwr15_off(num_channels));
        /* Frequencies pushed futher down towards the end */
        memcpy((char *) range + iwr_off(num_channels),
               buffer + iwr15_off(num_channels),
               iwr15_off(sensitivity) - iwr15_off(num_channels));
        /* This one moved up */
        memcpy((char *) range + iwr_off(sensitivity),
               buffer + iwr15_off(sensitivity),
               iwr15_off(num_bitrates) - iwr15_off(sensitivity));
        /* This one goes after avg_qual */
        memcpy((char *) range + iwr_off(num_bitrates),
               buffer + iwr15_off(num_bitrates),
               iwr15_off(min_rts) - iwr15_off(num_bitrates));
        /* Number of bitrates has changed, put it after */
        memcpy((char *) range + iwr_off(min_rts),
               buffer + iwr15_off(min_rts),
               iwr15_off(txpower_capa) - iwr15_off(min_rts));
        /* Added encoding_login_index, put it after */
        memcpy((char *) range + iwr_off(txpower_capa),
               buffer + iwr15_off(txpower_capa),
               iwr15_off(txpower) - iwr15_off(txpower_capa));
        /* Hum... That's an unexpected glitch. Bummer. */
        memcpy((char *) range + iwr_off(txpower),
               buffer + iwr15_off(txpower),
               iwr15_off(avg_qual) - iwr15_off(txpower));
        /* Avg qual moved up next to max_qual */
        memcpy((char *) range + iwr_off(avg_qual),
               buffer + iwr15_off(avg_qual),
               sizeof(struct iw_quality));
    }
    
    /* We are now checking much less than we used to do, because we can
     * accomodate more WE version. But, there are still cases where things
     * will break... */
    if (!iw_ignore_version) {
        /* We don't like very old version (unfortunately kernel 2.2.X) */
        if(range->we_version_compiled <= 10) {
            fprintf(stderr, "Warning: Driver for device %s has been compiled with an ancient version\n", ifname);
            fprintf(stderr, "of Wireless Extension, while this program support version 11 and later.\n");
            fprintf(stderr, "Some things may be broken...\n\n");
        }
        
        /* We don't like future versions of WE, because we can't cope with
         * the unknown */
        if(range->we_version_compiled > WE_MAX_VERSION) {
            fprintf(stderr, "Warning: Driver for device %s has been compiled with version %d\n", ifname, range->we_version_compiled);
            fprintf(stderr, "of Wireless Extension, while this program supports up to version %d.\n", WE_MAX_VERSION);
            fprintf(stderr, "Some things may be broken...\n\n");
        }
        
        /* Driver version verification */
        if((range->we_version_compiled > 10) && (range->we_version_compiled < range->we_version_source)) {
            fprintf(stderr, "Warning: Driver for device %s recommend version %d of Wireless Extension,\n", ifname, range->we_version_source);
            fprintf(stderr, "but has been compiled with version %d, therefore some driver features\n", range->we_version_compiled);
            fprintf(stderr, "may not be available...\n\n");
        }
        /* Note : we are only trying to catch compile difference, not source.
         * If the driver source has not been updated to the latest, it doesn't
         * matter because the new fields are set to zero */
    }
    
    /* Don't complain twice.
     * In theory, the test apply to each individual driver, but usually
     * all drivers are compiled from the same kernel. */
    iw_ignore_version = 1;
    
    return(0);
}

int iw_sockets_open(void) {

    static const int families[] = {
        AF_INET, AF_IPX, AF_AX25, AF_APPLETALK
    };
    unsigned int i;
    int sock;

    /* Try all families we support */
    for (i = 0; i < sizeof(families)/sizeof(int); ++i) {
        /* Try to open the socket, if success returns it */
        sock = socket(families[i], SOCK_DGRAM, 0);
        if(sock >= 0)
            return sock;
    }

    return -1;
}

/*------------------------------------------------------------------*/
/*
 * Perform a wireless scan on the specified interface.
 * This is a blocking procedure and it will when the scan is completed
 * or when an error occur.
 *
 * The scan results are given in a linked list of wireless_scan objects.
 * The caller *must* free the result himself (by walking the list).
 * If there is an error, -1 is returned and the error code is available
 * in errno.
 *
 * The parameter we_version can be extracted from the range structure
 * (range.we_version_compiled - see iw_get_range_info()), or using
 * iw_get_kernel_we_version(). For performance reason, you should
 * cache this parameter when possible rather than querying it every time.
 *
 * Return -1 for error and 0 for success.
 */
int iw_scan(int skfd,
            char *ifname,
            int we_version,
            wireless_scan_head *context) {
    
    int delay; /* in ms */
    
    /* Clean up context. Potential memory leak if(context.result != NULL) */
    context->result = NULL;
    context->retry = 0;
    
    /* Wait until we get results or error */
    while (1) {
        /* Try to get scan results */
        delay = iw_process_scan(skfd, ifname, we_version, context);
        
        /* Check termination */
        if (delay <= 0)
            break;
        
        /* Wait a bit */
        usleep(delay * 1000);
    }
    
    /* End - return -1 or 0 */
    return(delay);
}

int main(void) {
    wireless_scan_head head;
    wireless_scan *result;
    iwrange range;
    int sock;
    
    /* Open socket to kernel */
    sock = iw_sockets_open();
    
    /* Get some metadata to use for scanning */
    if (iw_get_range_info(sock, "wlan0", &range) < 0) {
        printf("Error during iw_get_range_info. Aborting.\n");
        exit(2);
    }

    /* Perform the scan */
    if (iw_scan(sock, "wlan0", range.we_version_compiled, &head) < 0) {
        printf("Error during iw_scan. Aborting.\n");
        exit(2);
    }
    
    printf("traverse\n");
    /* Traverse the results */
    result = head.result;
    
    while (NULL != result) {
        printf("%s\n", result->b.essid);
        result = result->next;
    }
    
    exit(0);
}

