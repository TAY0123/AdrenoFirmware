/* SPDX-License-Identifier: BSD-2-Clause-Patent */
/* Software GMU peer using the real AArch64 HFI ring transport. */
#include "AdrenoHfiA8xx.h"

#include <assert.h>
#include <stdio.h>

enum {
    PerformanceAck = 1u << AdrenoHfiStagePerformance,
    BandwidthAck = 1u << AdrenoHfiStageBandwidth,
};

typedef enum { Ack, Reject, NotifyFailure, StaleOnly } Reply;
static _Alignas(4096) uint32_t shared[4096];
static AdrenoHfi hfi;
static AdrenoHfiA8xxTablesState state;
static AdrenoHfiA8xxTablesConfig config;
static AdrenoHfiStartupServices services;
static AdrenoHfiStartupResult result;
static struct {
    uint64_t now;
    unsigned messages, waits, fail_stage;
    Reply failure;
    unsigned delay_first_ack, fail_wait, fail_health, fail_clock;
    uint32_t delayed_ack;
} peer;

static const uint32_t performance_wire[] = {
    0, 1, 2, 3, 0, UINT32_MAX, 0, 0x10203040, 0x50607080, 345678,
    2, 2, 0, 0, 0x11223344, 543210,
};
static const uint32_t bandwidth_wire[159] = {
    [0] = 2, [1] = 1, [2] = 2, [3] = 1, [4] = 2,
    [5] = 0x1000, [11] = 0x40000000, [17] = 0x60000001,
    [23] = 0x2000, [24] = 0x2004, [32] = 0x40000000,
    [39] = 0x20000002, [40] = 0x60000003,
};

static void ack(uint32_t command, uint32_t error)
{
    uint32_t cursor = shared[29];
    shared[2048 + cursor] = 0x1037e;
    shared[2048 + cursor + 1] = command;
    shared[2048 + cursor + 2] = error;
    shared[2048 + cursor + 3] = 0xfafafafa;
    shared[29] = cursor + 4;
}

static int notify(void *cookie)
{
    assert(cookie == &peer && peer.messages < 2);
    unsigned step = ++peer.messages;
    uint32_t cursor = shared[16], command = shared[1024 + cursor];
    const uint32_t *expected = step == 1 ? performance_wire : bandwidth_wire;
    unsigned words = step == 1 ? sizeof(performance_wire) / sizeof(*performance_wire) : 159;
    unsigned id = step == 1 ? 15 : 3;
    assert(command == (step << 20 | (words + 1) << 8 | id));
    for (unsigned i = 0; i < words; ++i) assert(shared[1024 + cursor + 1 + i] == expected[i]);
    assert(result.acknowledged_stages == (step == 1 ? 0 : PerformanceAck));
    shared[16] = shared[17];
    Reply reply = step == peer.fail_stage ? peer.failure : Ack;
    if (reply == NotifyFailure) return -1;
    if (reply == Reject) {
        ack(command, 37);
    } else if (reply == StaleOnly || (step == 1 && peer.delay_first_ack)) {
        ack(command ^ (1u << 20), 0);
        if (reply != StaleOnly) peer.delayed_ack = command;
    } else {
        ack(command, 0);
    }
    return 0;
}

static int now_us(void *cookie, uint64_t *time)
{
    assert(cookie == &peer);
    if (peer.fail_clock && peer.messages) return -1;
    *time = peer.now;
    return 0;
}

static int wait_us(void *cookie, unsigned delay)
{
    assert(cookie == &peer && delay == 10);
    ++peer.waits;
    if (peer.fail_wait) return -1;
    peer.now += delay;
    if (peer.delayed_ack) {
        assert(peer.messages == 1 && !result.acknowledged_stages);
        ack(peer.delayed_ack, 0);
        peer.delayed_ack = 0;
    }
    return 0;
}

static int health(void *cookie)
{
    assert(cookie == &peer);
    return peer.fail_health && result.acknowledged_stages == PerformanceAck;
}

static void setup(void)
{
    static const AdrenoHfiA8xxGpuLevel gx[] = {{0, 0, UINT32_MAX}, {345678000, 0x10203040, 0x50607080}};
    static const AdrenoHfiPerformanceLevel cx[] = {{0, 0}, {543210000, 0x11223344}};
    static const uint32_t cnoc_addresses[6] = {0x1000}, ddr_addresses[8] = {0x2000, 0x2004};
    static const uint32_t cnoc_data[2][6] = {{0x40000000}, {0x60000001}};
    static const uint32_t ddr_data[16][8] = {{0, 0x40000000}, {0x20000002, 0x60000003}};
    peer.now = 1000;
    peer.messages = peer.waits = peer.fail_stage = peer.delay_first_ack = 0;
    peer.fail_wait = peer.fail_health = peer.fail_clock = peer.delayed_ack = 0;
    peer.failure = Ack;
    state = (AdrenoHfiA8xxTablesState){0};
    config = (AdrenoHfiA8xxTablesConfig){
        .performance = {0, gx, 2, cx, 2},
        .bandwidth = {2, 1, 2, 1, 2, cnoc_addresses, cnoc_data, ddr_addresses, ddr_data},
    };
    services = (AdrenoHfiStartupServices){&peer, now_us, wait_us, health, 3, 10, 30, 60};
    assert(AdrenoHfiInitializeStopped(&hfi, shared, sizeof(shared), 0x60008000, &peer, notify) == AdrenoHfiSuccess);
}

static AdrenoHfiStartupStatus run(void)
{
    return AdrenoHfiA8xxProgramTables(&state, &hfi, &config, &services, &result);
}

static void check_failure(AdrenoHfiStartupStatus status, unsigned messages, unsigned acknowledged)
{
    assert(run() == status && peer.messages == messages && result.published_commands == messages);
    assert(result.acknowledged_stages == acknowledged && result.resources_must_retain);
    assert(state.sequence.attempted && !state.sequence.completed && state.sequence.poisoned && hfi.poisoned);
    assert(run() == AdrenoHfiStartupPoisoned && peer.messages == messages);
}

int main(void)
{
    setup();
    peer.delay_first_ack = 1;
    assert(run() == AdrenoHfiStartupSuccess && peer.messages == 2 && peer.waits == 1 && hfi.skipped == 1);
    assert(state.sequence.completed && result.stage == AdrenoHfiStageBandwidth && result.resources_must_retain);
    assert(result.acknowledged_stages == (PerformanceAck | BandwidthAck) && !hfi.pending_header && !hfi.poisoned);
    assert(run() == AdrenoHfiStartupBusy && peer.messages == 2);

    for (unsigned step = 1; step <= 2; ++step) {
        unsigned acknowledged = step == 1 ? 0 : PerformanceAck;
        setup();
        peer.fail_stage = step;
        peer.failure = Reject;
        assert(run() == AdrenoHfiStartupTransportError && result.transport_status == AdrenoHfiFirmwareError);
        assert(result.firmware_error == 37 && peer.messages == step && result.acknowledged_stages == acknowledged);
        assert(state.sequence.poisoned && hfi.poisoned && run() == AdrenoHfiStartupPoisoned);
        setup();
        peer.fail_stage = step;
        peer.failure = NotifyFailure;
        check_failure(AdrenoHfiStartupTransportError, step, acknowledged);
        setup();
        peer.fail_stage = step;
        peer.failure = StaleOnly;
        check_failure(AdrenoHfiStartupTimeout, step, acknowledged);
    }

    setup();
    peer.fail_wait = peer.delay_first_ack = 1;
    check_failure(AdrenoHfiStartupWaitError, 1, 0);
    setup();
    peer.fail_health = 1;
    check_failure(AdrenoHfiStartupPlatformFault, 1, PerformanceAck);
    setup();
    peer.fail_clock = 1;
    check_failure(AdrenoHfiStartupClockError, 1, 0);
    setup();
    config.bandwidth.ddr_wait_mask = 4;
    assert(run() == AdrenoHfiStartupInvalid && !state.sequence.attempted && !peer.messages && !shared[17]);
    setup();
    config.bandwidth.ddr_data = NULL;
    assert(run() == AdrenoHfiStartupInvalid && !state.sequence.attempted && !peer.messages && !shared[17]);
    setup();
    config.performance.version = 1;
    assert(run() == AdrenoHfiStartupInvalid && !state.sequence.attempted && !peer.messages && !shared[17]);
    puts("A8xx HFI table ordering, software ACKs, and failure propagation passed");
    return 0;
}
