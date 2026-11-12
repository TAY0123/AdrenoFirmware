/* SPDX-License-Identifier: BSD-2-Clause-Patent */
#include "AdrenoHfiA8xx.h"

/* The A8xx table message and entry layout are defined by Linux's
 * a6xx_hfi.h and a8xx_hfi_send_perf_table. */
enum { HfiMessageTable = 15, HfiTableGpuPerformance = 1, GxStride = 3, CxStride = 2 };

_Static_assert(AdrenoHfiA8xxMaxPerformanceWords + 1 <= UINT8_MAX,
               "HFI header word count must fit in eight bits");

static int valid_active_frequency(uint64_t hz, uint64_t previous_hz)
{
    return hz > previous_hz && hz % 1000 == 0 && hz / 1000 <= UINT32_MAX;
}

AdrenoHfiStatus AdrenoHfiValidateA8xxPerformance(const AdrenoHfiA8xxPerformance *table)
{
    if (!table || table->version != AdrenoHfiA8xxPerformanceVersion ||
        !table->gpu || table->gpu_count < 2 || table->gpu_count > AdrenoHfiA8xxMaxGpuLevels ||
        !table->gmu || table->gmu_count < 2 || table->gmu_count > AdrenoHfiA8xxMaxGmuLevels ||
        table->gpu[0].hz || table->gmu[0].hz || table->gpu[0].dependency_vote != UINT32_MAX) {
        return AdrenoHfiInvalid;
    }
    for (size_t i = 1; i < table->gpu_count; ++i) {
        if (!valid_active_frequency(table->gpu[i].hz, table->gpu[i - 1].hz)) return AdrenoHfiInvalid;
    }
    for (size_t i = 1; i < table->gmu_count; ++i) {
        if (!valid_active_frequency(table->gmu[i].hz, table->gmu[i - 1].hz)) return AdrenoHfiInvalid;
    }
    return AdrenoHfiSuccess;
}

AdrenoHfiStatus AdrenoHfiEncodeA8xxPerformance(const AdrenoHfiA8xxPerformance *table,
    uint32_t *message_id, uint32_t *payload, size_t capacity, size_t *payload_words)
{
    if (!payload_words) return AdrenoHfiInvalid;
    *payload_words = 0;
    if (!message_id || !payload || AdrenoHfiValidateA8xxPerformance(table) != AdrenoHfiSuccess) {
        return AdrenoHfiInvalid;
    }

    size_t words = 6 + GxStride * table->gpu_count + CxStride * table->gmu_count;
    if (capacity < words) return AdrenoHfiNoSpace;

    uint32_t *cursor = payload;
    *cursor++ = table->version;
    *cursor++ = HfiTableGpuPerformance;
    *cursor++ = (uint32_t)table->gpu_count;
    *cursor++ = GxStride;
    for (size_t i = 0; i < table->gpu_count; ++i) {
        const AdrenoHfiA8xxGpuLevel *level = &table->gpu[i];
        *cursor++ = level->vote;
        *cursor++ = level->dependency_vote;
        *cursor++ = (uint32_t)(level->hz / 1000);
    }
    *cursor++ = (uint32_t)table->gmu_count;
    *cursor++ = CxStride;
    for (size_t i = 0; i < table->gmu_count; ++i) {
        const AdrenoHfiPerformanceLevel *level = &table->gmu[i];
        *cursor++ = level->vote;
        *cursor++ = (uint32_t)(level->hz / 1000);
    }

    *message_id = HfiMessageTable;
    *payload_words = words;
    return AdrenoHfiSuccess;
}
