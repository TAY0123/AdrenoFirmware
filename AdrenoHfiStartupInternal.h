/* SPDX-License-Identifier: BSD-2-Clause-Patent */
#ifndef ADRENO_HFI_STARTUP_INTERNAL_H
#define ADRENO_HFI_STARTUP_INTERNAL_H

#include "AdrenoHfiStartup.h"

enum { AdrenoHfiBandwidthWords = 159, AdrenoHfiBandwidthMessage = 3 };

int AdrenoHfiBandwidthValid(const AdrenoHfiBandwidth *);
void AdrenoHfiEncodeBandwidth(const AdrenoHfiBandwidth *, uint32_t payload[AdrenoHfiBandwidthWords]);

typedef struct {
    const void *config;
    uint32_t stages;
    AdrenoHfiStartupStage completed_stage;
    int (*validate)(const void *);
    AdrenoHfiStartupStatus (*encode)(const void *, AdrenoHfiStartupStage,
        uint32_t *, uint32_t *, size_t, size_t *);
} AdrenoHfiStartupSequence;

AdrenoHfiStartupStatus AdrenoHfiStartupRunSequence(AdrenoHfiStartup *, AdrenoHfi *,
    const AdrenoHfiStartupSequence *, const AdrenoHfiStartupServices *, AdrenoHfiStartupResult *);

#endif
