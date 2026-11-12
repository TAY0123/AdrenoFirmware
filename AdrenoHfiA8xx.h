/* SPDX-License-Identifier: BSD-2-Clause-Patent */
#ifndef ADRENO_HFI_A8XX_H
#define ADRENO_HFI_A8XX_H

#include "AdrenoHfiStartup.h"

#ifdef __cplusplus
extern "C" {
#endif

enum {
    AdrenoHfiA8xxPerformanceVersion = 0,
    AdrenoHfiA8xxMaxGpuLevels = 32,
    AdrenoHfiA8xxMaxGmuLevels = 6,
    AdrenoHfiA8xxMaxPerformanceWords = 6 + 3 * AdrenoHfiA8xxMaxGpuLevels + 2 * AdrenoHfiA8xxMaxGmuLevels,
};

typedef struct {
    uint64_t hz;
    uint32_t vote;
    uint32_t dependency_vote;
} AdrenoHfiA8xxGpuLevel;

typedef struct {
    uint32_t version;
    const AdrenoHfiA8xxGpuLevel *gpu;
    size_t gpu_count;
    const AdrenoHfiPerformanceLevel *gmu;
    size_t gmu_count;
} AdrenoHfiA8xxPerformance;

/* Encode the A8xx version-0 performance table for AdrenoHfiSend. The returned
 * payload excludes the wire header and queue padding. Each input array has
 * an off entry at zero Hz followed by at least one strictly increasing OPP.
 * Frequencies must be exactly representable as uint32_t kHz. The off GX
 * dependency vote is UINT32_MAX. All vote words come from the platform's
 * verified OPPs and command database, and are copied without conversion.
 * This function only validates the wire inputs and never sends the message.
 * Inputs and output storage must be disjoint. On failure *payload_words is
 * zero and both *message_id and the payload remain unchanged. */
AdrenoHfiStatus AdrenoHfiEncodeA8xxPerformance(const AdrenoHfiA8xxPerformance *,
    uint32_t *message_id, uint32_t *payload, size_t capacity, size_t *payload_words);

#ifdef __cplusplus
}
#endif
#endif
