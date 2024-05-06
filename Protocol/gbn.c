#include <stdio.h>
#include <string.h>
#include "datalink.h"
#include "protocol.h"

#define DATA_TIMER 2000

#define MAX_SEQ 7

typedef struct {
    unsigned char kind; /* FRAME_DATA */
    unsigned char ack;
    unsigned char seq;
    unsigned char data[PKT_LEN];
    unsigned int crc;
} FRAME;

static unsigned char frame_nr = 0, buffer[PKT_LEN], nbuffered;
static unsigned char frame_expected = 0;
static int phl_ready = 0;
static int nak_flag = 0;

static int between(unsigned char a, unsigned char b, unsigned char c) {
    return ((a <= b && b < c) || (c < a && a <= b) || (b < c && c < a));
}

static void put_frame(unsigned char *frame, int len) {
    *(unsigned int *)(frame + len) = crc32(frame, len);
    send_frame(frame, len + 4);
    phl_ready = 0;
}

static void send_data(unsigned char frame_kind, unsigned char frame_nr, unsigned char frame_expected, unsigned char buffer[]) {
    FRAME s;
    s.kind = frame_kind;
    s.ack = (frame_expected - 1 + MAX_SEQ + 1) % (MAX_SEQ + 1);
    stop_ack_timer();

    if (frame_kind = FRAME_DATA) {
        s.seq = frame_nr;
        memcpy(s.data, buffer[frame_nr], PKT_LEN);
        dbg_frame("Send DATA %d %d, ID %d\n", s.seq, s.ack, *(short *)s.data);
        put_frame((unsigned char *)&s, 3 + PKT_LEN);
        start_timer(frame_nr, DATA_TIMER);
    } else if (frame_kind = FRAME_ACK) {
        dbg_frame("Send ACK  %d\n", s.ack);
        put_frame((unsigned char *)&s, 2);
    } else if (frame_kind = FRAME_NAK) {
        dbg_frame("Send NAK  %d\n", (s.ack + 1) % (MAX_SEQ + 1));
        // ACK next frame is NAK
        nak_flag = 1;
        put_frame((unsigned char *)&s, 2);
    } else {
        dbg_warning("**** Frame Kind Error\n");
    }
}

int main(int argc, char **argv) {
    unsigned char next_frame_to_send = 0;
    unsigned char ack_expected = 0;
    unsigned char frame_expected = 0;
    FRAME r;
    unsigned char buffer[MAX_SEQ + 1][PKT_LEN];
    unsigned char nbuffered = 0;

    int event, arg;
    int len = 0;

    protocol_init(argc, argv);
    lprintf("Designed by Xie Muhang, build: " __DATE__ "  "__TIME__"\n");
    disable_network_layer();

    while(1) {
        event = wait_for_event(&arg);

        switch (event) {
            
        }
    }
}
