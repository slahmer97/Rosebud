#include "core.h"
#include "packet_headers.h"

#if 0
#define PROFILE_A(x) do {DEBUG_OUT_L = (x);} while (0)
#define PROFILE_B(x) do {DEBUG_OUT_H = (x);} while (0)
#else
#define PROFILE_A(x) do {} while (0)
#define PROFILE_B(x) do {} while (0)
#endif

#define bswap_16(x) \
  (((x & 0xff00) >> 8) | ((x & 0x00ff) << 8))

#define bswap_32(x) \
  (((x & 0xff000000) >> 24) | ((x & 0x00ff0000) >>  8) | \
   ((x & 0x0000ff00) <<  8) | ((x & 0x000000ff) << 24))

// maximum number of slots (number of context objects)
#define MAX_CTX_COUNT 16

// maximum number of accelerators (number of accelerator context objects)
#define MAX_ACCEL_COUNT 16

// maximum number of accelerator groups (number of accelerator group objects)
#define MAX_ACCEL_GROUP_COUNT 8

// Maximum number of bytes to be checked by the fixed_sme engine
#define MAX_FIXED_SME_RULE 20

#define HASH_LB

// packet start offset
// DWORD align Ethernet payload
// provide space for header modifications
#define PKT_OFFSET 10
// PMEM in 8 blocks of 128 KB
// accelerators only connected to upper 2 blocks
#define PKTS_START (6*128*1024)

#ifdef HASH_LB
  #define DATA_OFFSET 4
#else
  #define DATA_OFFSET 0
#endif

// flows
struct flow {
  unsigned int hash;
  unsigned int state;
  union {
    volatile unsigned long long state_64;
    volatile unsigned int       states_32[2];
  } sme_state;
};

// #define FLOW_TABLE_SIZE 32768
#define FLOW_TABLE_SIZE 1024

unsigned char *flow_table;

// SME accelerator control registers
#define ACC_SME_STATUS (*((volatile unsigned int *)(IO_EXT_BASE + 0x0400)))
#define ACC_SME_STATUS_DONE (*((volatile unsigned int *)(IO_EXT_BASE + 0x0404)))
#define ACC_SME_STATUS_MATCH (*((volatile unsigned int *)(IO_EXT_BASE + 0x0408)))
#define ACC_SME_STATUS_BUSY (*((volatile unsigned int *)(IO_EXT_BASE + 0x040C)))
#define ACC_SME_STATE_INIT (*((volatile unsigned long long *)(IO_EXT_BASE + 0x0410)))
#define ACC_SME_STATE_INIT_L (*((volatile unsigned int *)(IO_EXT_BASE + 0x0410)))
#define ACC_SME_STATE_INIT_H (*((volatile unsigned int *)(IO_EXT_BASE + 0x0414)))
#define ACC_SME_DESC_ERROR (*((volatile unsigned int *)(IO_EXT_BASE + 0x0418)))
#define ACC_SME_DONE_ERROR (*((volatile unsigned int *)(IO_EXT_BASE + 0x041C)))

#define ACC_IP_MATCH_CTL (*((volatile unsigned int *)(IO_EXT_BASE + 0x0800)))

struct sme_accel_regs {
  volatile unsigned int ctrl;
  volatile unsigned int len;
  volatile unsigned int start;
  volatile unsigned int match_1hot;
  union {
    volatile unsigned long long state_64;
    volatile unsigned int       states_32[2];
  } state;
};

// Slot contexts
struct slot_context {
  int index;
  struct Desc desc;
  unsigned char *packet;
  unsigned char *header;

  struct eth_header *eth_hdr;
  union {
    struct ipv4_header *ipv4_hdr;
  } l3_header;
  union {
    struct tcp_header *tcp_hdr;
    struct udp_header *udp_hdr;
  } l4_header;

  unsigned int payload_offset;

  struct flow *flow;

  unsigned int state;
  unsigned int fixed_match;
};

#define STATE_DROP 0
#define STATE_SEND 1
#define STATE_MATCH_TCP 2
#define STATE_MATCH_UDP 3
#define STATE_MATCH_HTTP 4

struct slot_context context[MAX_CTX_COUNT];

struct accel_group;
struct accel_context;

// Accelerator groups
struct accel_group {
  unsigned int accel_count;
  unsigned int accel_mask;
  unsigned int accel_active_mask;

  struct accel_group *list_next;

  struct accel_context *member_list_head;
  struct accel_context *member_list_tail;

  char slot_waiting_head;
  char slot_waiting_tail;
  char waiting;
  struct slot_context *slot_waiting_queue[MAX_CTX_COUNT];
};

struct accel_group accel_group[MAX_ACCEL_GROUP_COUNT];

// Accelerator contexts
struct accel_context {
  struct sme_accel_regs *sme_accel;

  unsigned int index;
  unsigned int mask;

  struct accel_group *group;

  struct accel_context *list_next;
  struct accel_context *group_list_next;

  struct slot_context *active_slot;
};

struct accel_context accel_context[MAX_ACCEL_COUNT];

#define ACCEL_GROUP_TCP_INDEX 0
#define ACCEL_GROUP_UDP_INDEX 1
#define ACCEL_GROUP_HTTP_INDEX 2
#define ACCEL_GROUP_FIXED_INDEX 3

#define ACCEL_GROUP_TCP accel_group[ACCEL_GROUP_TCP_INDEX]
#define ACCEL_GROUP_UDP accel_group[ACCEL_GROUP_UDP_INDEX]
#define ACCEL_GROUP_HTTP accel_group[ACCEL_GROUP_HTTP_INDEX]
#define ACCEL_GROUP_FIXED accel_group[ACCEL_GROUP_FIXED_INDEX]

struct accel_context *fixed_accel;

static inline void add_accel_to_group(struct accel_group *grp, struct accel_context *accel)
{
  // set group
  accel->group = grp;

  // append to linked list
  if (grp->member_list_head)
  {
    grp->member_list_head->group_list_next = accel;
  }
  else
  {
    grp->member_list_tail = accel;
  }

  grp->member_list_head = accel;

  grp->accel_mask |= accel->mask;
  grp->accel_count++;
}

unsigned int slot_count;
unsigned int slot_size;
unsigned int header_slot_base;
unsigned int header_slot_size;

unsigned int accel_count;
unsigned int accel_group_count;
unsigned int accel_waiting_mask;
unsigned int accel_active_mask;

static inline void reserve_accel(struct accel_group *grp, struct slot_context *slot)
{
  grp->slot_waiting_queue[(int)grp->slot_waiting_head] = slot;

  grp->slot_waiting_head++;
  if (grp->slot_waiting_head >= slot_count)
    grp->slot_waiting_head = 0;

  grp->waiting = 1;
}

static inline struct slot_context *accel_pop_slot(struct accel_group *grp)
{
  struct slot_context *slot = grp->slot_waiting_queue[(int)grp->slot_waiting_tail];

  grp->slot_waiting_tail++;
  if (grp->slot_waiting_tail >= slot_count)
    grp->slot_waiting_tail = 0;

  if (grp->slot_waiting_head == grp->slot_waiting_tail)
    grp->waiting = 0;

  return slot;
}

static inline void take_accel(struct accel_context *accel, struct slot_context *slot)
{
  accel_active_mask |= accel->mask;
  accel->group->accel_active_mask |= accel->mask;
  accel->active_slot = slot;
}

static inline void release_accel(struct accel_context *accel)
{
  accel_active_mask &= ~accel->mask;
  accel->group->accel_active_mask &= ~accel->mask;
  accel->active_slot = 0;
}

static inline void handle_slot_rx_packet(struct slot_context *slot);
static inline void handle_accel_start(struct slot_context *slot, struct accel_context *accel);
static inline void handle_accel_done(struct slot_context *slot, struct accel_context *accel);

static inline void call_accel(struct accel_group *grp, struct slot_context *slot)
{
  if (~grp->accel_active_mask & grp->accel_mask)
  {
    // group has idle accelerators
    for (struct accel_context *accel = grp->member_list_tail; accel; accel = accel->group_list_next)
    {
      if (!accel->active_slot)
      {
        // handle packet
        handle_accel_start(slot, accel);
        return;
      }
    }
  }

  // no idle accelerators
  reserve_accel(grp, slot);
}

static inline void handle_slot_rx_packet(struct slot_context *slot)
{
  char ch;
  unsigned int payload_offset = ETH_HEADER_SIZE + DATA_OFFSET;
  unsigned int packet_length = slot->desc.len;
  unsigned int payload_length;

  unsigned int flow_hash;
  struct flow* flow;

  unsigned int match;
  unsigned int mask;

  PROFILE_B(0x00010000);

  // check eth type
  PROFILE_B(0xF0010000 | slot->eth_hdr->type);
  if (slot->eth_hdr->type == bswap_16(0x0800))
  {
    PROFILE_B(0x00010001);
    // IPv4 packet
    slot->l3_header.ipv4_hdr = (struct ipv4_header*)(slot->header+payload_offset);

    // start IP check
    ACC_IP_MATCH_CTL = slot->l3_header.ipv4_hdr->src_ip;

    // check version
    ch = slot->l3_header.ipv4_hdr->version_ihl;
    if ((ch & 0xF0) != 0x40)
    {
      // invalid version, drop it
      PROFILE_B(0x00010009);
      goto drop;
    }

    // header size from IHL
    payload_offset += (ch & 0xf) << 2;
    slot->l4_header.tcp_hdr = (struct tcp_header*)(slot->header+payload_offset);

    // check IP
    if (ACC_IP_MATCH_CTL)
    {
      // it's a match, drop it
      PROFILE_B(0x0001000a);
      goto drop;
    }

    PROFILE_B(0x00010002);
    // check protocol
    switch (slot->l3_header.ipv4_hdr->protocol)
    {
      case 0x06:
        PROFILE_B(0x00010003);
        // TCP
        PROFILE_B(0xF0010000 | slot->l4_header.tcp_hdr->dest_port);

        // header size from flags field
        ch = ((char *)slot->l4_header.tcp_hdr)[12];
        payload_offset += (ch & 0xF0) >> 2;

        if (payload_offset >= packet_length)
          // no payload
          goto drop;

        payload_length = packet_length - payload_offset;
        slot->payload_offset = payload_offset;

        // start fixed SME
        fixed_accel->sme_accel->start = (unsigned int)(slot->desc.data)+payload_offset;
        fixed_accel->sme_accel->len = payload_length > MAX_FIXED_SME_RULE ? MAX_FIXED_SME_RULE : payload_length;
        fixed_accel->sme_accel->ctrl = 1;

        // unaligned read of flow hash
        flow_hash = *((unsigned short *)(slot->header)) | (*((unsigned short *)(slot->header+2)) << 16);

        flow = (struct flow *)(flow_table + (flow_hash & ((FLOW_TABLE_SIZE-1) << 4)));

        if (flow->hash == 0)
        {
          // inactive
          // set flow hash
          flow->hash = flow_hash;
          // init state
          flow->state = 0;
          // init SME state
          accel_context[0].sme_accel->state.state_64 = 0;
        }
        else if (flow->hash == flow_hash)
        {
          // match
          // reload SME state
          accel_context[0].sme_accel->state.state_64 = flow->sme_state.state_64;
        }
        else
        {
          // conflict, overwrite
          // set flow hash
          flow->hash = flow_hash;
          // init state
          flow->state = 0;
          // init SME state
          accel_context[0].sme_accel->state.state_64 = 0;
        }

        slot->flow = flow;

        if (slot->l4_header.tcp_hdr->src_port == bswap_16(80) || slot->l4_header.tcp_hdr->dest_port == bswap_16(80))
        {
          PROFILE_B(0x00010004);
          // HTTP

          // start SME
          call_accel(&ACCEL_GROUP_HTTP, slot);

          slot->state = STATE_MATCH_HTTP;

          // read match result from fixed SME
          match = fixed_accel->sme_accel->match_1hot;
          slot->fixed_match = match;

          // handle fixed SME rules

          // ET DOS Terse HTTP GET Likely LOIC
          // ET DOS Terse HTTP GET Likely AnonMafiaIC DDoS tool
          // ET DOS Terse HTTP GET Likely AnonGhost DDoS tool
          mask = (1 << 10) | (1 << 11) | (1 << 12);
          if (match & mask)
          {
            slot->state = STATE_SEND;
            return;
          }

          return;
        }
        else
        {
          PROFILE_B(0x00010005);
          // other TCP

          call_accel(&ACCEL_GROUP_TCP, slot);

          slot->state = STATE_MATCH_TCP;

          // read match result from fixed SME
          match = fixed_accel->sme_accel->match_1hot;
          slot->fixed_match = match;

          // handle fixed SME rules

          // ET DOS SMBLoris NBSS Length Mem Exhaustion Attempt (PoC Based)
          if (match & (1 << 24))
          {
            if (slot->l4_header.tcp_hdr->dest_port == bswap_16(139) ||
              slot->l4_header.tcp_hdr->dest_port == bswap_16(445))
            {
              slot->state = STATE_SEND;
              return;
            }
          }

          // ET DOS Excessive Large Tree Connect Response
          if (match & (1 << 27))
          {
            if (slot->l4_header.tcp_hdr->src_port == bswap_16(445))
            {
              slot->state = STATE_SEND;
              return;
            }
          }

          return;
        }
      case 0x11:
        PROFILE_B(0x00010006);
        // UDP
        payload_offset += UDP_HEADER_SIZE;

        if (payload_offset >= packet_length)
          // no payload
          goto drop;

        payload_length = packet_length - payload_offset;
        slot->payload_offset = payload_offset;

        // start fixed SME
        fixed_accel->sme_accel->start = (unsigned int)(slot->desc.data)+payload_offset;
        fixed_accel->sme_accel->len = payload_length > MAX_FIXED_SME_RULE ? MAX_FIXED_SME_RULE : payload_length;
        fixed_accel->sme_accel->ctrl = 1;

        slot->flow = 0;

        accel_context[0].sme_accel->state.state_64 = 0;

        // start SME
        call_accel(&ACCEL_GROUP_UDP, slot);

        slot->state = STATE_MATCH_UDP;

        // read match result from fixed SME
        match = fixed_accel->sme_accel->match_1hot;
        slot->fixed_match = match;

        // handle fixed SME rules

        // source port match
        if (slot->l4_header.udp_hdr->src_port == bswap_16(123))
        {
          if ((slot->l4_header.udp_hdr->dest_port & bswap_16(0xfc00)) == 0)
          {
            // ET DOS Likely NTP DDoS In Progress MON_LIST Response to Non-Ephemeral Port IMPL 0x02
            // ET DOS Likely NTP DDoS In Progress PEER_LIST Response to Non-Ephemeral Port IMPL 0x02
            // ET DOS Likely NTP DDoS In Progress PEER_LIST Response to Non-Ephemeral Port IMPL 0x03
            // ET DOS Likely NTP DDoS In Progress PEER_LIST_SUM Response to Non-Ephemeral Port IMPL 0x02
            // ET DOS Likely NTP DDoS In Progress PEER_LIST_SUM Response to Non-Ephemeral Port IMPL 0x03
            // ET DOS Likely NTP DDoS In Progress MON_LIST Response to Non-Ephemeral Port IMPL 0x03
            // ET DOS Likely NTP DDoS In Progress GET_RESTRICT Response to Non-Ephemeral Port IMPL 0x03
            // ET DOS Likely NTP DDoS In Progress GET_RESTRICT Response to Non-Ephemeral Port IMPL 0x02
            mask = (1 << 4) | (1 << 5) | (1 << 6) | (1 << 7) | (1 << 8) | (1 << 9) | (1 << 13) | (1 << 14);
            if (match & mask)
            {
              slot->state = STATE_SEND;
              return;
            }
          }

          // ET DOS Possible NTP DDoS Multiple MON_LIST Seq 0 Response Spanning Multiple Packets IMPL 0x02
          // ET DOS Possible NTP DDoS Multiple MON_LIST Seq 0 Response Spanning Multiple Packets IMPL 0x03
          // ET DOS Likely NTP DDoS In Progress Multiple UNSETTRAP Mode 6 Responses
          mask = (1 << 2) | (1 << 3) | (1 << 21);
          if (match & mask)
          {
            slot->state = STATE_SEND;
            return;
          }
        }
        else if (slot->l4_header.udp_hdr->src_port == bswap_16(5093))
        {
          // ET DOS Possible Sentinal LM  Application attack in progress Outbound (Response)
          // ET DOS Possible Sentinal LM Amplification attack (Response) Inbound
          mask = (1 << 22) | (1 << 23);
          if (match & mask)
          {
            slot->state = STATE_SEND;
            return;
          }
        }
        else if (slot->l4_header.udp_hdr->src_port == bswap_16(11211))
        {
          // ET DOS Possible Memcached DDoS Amplification Response Outbound
          // ET DOS Possible Memcached DDoS Amplification Inbound
          mask = (1 << 25) | (1 << 26);
          if (match & mask)
          {
            slot->state = STATE_SEND;
            return;
          }
        }

        // dest port match
        if (slot->l4_header.udp_hdr->dest_port == bswap_16(123))
        {
          // ET DOS Possible NTP DDoS Inbound Frequent Un-Authed MON_LIST Requests IMPL 0x02
          // ET DOS Possible NTP DDoS Inbound Frequent Un-Authed MON_LIST Requests IMPL 0x03
          // ET DOS Possible NTP DDoS Inbound Frequent Un-Authed PEER_LIST Requests IMPL 0x03
          // ET DOS Possible NTP DDoS Inbound Frequent Un-Authed PEER_LIST Requests IMPL 0x02
          // ET DOS Possible NTP DDoS Inbound Frequent Un-Authed PEER_LIST_SUM Requests IMPL 0x03
          // ET DOS Possible NTP DDoS Inbound Frequent Un-Authed PEER_LIST_SUM Requests IMPL 0x02
          // ET DOS Possible NTP DDoS Inbound Frequent Un-Authed GET_RESTRICT Requests IMPL 0x02
          // ET DOS Possible NTP DDoS Inbound Frequent Un-Authed GET_RESTRICT Requests IMPL 0x03
          mask = (1 << 0) | (1 << 1) | (1 << 15) | (1 << 16) | (1 << 17) | (1 << 18) | (1 << 19) | (1 << 20);
          if (match & mask)
          {
            slot->state = STATE_SEND;
            return;
          }
        }

        return;
    }
  }

  PROFILE_B(0x00010007);
drop:
  PROFILE_B(0x00010008);
  slot->desc.len = 0;
  pkt_send(&slot->desc);
}

static inline void handle_accel_start(struct slot_context *slot, struct accel_context *accel)
{
  // start SME
  accel->sme_accel->start = (unsigned int)(slot->desc.data)+slot->payload_offset;
  accel->sme_accel->len = slot->desc.len-slot->payload_offset;
  accel->sme_accel->ctrl = 1;

  // mark accelerator in use
  take_accel(accel, slot);
}

static inline void handle_accel_done(struct slot_context *slot, struct accel_context *accel)
{
  unsigned int match = accel->sme_accel->match_1hot;
  unsigned int mask;

  struct flow* flow = slot->flow;

  if (flow)
  {
    // save SME state
    flow->sme_state.state_64 = accel->sme_accel->state.state_64;

    if (flow->state == 1)
    {
      goto send;
    }
  }

  if (slot->state == STATE_SEND)
  {
    // unconditional send
    goto send;
  }

  if (slot->state == STATE_DROP)
  {
    // unconditional drop
    goto drop;
  }

  if (!match)
  {
    // no match; drop packet
    goto drop;
  }

  // handle SME rules

  switch (slot->state)
  {
    case STATE_MATCH_TCP:
      // TCP rules

      // source port match
      if (slot->l4_header.tcp_hdr->src_port == bswap_16(445))
      {
        // ET DOS SMB Tree_Connect Stack Overflow Attempt (CVE-2017-0016)
        mask = (1 << 0) | (1 << 5);
        if ((match & mask) == mask)
          goto send;
      }

      // dest port match
      if (slot->l4_header.tcp_hdr->dest_port == bswap_16(445))
      {
        // ET DOS Microsoft Windows LSASS Remote Memory Corruption (CVE-2017-0004)
        mask = (1 << 7) | (1 << 6) | (1 << 4) | (1 << 2);
        if ((match & mask) == mask)
          goto send;
      }

      // any port
      // ET DOS CVE-2013-0230 Miniupnpd SoapAction MethodName Buffer Overflow
      mask = (1 << 3) | (1 << 1);
      if ((match & mask) == mask)
        goto send;

      goto drop;
    case STATE_MATCH_HTTP:
      // HTTP rules

      mask = (1 << 12);
      if ((match & mask) == mask)
        goto send;

      mask = (1 << 17) | (1 << 20) | (1 << 22) | (1 << 15) | (1 << 28) | (1 << 19);
      if ((match & mask) == mask)
        goto send;

      mask = (1 << 28);
      if ((match & mask) == mask)
        goto send;

      mask = (1 << 26) | (1 << 23) | (1 << 29) | (1 << 1);
      if ((match & mask) == mask)
        goto send;

      mask = (1 << 17) | (1 << 4) | (1 << 2);
      if ((match & mask) == mask)
        goto send;

      mask = (1 << 26) | (1 << 7) | (1 << 9);
      if ((match & mask) == mask)
        goto send;

      mask = (1 << 17) | (1 << 5);
      if ((match & mask) == mask)
        goto send;

      mask = (1 << 27) | (1 << 13) | (1 << 8) | (1 << 10);
      if ((match & mask) == mask)
        goto send;

      mask = (1 << 24);
      if ((match & mask) == mask)
        goto send;

      mask = (1 << 25);
      if ((match & mask) == mask)
        goto send;

      mask = (1 << 31);
      if ((match & mask) == mask)
        goto send;

      mask = (1 << 3) | (1 << 21) | (1 << 16) | (1 << 14) | (1 << 11);
      if ((match & mask) == mask)
        goto send;

      mask = (1 << 6) | (1 << 30);
      if ((match & mask) == mask)
        goto send;

      // ET DOS Terse HTTP GET Likely GoodBye 5.2 DDoS tool
      mask = (1 << 0);
      if ((match & mask) == mask)
        goto send;

      // ET DOS HTTP GET AAAAAAAA Likely FireFlood
      mask = (1 << 18);
      if ((match & mask) == mask)
        goto send;

      goto drop;
    case STATE_MATCH_UDP:
      // UDP rules

      // source port match
      if (slot->l4_header.udp_hdr->src_port == bswap_16(53))
      {
        if ((slot->l4_header.udp_hdr->dest_port & bswap_16(0xfc00)) == 0)
        {
          // ET DOS DNS Amplification Attack Possible Outbound Windows Non-Recursive Root Hint Reserved Port
          // ET DOS DNS Amplification Attack Possible Inbound Windows Non-Recursive Root Hint Reserved Port
          mask = (1 << 20) | (1 << 4);
          if ((match & mask) == mask)
            goto send;
        }
      }
      else if (slot->l4_header.udp_hdr->src_port == bswap_16(389))
      {
        if (slot->l4_header.udp_hdr->dest_port == bswap_16(389))
        {
          // ET DOS CLDAP Amplification Reflection (PoC based)
          mask =  (1 << 7);
          if ((match & mask) == mask)
            goto send;
        }
      }
      else if (slot->l4_header.udp_hdr->src_port == bswap_16(1434))
      {
        // ET DOS MC-SQLR Response Outbound Possible DDoS Participation
        // ET DOS MC-SQLR Response Inbound Possible DDoS Target
        mask = (1 << 3) | (1 << 13) | (1 << 8) | (1 << 9) | (1 << 14);
        if ((match & mask) == mask)
          goto send;
      }

      // dest port match
      if (slot->l4_header.udp_hdr->dest_port == bswap_16(53))
      {
        // ET DOS DNS Amplification Attack Inbound
        mask = (1 << 2) | (1 << 1);
        if ((match & mask) == mask)
          goto send;
      }
      else if (slot->l4_header.udp_hdr->dest_port == bswap_16(389))
      {
        // ET DOS Potential CLDAP Amplification Reflection
        mask = (1 << 15);
        if ((match & mask) == mask)
          goto send;
      }
      else if (slot->l4_header.udp_hdr->dest_port == bswap_16(1900))
      {
        // ET DOS LibuPnP CVE-2012-5963 ST UDN Buffer Overflow
        mask = (1 << 6);
        if ((match & mask) == mask)
          goto send;

        // ET DOS Miniupnpd M-SEARCH Buffer Overflow CVE-2013-0229
        mask = (1 << 10);
        if ((match & mask) == mask)
          goto send;

        // ET DOS Possible SSDP Amplification Scan in Progress
        mask = (1 << 11) | (1 << 12);
        if ((match & mask) == mask)
          goto send;

        // ET DOS LibuPnP CVE-2012-5958 ST DeviceType Buffer Overflow
        mask = (1 << 6) | (1 << 16);
        if ((match & mask) == mask)
          goto send;

        // ET DOS LibuPnP CVE-2012-5964 ST URN ServiceType Buffer Overflow
        mask = (1 << 6) | (1 << 18);
        if ((match & mask) == mask)
          goto send;

        // ET DOS LibuPnP CVE-2012-5965 ST URN DeviceType Buffer Overflow
        mask = (1 << 6) | (1 << 17);
        if ((match & mask) == mask)
          goto send;

        // ET DOS LibuPnP CVE-2012-5961 ST UDN Buffer Overflow
        mask = (1 << 6) | (1 << 16);
        if ((match & mask) == mask)
          goto send;
      }
      else if (slot->l4_header.udp_hdr->dest_port == bswap_16(5093))
      {
        // ET DOS Possible Sentinal LM Amplification attack (Request) Inbound
        mask =  (1 << 19);
        if ((match & mask) == mask)
          goto send;
      }
      else if (slot->l4_header.udp_hdr->dest_port == bswap_16(11211))
      {
        // ET DOS Possible Memcached DDoS Amplification Query (set)
        mask =  (1 << 0) | (1 << 5);
        if ((match & mask) == mask)
          goto send;
      }

      goto drop;
  }

send:
  if (flow)
  {
    // mark flow
    flow->state = 1;
  }

  // reset accelerator
  accel->sme_accel->ctrl = 1<<4;

  // send packet to host
  slot->desc.port = 2;

  if (DATA_OFFSET) {
    // apply offset
    slot->desc.data = slot->packet + DATA_OFFSET;
    slot->desc.len = slot->desc.len - DATA_OFFSET;
  }

  pkt_send(&slot->desc);
  return;

drop:
  // reset accelerator
  accel->sme_accel->ctrl = 1<<4;

  // drop packet
  slot->desc.len = 0;

  pkt_send(&slot->desc);
}

int main(void)
{
  struct slot_context *slot;
  struct accel_context *accel;
  struct accel_group *grp;

  DEBUG_OUT_L = 0;
  DEBUG_OUT_H = 0;

  // set slot configuration parameters
  slot_count = 16;
  slot_size = 16*1024;
  header_slot_base = DMEM_BASE + (DMEM_SIZE >> 1);
  header_slot_size = 128;

  accel_count = 4+4+4+1;
  accel_group_count = 4;

  if (slot_count > MAX_SLOT_COUNT)
    slot_count = MAX_SLOT_COUNT;

  if (slot_count > MAX_CTX_COUNT)
    slot_count = MAX_CTX_COUNT;

  if (accel_count > MAX_ACCEL_COUNT)
    accel_count = MAX_ACCEL_COUNT;

  if (accel_group_count > MAX_ACCEL_GROUP_COUNT)
    accel_group_count = MAX_ACCEL_GROUP_COUNT;

  PROFILE_A(0x00000001);

  // Do this at the beginning, so LB can fill the slots while
  // initializing other things.
  init_hdr_slots(slot_count, header_slot_base, header_slot_size);
  init_slots(slot_count, PKTS_START+PKT_OFFSET, slot_size);
	set_masks(0x30); // Enable only Evict + Poke

  PROFILE_A(0x00000002);

  accel_waiting_mask = 0;
  accel_active_mask = 0;

  // init slot context structures
  for (int i = 0; i < slot_count; i++)
  {
    context[i].index = i;
    context[i].desc.tag = i+1;
    context[i].desc.data = (unsigned char *)(PMEM_BASE + PKTS_START + PKT_OFFSET + i*slot_size);
    context[i].packet = (unsigned char *)(PMEM_BASE + PKTS_START + PKT_OFFSET + i*slot_size);
    context[i].header = (unsigned char *)(header_slot_base + PKT_OFFSET + i*header_slot_size);

    context[i].eth_hdr = (struct eth_header*)(context[i].header + DATA_OFFSET);
  }

  PROFILE_A(0x00000003);

  // init accelerator group structures
  grp = 0;
  for (int i = 0; i < accel_group_count; i++)
  {
    accel_group[i].accel_count = 0;
    accel_group[i].accel_mask = 0;
    accel_group[i].accel_active_mask = 0;
    accel_group[i].slot_waiting_head = 0;
    accel_group[i].slot_waiting_tail = 0;

    if (grp)
    {
      grp->list_next = &accel_group[i];
    }

    grp = &accel_group[i];
  }

  PROFILE_A(0x00000004);

  // init accelerator context structures
  accel = 0;
  for (int i = 0; i < accel_count; i++)
  {
    accel_context[i].index = i;
    accel_context[i].mask = 1 << i;
    accel_context[i].sme_accel = (struct sme_accel_regs *)(IO_EXT_BASE + i*64);
    accel_context[i].group = 0;
    accel_context[i].list_next = 0;
    accel_context[i].group_list_next = 0;
    accel_context[i].active_slot = 0;

    if (accel)
    {
      accel->list_next = &accel_context[i];
    }

    accel = &accel_context[i];

    if (i < 4)
    {
      grp = &ACCEL_GROUP_TCP;
    }
    else if (i < 8)
    {
      grp = &ACCEL_GROUP_UDP;
    }
    else if (i < 12)
    {
      grp = &ACCEL_GROUP_HTTP;
    }
    else if (i < 13)
    {
      grp = &ACCEL_GROUP_FIXED;
      fixed_accel = accel;
    }
    else
    {
      continue;
    }

    add_accel_to_group(grp, &accel_context[i]);
  }

  // Reset accel state reg, meaning default start with no state
  accel_context[0].sme_accel->state.state_64 = 0;

  // init flow table
  flow_table = (unsigned char *)PMEM_BASE;

  for (int i = 0; i < FLOW_TABLE_SIZE; i++)
  {
    struct flow* flow = (struct flow *)(flow_table + (i << 4));

    flow->hash = 0;
  }

  PROFILE_A(0x00000005);

  while (1)
  {
    unsigned int temp;

    PROFILE_A(0x00010000);

    // check for new packets
    while (in_pkt_ready())
    {
      struct Desc desc;

      PROFILE_A(0x00010001);

      // read descriptor
      read_in_pkt(&desc);

      // compute index
      slot = &context[desc.tag-1];

      // copy descriptor into context
      slot->desc = desc;

      PROFILE_A(0x00010002);
      // handle packet
      handle_slot_rx_packet(slot);
      PROFILE_A(0x00010003);
    }

    PROFILE_A(0x00020000);

    // handle accelerator done
    temp = ACC_SME_STATUS_DONE & accel_active_mask;
    if (temp)
    {
      PROFILE_A(0x00020001);
      for (accel = &accel_context[0]; accel; accel = accel->list_next)
      {
        PROFILE_A(0x00020002);
        if (temp & accel->mask)
        {
          PROFILE_A(0x00020003);
          // handle packet
          handle_accel_done(accel->active_slot, accel);
          PROFILE_A(0x00020004);

          // release accelerator
          release_accel(accel);
          PROFILE_A(0x00020005);

          // try to restart accelerator
          if (accel->group->waiting)
          {
            // pop from waiting list
            slot = accel_pop_slot(accel->group);
            PROFILE_A(0x00020006);

            // handle packet
            handle_accel_start(slot, accel);
            PROFILE_A(0x00020007);
          }
        }
      }
    }
  }

  return 1;
}
