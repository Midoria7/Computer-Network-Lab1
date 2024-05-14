#include <stdio.h>
#include <string.h>
#include "datalink.h"
#include "protocol.h"

#define DATA_TIMER 2000
#define ACK_TIMER 1200
#define MAX_SEQ 63
#define NR_BUFS ((MAX_SEQ + 1) / 2)
#define inc(k) k = (k + 1) % (MAX_SEQ + 1)

typedef struct {
    unsigned char kind; /* FRAME_DATA */
    unsigned char ack;
    unsigned char seq;
    unsigned char data[PKT_LEN];
    unsigned int crc;
} FRAME;

static int phl_ready = 0, no_nak = 1;

static int between(unsigned char a, unsigned char b, unsigned char c) {
    return ((a <= b && b < c) || (c < a && a <= b) || (b < c && c < a));
}

static void put_frame(unsigned char *frame, int len) {
    *(unsigned int *)(frame + len) = crc32(frame, len);
    send_frame(frame, len + 4);
    phl_ready = 0;
}

static void send_data(unsigned char frame_kind, unsigned char frame_nr, unsigned char frame_expected, unsigned char buffer[][PKT_LEN]) {
    FRAME s;
    s.kind = frame_kind;
    s.ack = (frame_expected - 1 + MAX_SEQ + 1) % (MAX_SEQ + 1);
    stop_ack_timer();

    if (frame_kind == FRAME_DATA) {
        s.seq = frame_nr;
        memcpy(s.data, buffer[frame_nr % NR_BUFS], PKT_LEN);
        dbg_frame("Send DATA %d %d, ID %d\n", s.seq, s.ack, *(short *)s.data);
        put_frame((unsigned char *)&s, 3 + PKT_LEN);
        start_timer(frame_nr, DATA_TIMER);
    } else if (frame_kind == FRAME_ACK) {
        dbg_frame("Send ACK  %d\n", s.ack);
        put_frame((unsigned char *)&s, 2);
    } else if (frame_kind == FRAME_NAK) {
        dbg_frame("Send NAK  %d\n", (s.ack + 1) % (MAX_SEQ + 1));
        // ACK next frame is NAK
        no_nak = 0;
        put_frame((unsigned char *)&s, 2);
    } else {
        dbg_warning("**** Frame Kind Error\n");
    }
}

int main(int argc, char **argv) {
    unsigned char ack_expected = 0;
    unsigned char next_frame_to_send = 0;
    unsigned char frame_expected = 0;
    unsigned char too_far = NR_BUFS;
    FRAME r;
    unsigned char out_buf[NR_BUFS][PKT_LEN];
    unsigned char in_buf[NR_BUFS][PKT_LEN];
    unsigned int arrived[NR_BUFS];
    unsigned int nbuffered = 0;
    memset(arrived, 0, sizeof(arrived));

    int event, arg;
    int len = 0;

    protocol_init(argc, argv);
    lprintf("Designed by Xie Muhang, build: " __DATE__ "  "__TIME__"\n");
    disable_network_layer();

    while(1) {
        event = wait_for_event(&arg);

        switch (event) {
            case NETWORK_LAYER_READY:
                nbuffered = nbuffered + 1;
                get_packet(out_buf[next_frame_to_send % NR_BUFS]);
                send_data(FRAME_DATA, next_frame_to_send, frame_expected, out_buf);
                inc(next_frame_to_send);
                break;
            
            case PHYSICAL_LAYER_READY:
                phl_ready = 1;
                break;

            case FRAME_RECEIVED:
                len = recv_frame((unsigned char *)&r, sizeof(r));
                if (len < 5 || crc32((unsigned char *)&r, len) != 0) { // 5 or 6?
                    dbg_event("**** Receiver Error, Bad CRC Checksum\n");
                    if (no_nak)
                        send_data(FRAME_NAK, 0, frame_expected, out_buf);
                    break;
                }

                if (r.kind == FRAME_DATA) {
                    dbg_frame("Recv DATA %d %d, ID %d\n", r.seq, r.ack, *(short *)r.data);
                    if (r.seq != frame_expected && no_nak) {
                        dbg_event(" Recv frame wrong, NAK sent back\n");
                        send_data(FRAME_NAK, 0, frame_expected, out_buf);
                    } else start_ack_timer(ACK_TIMER);

                    if (between(frame_expected, r.seq, too_far)
                        && arrived[r.seq % NR_BUFS] == 0) {
                        arrived[r.seq % NR_BUFS] = 1;
                        memcpy(in_buf[r.seq % NR_BUFS], r.data, PKT_LEN);
                        while (arrived[frame_expected % NR_BUFS]) {
                            dbg_event("Put packetseq:%d, ID: %d\n", frame_expected, *(short *)(in_buf[frame_expected % NR_BUFS]));
                            put_packet(in_buf[frame_expected % NR_BUFS], PKT_LEN);
                            no_nak = 1;
                            arrived[frame_expected % NR_BUFS] = 0;
                            inc(frame_expected);
                            inc(too_far);
                            start_ack_timer(ACK_TIMER);
                        }
                    }
                }
                
                if (r.kind == FRAME_ACK) 
                    dbg_frame("Recv ACK  %d\n", r.ack);

                if (r.kind == FRAME_NAK && between(ack_expected, (r.ack + 1) % (MAX_SEQ + 1), next_frame_to_send)) {
                    dbg_frame("Recv NAK  %d\n",  (r.ack + 1) % (MAX_SEQ + 1));
                    send_data(FRAME_DATA, (r.ack + 1) % (MAX_SEQ + 1), frame_expected, out_buf);
                }

                while (between(ack_expected, r.ack, next_frame_to_send)) {
                    nbuffered = nbuffered - 1;
                    stop_timer(ack_expected % NR_BUFS);
                    inc(ack_expected);
                }
                break;

            case DATA_TIMEOUT:
                dbg_event("---- DATA %d timeout\n", arg);
                send_data(FRAME_DATA, arg, frame_expected, out_buf);
                break;
            
            case ACK_TIMEOUT:
                dbg_event("---- ACK timeout\n");
                send_data(FRAME_ACK, 0, frame_expected, out_buf);
                break;
            
        }
        if (nbuffered < NR_BUFS && phl_ready)
            enable_network_layer();
        else
            disable_network_layer();
                
    }
}
