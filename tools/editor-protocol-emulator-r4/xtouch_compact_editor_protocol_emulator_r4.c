#include <CoreMIDI/CoreMIDI.h>
#include <CoreFoundation/CoreFoundation.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <stdarg.h>
#include <signal.h>
#include <time.h>

#define REPORT_NAME "XTouch_Compact_Editor_Protocol_Emulator_R4_Report.txt"
#define MAX_SYSEX 1024

typedef enum { STATE_APP = 0, STATE_UBOOT = 1 } VirtualState;

static MIDIClientRef gClient = 0;
static MIDIEndpointRef gDest = 0;
static MIDIEndpointRef gSource = 0;
static FILE *gReport = NULL;
static struct timespec gStart;
static volatile sig_atomic_t gStop = 0;
static VirtualState gState = STATE_APP;
static double gLastAB60 = -1.0;
static int gCapturedPostUBoot = 0;
static uint8_t gSysex[MAX_SYSEX];
static size_t gSysexLen = 0;
static int gInSysex = 0;

static double now_ms(void) {
    struct timespec t;
    clock_gettime(CLOCK_MONOTONIC, &t);
    return (double)(t.tv_sec - gStart.tv_sec) * 1000.0 + (double)(t.tv_nsec - gStart.tv_nsec) / 1000000.0;
}

static void log_line(const char *fmt, ...) {
    va_list ap;
    double ms = now_ms();
    fprintf(stdout, "%10.3f ms  ", ms);
    if (gReport) fprintf(gReport, "%10.3f ms  ", ms);
    va_start(ap, fmt);
    vfprintf(stdout, fmt, ap);
    va_end(ap);
    va_start(ap, fmt);
    if (gReport) vfprintf(gReport, fmt, ap);
    va_end(ap);
    fputc('\n', stdout);
    fflush(stdout);
    if (gReport) { fputc('\n', gReport); fflush(gReport); }
}

static void log_hex(const char *prefix, const uint8_t *d, size_t n) {
    double ms = now_ms();
    fprintf(stdout, "%10.3f ms  %s [", ms, prefix);
    if (gReport) fprintf(gReport, "%10.3f ms  %s [", ms, prefix);
    for (size_t i = 0; i < n; ++i) {
        fprintf(stdout, "%s%02X", i ? " " : "", d[i]);
        if (gReport) fprintf(gReport, "%s%02X", i ? " " : "", d[i]);
    }
    fprintf(stdout, "]\n");
    if (gReport) { fprintf(gReport, "]\n"); fflush(gReport); }
    fflush(stdout);
}

static void send_bytes(const uint8_t *data, size_t len, const char *label) {
    struct { MIDIPacketList list; uint8_t storage[256]; } buffer;
    MIDIPacket *p = MIDIPacketListInit(&buffer.list);
    p = MIDIPacketListAdd(&buffer.list, sizeof(buffer), p, 0, (UInt16)len, data);
    if (!p) { log_line("ERROR could not build MIDI packet for %s", label); return; }
    OSStatus s = MIDIReceived(gSource, &buffer.list);
    if (s == noErr) log_hex(label, data, len);
    else log_line("ERROR MIDIReceived(%s) status=%d", label, (int)s);
}

static int has_header(const uint8_t *d, size_t n, uint8_t cmd) {
    return n >= 6 && d[0] == 0xF0 && d[1] == 0x40 && d[2] == 0x41 && d[3] == 0x42 && d[4] == cmd && d[n-1] == 0xF7;
}

static void handle_sysex(const uint8_t *d, size_t n) {
    log_hex("RX", d, n);

    if (has_header(d, n, 0x51)) {
        if (gState == STATE_APP) {
            static const uint8_t appReply[] = {0xF0,0x40,0x41,0x42,0x51,0x00,0x01,0x00,0x00,0x01,0x0E,0x00,0x00,0xF7};
            send_bytes(appReply, sizeof(appReply), "TX APP @ABQ reply (FW 1.14 emulation)");
        } else {
            log_line("UBOOT state: @ABQ deliberately left unanswered");
        }
        return;
    }

    if (has_header(d, n, 0x36)) {
        if (gState == STATE_UBOOT) {
            static const uint8_t ubootReply[] = {
                0xF0,0x40,0x41,0x42,0x36,0x00,
                0x02,0x02,0x02,0x02,0x01,0x01,0x01,0x01,
                0x00,0x00,0x00,0x00,0x00,0x00,0x00,0xF7
            };
            send_bytes(ubootReply, sizeof(ubootReply), "TX uBoot @AB6 reply signature=0x11112222");
        } else {
            log_line("APP state: @AB6 uBoot poll deliberately left unanswered");
        }
        return;
    }

    if (has_header(d, n, 0x60) && d[5] == 0x00 && gState == STATE_APP) {
        gLastAB60 = now_ms();
        log_line("TRANSITION CANDIDATE part 1 captured: @AB` 00 (no reply)");
        return;
    }

    if (has_header(d, n, 0x61) && d[5] == 0x01 && gState == STATE_APP) {
        double dt = gLastAB60 < 0.0 ? 1e9 : now_ms() - gLastAB60;
        if (dt <= 250.0) {
            log_line("TRANSITION CANDIDATE part 2 captured: @ABa 01, %.3f ms after @AB` 00", dt);
            gState = STATE_UBOOT;
            log_line("VIRTUAL STATE CHANGE: APP -> UBOOT (emulation only; no hardware command sent)");
        } else {
            log_line("@ABa 01 captured without recent @AB` 00; state remains APP");
        }
        return;
    }

    if (has_header(d, n, 0x52)) {
        log_line("@ABR normal hardware-layer command captured; deliberately not answered");
        return;
    }

    if (gState == STATE_UBOOT && n >= 6 && d[0] == 0xF0 && d[1] == 0x40 && d[2] == 0x41 && d[3] == 0x42) {
        log_line("FIRST POST-UBOOT COMMAND CAPTURED: command=0x%02X parameter=0x%02X", d[4], d[5]);
        log_line("NO ACK SENT. Capture complete; stopping emulator safely.");
        gCapturedPostUBoot = 1;
        gStop = 1;
        return;
    }

    log_line("Non-identity SysEx deliberately NOT answered");
}

static void feed_byte(uint8_t b) {
    if (!gInSysex) {
        if (b == 0xF0) { gInSysex = 1; gSysexLen = 0; gSysex[gSysexLen++] = b; }
        return;
    }
    if (gSysexLen < MAX_SYSEX) gSysex[gSysexLen++] = b;
    else { log_line("ERROR SysEx buffer overflow; frame discarded"); gInSysex = 0; gSysexLen = 0; return; }
    if (b == 0xF7) { handle_sysex(gSysex, gSysexLen); gInSysex = 0; gSysexLen = 0; }
}

static void read_proc(const MIDIPacketList *pktlist, void *readProcRefCon, void *srcConnRefCon) {
    (void)readProcRefCon; (void)srcConnRefCon;
    const MIDIPacket *pkt = &pktlist->packet[0];
    for (UInt32 i = 0; i < pktlist->numPackets; ++i) {
        for (UInt16 j = 0; j < pkt->length; ++j) feed_byte(pkt->data[j]);
        pkt = MIDIPacketNext(pkt);
    }
}

static void on_sigint(int sig) { (void)sig; gStop = 1; }

static void cleanup(void) {
    if (gDest) MIDIEndpointDispose(gDest);
    if (gSource) MIDIEndpointDispose(gSource);
    if (gClient) MIDIClientDispose(gClient);
    if (gReport) {
        fprintf(gReport, "%10.3f ms  Emulator stopping. post_uboot_capture=%s\n", now_ms(), gCapturedPostUBoot ? "yes" : "no");
        fclose(gReport);
        gReport = NULL;
    }
}

int main(void) {
    clock_gettime(CLOCK_MONOTONIC, &gStart);
    signal(SIGINT, on_sigint);
    signal(SIGTERM, on_sigint);

    gReport = fopen(REPORT_NAME, "w");
    if (!gReport) { fprintf(stderr, "Cannot create %s\n", REPORT_NAME); return 1; }

    fprintf(gReport,
        "X-TOUCH COMPACT EDITOR PROTOCOL EMULATOR R4\n"
        "Physical X-Touch Compact MUST be powered OFF / USB disconnected.\n"
        "Virtual APP identity: firmware 1.14.\n"
        "Virtual uBoot state reply: nibble signature 0x11112222.\n"
        "Transition heuristic: exact R3 pair @AB` 00 -> @ABa 01 within 250 ms.\n"
        "SAFETY: no @AB3/@AB4/@AB5/@AB8 acknowledgement, no firmware data, no erase/write/reset command.\n\n");
    fflush(gReport);

    OSStatus s = MIDIClientCreate(CFSTR("XTouch Compact R4 Emulator"), NULL, NULL, &gClient);
    if (s != noErr) { fprintf(stderr, "MIDIClientCreate failed: %d\n", (int)s); cleanup(); return 2; }
    s = MIDIDestinationCreate(gClient, CFSTR("X-TOUCH COMPACT"), read_proc, NULL, &gDest);
    if (s != noErr) { fprintf(stderr, "MIDIDestinationCreate failed: %d\n", (int)s); cleanup(); return 3; }
    s = MIDISourceCreate(gClient, CFSTR("X-TOUCH COMPACT"), &gSource);
    if (s != noErr) { fprintf(stderr, "MIDISourceCreate failed: %d\n", (int)s); cleanup(); return 4; }

    log_line("Virtual CoreMIDI source + destination created as X-TOUCH COMPACT.");
    log_line("SAFE R4: APP @ABQ and virtual-uBoot @AB6 are the ONLY replies implemented.");
    log_line("Physical Compact must remain disconnected for the entire run.");
    log_line("Open X-TOUCH Editor; enter its normal firmware-update workflow with the original Behringer .bin.");
    log_line("R4 will stop automatically after the first post-uBoot @AB command is captured.");

    while (!gStop) CFRunLoopRunInMode(kCFRunLoopDefaultMode, 0.10, false);
    cleanup();
    return 0;
}
