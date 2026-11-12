# AdrenoFirmware

Adreno firmware and HFI protocol support.

GMU cold start and HFI work on sheng hardware.

`AdrenoHfiEncodeA8xxPerformance` prepares the version-0 A8xx performance
table for `AdrenoHfiSend`. It serializes up to 32 GX and six CX levels from
caller-supplied verified OPP and command-db votes. The maximum payload is
114 dwords, excluding the HFI header. The existing A6xx/A7xx startup
runner keeps its own format. A8xx power, bandwidth configuration, message
submission and firmware responses require platform qualification.
[Linux performance-table encoder](https://github.com/torvalds/linux/blob/9d66f26fefacebfeed15de31c1123451a17ff119/drivers/gpu/drm/msm/adreno/a6xx_hfi.c) ·
[Linux HFI definitions](https://github.com/torvalds/linux/blob/9d66f26fefacebfeed15de31c1123451a17ff119/drivers/gpu/drm/msm/adreno/a6xx_hfi.h) ·
[Linux GMU level limits](https://github.com/torvalds/linux/blob/9d66f26fefacebfeed15de31c1123451a17ff119/drivers/gpu/drm/msm/adreno/a6xx_gmu.h)

`AdrenoHfiA8xxProgramTables` runs the first two commands in the A8xx
startup sequence. It waits for the performance table ACK before sending
the 159-dword bandwidth payload. The caller supplies the full fixed CNOC
and DDR matrices, with unused entries zeroed. It shares the HFI queues,
deadline handling and failure state with the A6xx/A7xx runner. Success
reports both table ACKs. Power, reset, SMMU ownership, and the
platform's OPP and command-db data are prerequisites. ACD, CLX, IFPC,
thinmem configuration and core start remain separate stages.
[Linux HFI startup](https://github.com/torvalds/linux/blob/9d66f26fefacebfeed15de31c1123451a17ff119/drivers/gpu/drm/msm/adreno/a6xx_hfi.c)

Run the host-only reference and bounds checks with Clang.

```sh
clang -std=c11 -Wall -Wextra -Werror -g -fsanitize=address,undefined \
  AdrenoHfiA8xx.c test_a8xx_performance.c -o /tmp/test-a8xx-performance
/tmp/test-a8xx-performance
clang -target arm64e-apple-macos26.5 -std=c11 -Wall -Wextra -Werror \
  -O2 -ffreestanding -mgeneral-regs-only -c AdrenoHfiA8xx.c \
  -o /tmp/AdrenoHfiA8xx.arm64e.o
```

Run the software GMU peer on a little-endian AArch64 host.

```sh
clang -std=c11 -Wall -Wextra -Werror -g -fsanitize=address,undefined \
  AdrenoHfi.c AdrenoHfiStartup.c AdrenoHfiA8xx.c AdrenoHfiA8xxStartup.c \
  test_a8xx_startup.c -o /tmp/test-a8xx-startup
/tmp/test-a8xx-startup
```

Build and interface details: [documentation](https://unixnu.com/drivers/adreno-firmware). Component sources and dependencies: [catalog](https://unixnu.com/reference/components).
