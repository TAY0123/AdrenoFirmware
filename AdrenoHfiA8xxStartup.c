/* SPDX-License-Identifier: BSD-2-Clause-Patent */
#include "AdrenoHfiA8xx.h"
#include "AdrenoHfiStartupInternal.h"

static int tables_valid(const void *context)
{
    const AdrenoHfiA8xxTablesConfig *config = context;
    return config && AdrenoHfiValidateA8xxPerformance(&config->performance) == AdrenoHfiSuccess &&
        AdrenoHfiBandwidthValid(&config->bandwidth);
}

static AdrenoHfiStartupStatus encode_table(const void *context, AdrenoHfiStartupStage stage,
    uint32_t *id, uint32_t *payload, size_t capacity, size_t *words)
{
    const AdrenoHfiA8xxTablesConfig *config = context;
    switch (stage) {
    case AdrenoHfiStagePerformance:
        return AdrenoHfiEncodeA8xxPerformance(&config->performance, id, payload, capacity, words) ==
            AdrenoHfiSuccess ? AdrenoHfiStartupSuccess : AdrenoHfiStartupInvalid;
    case AdrenoHfiStageBandwidth:
        if (capacity < AdrenoHfiBandwidthWords) return AdrenoHfiStartupInvalid;
        AdrenoHfiEncodeBandwidth(&config->bandwidth, payload);
        *id = AdrenoHfiBandwidthMessage;
        *words = AdrenoHfiBandwidthWords;
        return AdrenoHfiStartupSuccess;
    default:
        return AdrenoHfiStartupInvalid;
    }
}

AdrenoHfiStartupStatus AdrenoHfiA8xxProgramTables(AdrenoHfiA8xxTablesState *state, AdrenoHfi *hfi,
    const AdrenoHfiA8xxTablesConfig *config, const AdrenoHfiStartupServices *services,
    AdrenoHfiStartupResult *result)
{
    const AdrenoHfiStartupSequence sequence = {
        .config = config,
        .stages = (1u << AdrenoHfiStagePerformance) | (1u << AdrenoHfiStageBandwidth),
        .completed_stage = AdrenoHfiStageBandwidth,
        .validate = tables_valid,
        .encode = encode_table,
    };
    return AdrenoHfiStartupRunSequence(state ? &state->sequence : NULL, hfi, &sequence, services, result);
}
