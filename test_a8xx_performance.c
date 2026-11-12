/* SPDX-License-Identifier: BSD-2-Clause-Patent */
#include "AdrenoHfiA8xx.h"

#include <assert.h>
#include <stdio.h>

static const uint32_t canary = UINT32_C(0xa5a5a5a5);

static void check_refusal(const AdrenoHfiA8xxPerformance *table, size_t capacity,
                          AdrenoHfiStatus status)
{
    uint32_t id = canary, payload[AdrenoHfiA8xxMaxPerformanceWords];
    size_t words = 99;
    for (size_t i = 0; i < AdrenoHfiA8xxMaxPerformanceWords; ++i) payload[i] = canary;
    assert(AdrenoHfiEncodeA8xxPerformance(table, &id, payload, capacity, &words) == status);
    assert(id == canary && !words);
    for (size_t i = 0; i < AdrenoHfiA8xxMaxPerformanceWords; ++i) assert(payload[i] == canary);
}

static void check_reference(void)
{
    /* Synthetic votes. The expected bytes follow the version/type and the
     * two packed entries in the pinned Linux HFI definition. */
    static const AdrenoHfiA8xxGpuLevel gpu[] = {
        {0, 0xa1b2c3d4, UINT32_MAX},
        {345678000, 0x10203040, 0x50607080},
        {765432000, 0x12345678, 0x90abcdef},
    };
    static const AdrenoHfiPerformanceLevel gmu[] = {
        {0, 0x01020304}, {543210000, 0x11223344},
    };
    static const uint8_t expected[] = {
        0x00, 0x00, 0x00, 0x00,  0x01, 0x00, 0x00, 0x00, /* version, type */
        0x03, 0x00, 0x00, 0x00,  0x03, 0x00, 0x00, 0x00, /* GX count, stride */
        0xd4, 0xc3, 0xb2, 0xa1,  0xff, 0xff, 0xff, 0xff,  0x00, 0x00, 0x00, 0x00,
        0x40, 0x30, 0x20, 0x10,  0x80, 0x70, 0x60, 0x50,  0x4e, 0x46, 0x05, 0x00,
        0x78, 0x56, 0x34, 0x12,  0xef, 0xcd, 0xab, 0x90,  0xf8, 0xad, 0x0b, 0x00,
        0x02, 0x00, 0x00, 0x00,  0x02, 0x00, 0x00, 0x00, /* CX count, stride */
        0x04, 0x03, 0x02, 0x01,  0x00, 0x00, 0x00, 0x00,
        0x44, 0x33, 0x22, 0x11,  0xea, 0x49, 0x08, 0x00,
    };
    const AdrenoHfiA8xxPerformance table = {0, gpu, 3, gmu, 2};
    uint32_t id = canary, payload[AdrenoHfiA8xxMaxPerformanceWords];
    size_t words = 0;
    payload[sizeof(expected) / 4] = canary;
    assert(AdrenoHfiEncodeA8xxPerformance(&table, &id, payload, sizeof(expected) / 4, &words) == AdrenoHfiSuccess);
    assert(id == 15 && words * 4 == sizeof(expected) && payload[words] == canary);
    for (size_t i = 0; i < sizeof(expected); ++i) {
        assert((uint8_t)(payload[i / 4] >> (8 * (i % 4))) == expected[i]);
    }
    check_refusal(&table, words - 1, AdrenoHfiNoSpace);
    check_refusal(&table, 0, AdrenoHfiNoSpace);
    assert(AdrenoHfiEncodeA8xxPerformance(&table, &id, payload, words, NULL) == AdrenoHfiInvalid);
}

static void check_counts(void)
{
    AdrenoHfiA8xxGpuLevel gpu[AdrenoHfiA8xxMaxGpuLevels];
    AdrenoHfiPerformanceLevel gmu[AdrenoHfiA8xxMaxGmuLevels];
    for (size_t i = 0; i < AdrenoHfiA8xxMaxGpuLevels; ++i) {
        gpu[i] = (AdrenoHfiA8xxGpuLevel){1000 * i, (uint32_t)i, i ? (uint32_t)(100 + i) : UINT32_MAX};
    }
    for (size_t i = 0; i < AdrenoHfiA8xxMaxGmuLevels; ++i) {
        gmu[i] = (AdrenoHfiPerformanceLevel){2000 * i, (uint32_t)(200 + i)};
    }
    gpu[AdrenoHfiA8xxMaxGpuLevels - 1].hz = UINT64_C(1000) * UINT32_MAX;
    gmu[AdrenoHfiA8xxMaxGmuLevels - 1].hz = UINT64_C(1000) * UINT32_MAX;
    for (size_t gx = 2; gx <= AdrenoHfiA8xxMaxGpuLevels; ++gx) {
        for (size_t cx = 2; cx <= AdrenoHfiA8xxMaxGmuLevels; ++cx) {
            const AdrenoHfiA8xxPerformance table = {0, gpu, gx, gmu, cx};
            size_t words, capacity = 6 + gx * 3 + cx * 2, cx_entry = 4 + gx * 3;
            uint32_t id, payload[AdrenoHfiA8xxMaxPerformanceWords + 1];
            payload[capacity] = canary;
            assert(AdrenoHfiEncodeA8xxPerformance(&table, &id, payload, capacity, &words) == AdrenoHfiSuccess);
            assert(id == 15 && words == capacity && payload[capacity] == canary);
            assert(payload[0] == 0 && payload[1] == 1 && payload[2] == gx && payload[3] == 3);
            assert(payload[cx_entry] == cx && payload[cx_entry + 1] == 2);
            assert(payload[cx_entry - 1] == gpu[gx - 1].hz / 1000);
            assert(payload[words - 2] == gmu[cx - 1].vote && payload[words - 1] == gmu[cx - 1].hz / 1000);
        }
    }
}

static void check_invalid(void)
{
    AdrenoHfiA8xxGpuLevel gpu[] = {{0, 0, UINT32_MAX}, {1000, 1, 2}, {2000, 3, 4}};
    AdrenoHfiPerformanceLevel gmu[] = {{0, 0}, {1000, 5}, {2000, 6}};
    AdrenoHfiA8xxPerformance table = {0, gpu, 3, gmu, 3};
    const size_t capacity = AdrenoHfiA8xxMaxPerformanceWords;
    check_refusal(NULL, capacity, AdrenoHfiInvalid);
    table.version = 1;
    check_refusal(&table, capacity, AdrenoHfiInvalid);
    table.version = 0;
    table.gpu = NULL;
    check_refusal(&table, capacity, AdrenoHfiInvalid);
    table.gpu = gpu;
    table.gmu = NULL;
    check_refusal(&table, capacity, AdrenoHfiInvalid);
    table.gmu = gmu;
    table.gpu_count = SIZE_MAX;
    check_refusal(&table, capacity, AdrenoHfiInvalid);
    table.gpu_count = 1;
    check_refusal(&table, capacity, AdrenoHfiInvalid);
    table.gpu_count = 3;
    table.gmu_count = AdrenoHfiA8xxMaxGmuLevels + 1;
    check_refusal(&table, capacity, AdrenoHfiInvalid);
    table.gmu_count = 0;
    check_refusal(&table, capacity, AdrenoHfiInvalid);
    table.gmu_count = 3;
    gpu[0].dependency_vote = 0;
    check_refusal(&table, capacity, AdrenoHfiInvalid);
    gpu[0].dependency_vote = UINT32_MAX;
    gmu[0].hz = 1000;
    check_refusal(&table, capacity, AdrenoHfiInvalid);
    gmu[0].hz = 0;
    gpu[2].hz = gpu[1].hz;
    check_refusal(&table, capacity, AdrenoHfiInvalid);
    gpu[2].hz = 2000;
    gmu[2].hz = 1001;
    check_refusal(&table, capacity, AdrenoHfiInvalid);
    gmu[2].hz = (UINT64_C(1) + UINT32_MAX) * 1000;
    check_refusal(&table, capacity, AdrenoHfiInvalid);
}

int main(void)
{
    check_reference();
    check_counts();
    check_invalid();
    puts("A8xx HFI performance table bytes, capacities and refusal cases passed");
}
