#include "radio.h"
#include "diag.h"
#include <string.h>

#define HEADER_MCU   0x01u
#define HEADER_RADIO 0x02u // answers
#define HEADER_EVENT 0x03u // the radio's notification: answered with F0
#define HEADER_INFO  0x08u // the radio's long frame: answered with F1

#define CMD_MODE      0x01u
#define CMD_REPORT    0x02u
#define CMD_CONSUMER  0x03u
#define CMD_STATUS    0x06u
#define CMD_NAME      0x09u
#define CMD_ACK_EVENT 0xF0u
#define CMD_ACK_INFO  0xF1u

#define STATUS_LEN 10u // 02 06 + 6 bytes + checksum
#define ANSWER_LEN 6u
#define INFO_LEN   22u

#define MIN_GAP_MS       1u   // after a frame, so that ATT has time to drop before it is looked at
#define TX_TIMEOUT_MS    50u  // a frame that never finishes (the serial port is stuck)
#define MODE_RESEND_MS   300u // the mode command is sent again until the radio has answered it (it may not be up yet)
#define MODE_TRIES       8u
#define REFUSED_RETRY_MS 10u
#define POLL_MS          200u
#define KEY_QUEUE        8u // a power of two: keyboard states waiting to go out (a tap is a press and a release in one pass, and a
                            // fast sequence must not lose one: the queue holds eight states, ~4 keys typed back to back)
#define CONSUMER_QUEUE   4u // consumer usages waiting

uint8_t radio_checksum(const uint8_t *data, uint8_t length)
{
    uint8_t sum = 0;

    for (uint8_t i = 0; i < length; i++) {
        sum = (uint8_t)(sum + data[i]);
    }

    return (uint8_t)(0x55u - sum);
}

uint8_t radio_build_command(uint8_t *out, uint8_t command, uint8_t a, uint8_t b, uint8_t c)
{
    out[0] = HEADER_MCU;
    out[1] = command;
    out[2] = a;
    out[3] = b;
    out[4] = c;
    out[5] = radio_checksum(out, 5);
    return RADIO_COMMAND_LEN;
}

// 01 02 <mods> <5 usages> 00 <16-byte bitmap of the usages below 128> 00 00 00 00 <checksum>.
// The radio presents two keyboards to the host, a plain one (the list) and an NKRO one (the bitmap), so a key goes in ONE of them,
// never both: in both it was typed twice. Up to 5 keys go in the list; six keys, more than the list holds, in the bitmap.
uint8_t radio_build_report(uint8_t *out, uint8_t mods, const uint8_t keys[6])
{
    uint8_t count = 0;

    memset(out, 0, RADIO_REPORT_LEN);
    out[0] = HEADER_MCU;
    out[1] = CMD_REPORT;
    out[2] = mods;

    for (uint8_t i = 0; i < 6; i++) {
        if (keys[i] != 0) {
            count++;
        }
    }

    if (count <= 5) {
        uint8_t n = 0;

        for (uint8_t i = 0; i < 6; i++) {
            if (keys[i] != 0) {
                out[3 + n++] = keys[i];
            }
        }
    } else {
        for (uint8_t i = 0; i < 6; i++) {
            if (keys[i] != 0 && keys[i] < 128u) {
                out[9 + (keys[i] >> 3)] |= (uint8_t)(1u << (keys[i] & 7u));
            }
        }
    }

    out[RADIO_REPORT_LEN - 1] = radio_checksum(out, RADIO_REPORT_LEN - 1);
    return RADIO_REPORT_LEN;
}

// 01 03 <usage lo> <usage hi> 00 <7 bytes, 0> <checksum> (13 bytes): the stock's consumer record
uint8_t radio_build_consumer(uint8_t *out, uint16_t usage)
{
    memset(out, 0, RADIO_CONSUMER_LEN);
    out[0]                          = HEADER_MCU;
    out[1]                          = CMD_CONSUMER;
    out[2]                          = (uint8_t)(usage & 0xFFu);
    out[3]                          = (uint8_t)(usage >> 8);
    out[RADIO_CONSUMER_LEN - 1] = radio_checksum(out, RADIO_CONSUMER_LEN - 1);
    return RADIO_CONSUMER_LEN;
}

// 01 09 <type> <length> <name> 00 ... <checksum> (32 bytes): type 0 is the Bluetooth LE name, 1 the classic one
uint8_t radio_build_name(uint8_t *out, uint8_t type, const char *name)
{
    uint8_t length = 0;

    memset(out, 0, RADIO_NAME_FRAME_LEN);

    while (name[length] != 0 && length < RADIO_NAME_MAX) {
        out[4 + length] = (uint8_t)name[length];
        length++;
    }

    out[0]                            = HEADER_MCU;
    out[1]                            = CMD_NAME;
    out[2]                            = type;
    out[3]                            = length;
    out[RADIO_NAME_FRAME_LEN - 1] = radio_checksum(out, RADIO_NAME_FRAME_LEN - 1);
    return RADIO_NAME_FRAME_LEN;
}

typedef enum { KIND_NONE, KIND_MODE, KIND_REPORT, KIND_STATUS, KIND_ACK, KIND_CONSUMER, KIND_NAME } kind_t;

static uint8_t frame[RADIO_FRAME_MAX];
static uint8_t frame_length;
static bool    transmitting;
static kind_t  last_kind;      // the frame that went out last
static uint8_t last_mods;      // ... and, when it was a keyboard report, what it carried
static uint8_t last_keys[6];
static uint16_t last_usage;    // ... or a consumer usage
static uint8_t acked_mods;     // the keyboard state the radio last accepted
static uint8_t acked_keys[6];
static bool    acked_valid;
static uint16_t acked_usage;   // the consumer usage the radio last accepted
static bool    acked_usage_valid;
static uint16_t t_start;       // when the frame in flight began
static uint16_t t_sent;        // when the last frame ended
static uint16_t t_status;
static uint16_t t_mode;
static uint16_t t_backoff;
static bool    backoff;
static bool    active;

static bool    mode_pending; // to be sent
static bool    mode_unacked; // sent, no answer yet
static uint8_t mode_tries;
static uint8_t mode_slot;
static bool    mode_pairing;
static bool    ack_event;    // a 03 frame came: answer F0
static bool    ack_info;     // an 08 frame came: answer F1
static bool    names_done;   // the Bluetooth names are set once per power-up, after the first answer to the mode command
static uint8_t names_step;   // 1: the LE name goes next, 2: the classic one

// The reports wait in two small queues, in order: a tap is a press and a release back to back, and "the latest state" alone would
// lose the press. Full: the newest replaces the last.
static uint8_t  key_queue[KEY_QUEUE][7]; // modifiers, 6 keys
static uint8_t  key_head;
static uint8_t  key_count;
static uint16_t consumer_queue[CONSUMER_QUEUE];
static uint8_t  consumer_head;
static uint8_t  consumer_count;

static uint8_t  rx_frame[INFO_LEN];
static uint8_t  rx_length;
static uint8_t  status[6];
static uint8_t  last_info[INFO_LEN]; // the last 08 frame, whole: what is in it is not known yet (diag.h)

static void key_push(uint8_t mods, const uint8_t *keys)
{
    uint8_t at;

    if (key_count < KEY_QUEUE) {
        at = (uint8_t)((key_head + key_count) & (KEY_QUEUE - 1u));
        key_count++;
    } else {
        at = (uint8_t)((key_head + key_count - 1u) & (KEY_QUEUE - 1u));
    }

    key_queue[at][0] = mods;
    memcpy(&key_queue[at][1], keys, 6);
}

static void key_unpop(void) // a refused report goes back to the front
{
    if (key_count < KEY_QUEUE) {
        key_head              = (uint8_t)((key_head + KEY_QUEUE - 1u) & (KEY_QUEUE - 1u));
        key_queue[key_head][0] = last_mods;
        memcpy(&key_queue[key_head][1], last_keys, 6);
        key_count++;
    }
}

static void consumer_push(uint16_t usage)
{
    uint8_t at;

    if (consumer_count < CONSUMER_QUEUE) {
        at = (uint8_t)((consumer_head + consumer_count) & (CONSUMER_QUEUE - 1u));
        consumer_count++;
    } else {
        at = (uint8_t)((consumer_head + consumer_count - 1u) & (CONSUMER_QUEUE - 1u));
    }

    consumer_queue[at] = usage;
}

static void consumer_unpop(void)
{
    if (consumer_count < CONSUMER_QUEUE) {
        consumer_head                = (uint8_t)((consumer_head + CONSUMER_QUEUE - 1u) & (CONSUMER_QUEUE - 1u));
        consumer_queue[consumer_head] = last_usage;
        consumer_count++;
    }
}

bool radio_active(void)
{
    return active;
}

bool radio_idle(void)
{
    return !transmitting && !mode_pending && key_count == 0 && consumer_count == 0 && !ack_event && !ack_info && names_step == 0;
}

const uint8_t *radio_status(void)
{
    return status;
}

const uint8_t *radio_last_info(void)
{
    return last_info;
}

static void clear_queues(void)
{
    key_head       = 0;
    key_count      = 0;
    consumer_head  = 0;
    consumer_count = 0;
}

void radio_init(void)
{
    transmitting      = false;
    last_kind         = KIND_NONE;
    active            = false;
    mode_pending      = false;
    mode_unacked      = false;
    ack_event         = false;
    ack_info          = false;
    names_done        = false;
    names_step        = 0;
    backoff           = false;
    acked_valid       = false;
    acked_usage_valid = false;
    rx_length         = 0;
    t_sent            = 0;
    t_status          = 0;
    clear_queues();
    memset(status, 0, sizeof status);
    memset(last_info, 0, sizeof last_info);
    radio_hook_select(false);
}

void radio_set_link(uint8_t slot, bool pairing)
{
    static const uint8_t no_keys[6] = {0, 0, 0, 0, 0, 0};

    active       = true;
    mode_slot    = (slot > RADIO_MODE_BT3) ? RADIO_MODE_BT3 : slot;
    mode_pairing = pairing;
    mode_pending = true;
    mode_unacked = true;
    mode_tries   = 0;

    // whatever the old link held down is let go on the new one too
    clear_queues();
    key_push(0, no_keys);
    consumer_push(0);
    acked_valid       = false; // what the radio holds on the new link is not known
    acked_usage_valid = false;
}

void radio_stop(void)
{
    active       = false;
    mode_pending = false;
    mode_unacked = false;
    ack_event    = false;
    ack_info     = false;
    names_step   = 0;
    transmitting = false;
    rx_length    = 0;
    clear_queues();
    radio_hook_select(false);
}

void radio_monitor(bool on)
{
    if (!on) {
        radio_stop();
        return;
    }

    active       = true;
    mode_pending = false;
    mode_unacked = false;
    clear_queues();
}

// One frame, waiting for the radio (ATT) and for the end of the transmission with delays: the timer is stopped when this runs, so the
// clock does not move
static void send_blocking(uint8_t command, uint8_t a, uint8_t b)
{
    uint8_t f[RADIO_COMMAND_LEN];

    for (uint8_t i = 0; i < 50u && !radio_hook_ready(); i++) {
        radio_hook_delay_ms(1);
    }

    radio_build_command(f, command, a, b, 0);
    radio_hook_select(true);
    radio_hook_tx_start(f, RADIO_COMMAND_LEN);

    for (uint8_t i = 0; i < 20u && !radio_hook_tx_done(); i++) {
        radio_hook_delay_ms(1);
    }

    radio_hook_select(false);
    radio_hook_delay_ms(20); // the stock firmware leaves 20 units between its frames
}

void radio_sleep(void)
{
    send_blocking(0x0Cu, 0x08u, 0x07u);
    send_blocking(0x0Bu, 0x00u, 0x00u);
    radio_stop();
}

void radio_send_keys(uint8_t mods, const uint8_t keys[6])
{
    key_push(mods, keys);
}

void radio_send_consumer(uint16_t usage)
{
    consumer_push(usage);
}

static bool last_report_is_acked(void)
{
    return acked_valid && acked_mods == last_mods && memcmp(acked_keys, last_keys, sizeof acked_keys) == 0;
}

static void mode_answered(void)
{
    mode_unacked = false;

    if (!names_done) {
        names_done = true;
        names_step = 1; // the Bluetooth names, once
    }
}

static void refused(void)
{
    // "not now": what the frame was sent for has to be offered again, unless it changed nothing
    if (last_kind == KIND_REPORT && !last_report_is_acked()) {
        key_unpop();
        backoff   = true;
        t_backoff = (uint16_t)(radio_hook_now_ms() + REFUSED_RETRY_MS);
    } else if (last_kind == KIND_CONSUMER && !(acked_usage_valid && acked_usage == last_usage)) {
        consumer_unpop();
        backoff   = true;
        t_backoff = (uint16_t)(radio_hook_now_ms() + REFUSED_RETRY_MS);
    } else if (last_kind == KIND_MODE) {
        mode_answered(); // already in that mode
    }
}

// One frame from the radio. Anything that does not check out is dropped and the parser looks for the next 02 / 03 / 08.
static void rx_frame_done(void)
{
    if (radio_checksum(rx_frame, (uint8_t)(rx_length - 1)) != rx_frame[rx_length - 1]) {
        return;
    }

    diag_inc(DIAG_RADIO_RX);

    switch (rx_frame[0]) {
        case HEADER_EVENT:
            ack_event = true;
            break;

        case HEADER_INFO:
            memcpy(last_info, rx_frame, INFO_LEN);
            ack_info = true;
            break;

        case HEADER_RADIO:
            if (rx_frame[1] == CMD_STATUS && rx_length == STATUS_LEN) {
                memcpy(status, &rx_frame[2], sizeof status);
            } else if (rx_frame[1] == 0x00u && rx_frame[2] == 0x01u) {
                refused();
            } else if (rx_frame[1] == CMD_MODE) {
                diag_inc(DIAG_RADIO_MODE);
                mode_answered();
            } else if (rx_frame[1] == CMD_REPORT && last_kind == KIND_REPORT) {
                acked_mods = last_mods;
                memcpy(acked_keys, last_keys, sizeof acked_keys);
                acked_valid = true;
            } else if (rx_frame[1] == CMD_CONSUMER && last_kind == KIND_CONSUMER) {
                acked_usage       = last_usage;
                acked_usage_valid = true;
            }
            break;

        default:
            break;
    }
}

static uint8_t frame_want(void)
{
    if (rx_length < 2) {
        return ANSWER_LEN;
    }

    if (rx_frame[0] == HEADER_RADIO && rx_frame[1] == CMD_STATUS) {
        return STATUS_LEN;
    }

    return (rx_frame[0] == HEADER_INFO) ? INFO_LEN : ANSWER_LEN;
}

static void rx_task(void)
{
    uint8_t b;

    while (radio_hook_rx_pop(&b)) {
        if (rx_length == 0 && b != HEADER_RADIO && b != HEADER_EVENT && b != HEADER_INFO) {
            continue;
        }

        rx_frame[rx_length++] = b;

        if (rx_length >= frame_want()) {
            rx_frame_done();
            rx_length = 0;
        }
    }
}

static void start_frame(kind_t kind)
{
    diag_inc(DIAG_RADIO_TX);
    last_kind = kind;
    t_start   = radio_hook_now_ms();
    radio_hook_select(true);
    radio_hook_tx_start(frame, frame_length);
    transmitting = true;
}

// Which frame goes out next, if any: only with ATT high, a millisecond after the last one
static void pick_next(void)
{
    const uint16_t now = radio_hook_now_ms();

    if (!radio_hook_ready()) {
        return; // ATT low: the radio is busy
    }

    if ((uint16_t)(now - t_sent) < MIN_GAP_MS) {
        return;
    }

    if (backoff && (int16_t)(now - t_backoff) >= 0) {
        backoff = false;
    }

    if (ack_event) {
        ack_event    = false;
        frame_length = radio_build_command(frame, CMD_ACK_EVENT, 0, 0, 0);
        start_frame(KIND_ACK);
    } else if (ack_info) {
        ack_info     = false;
        frame_length = radio_build_command(frame, CMD_ACK_INFO, 0, 0, 0);
        start_frame(KIND_ACK);
    } else if (mode_pending) {
        mode_pending = false;
        t_mode       = now;
        mode_tries++;
        frame_length = radio_build_command(frame, CMD_MODE, mode_pairing ? 1u : 0u, mode_slot, 0);
        start_frame(KIND_MODE);
    } else if (names_step) {
        const uint8_t type = (uint8_t)(names_step - 1u); // 0: Bluetooth LE, 1: classic

        names_step   = (names_step == 1u) ? 2u : 0u;
        frame_length = radio_build_name(frame, type, RADIO_NAME);
        start_frame(KIND_NAME);
    } else if (key_count && !backoff) {
        last_mods = key_queue[key_head][0];
        memcpy(last_keys, &key_queue[key_head][1], 6);
        key_head = (uint8_t)((key_head + 1u) & (KEY_QUEUE - 1u));
        key_count--;
        frame_length = radio_build_report(frame, last_mods, last_keys);
        start_frame(KIND_REPORT);
    } else if (consumer_count && !backoff) {
        last_usage     = consumer_queue[consumer_head];
        consumer_head  = (uint8_t)((consumer_head + 1u) & (CONSUMER_QUEUE - 1u));
        consumer_count--;
        frame_length = radio_build_consumer(frame, last_usage);
        start_frame(KIND_CONSUMER);
    } else if (mode_unacked && (uint16_t)(now - t_mode) >= MODE_RESEND_MS) {
        if (mode_tries < MODE_TRIES) {
            mode_pending = true; // not answered: the radio may not have been up yet
        } else {
            mode_unacked = false;
        }
    } else if ((uint16_t)(now - t_status) >= POLL_MS) {
        t_status     = now;
        frame_length = radio_build_command(frame, CMD_STATUS, 0, 0, 0);
        start_frame(KIND_STATUS);
    }
}

void radio_task(void)
{
    if (!active) {
        return;
    }

    rx_task();

    if (transmitting) {
        if (radio_hook_tx_done() || (uint16_t)(radio_hook_now_ms() - t_start) >= TX_TIMEOUT_MS) {
            radio_hook_select(false); // released with the last byte, as the stock does in its interrupt
            t_sent       = radio_hook_now_ms();
            transmitting = false;
        }
        return;
    }

    pick_next();
}
