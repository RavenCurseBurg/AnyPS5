# Native multisampled draw replay after the DCC import change

These measurements compare main at `65ff3ae335ac7ad0be8c38727a1d1ff799815fbb` with `2e566609eeaeece5236a2ae74a07daf7a76dc8d4` on Windows 11. The test uses project shaders and a synthetic 4× MSAA scene. The two-target case preserves an attachment whose writes are masked.

| GPU and driver | Single color attachment, base → change | Two color attachments, base → change |
| --- | ---: | ---: |
| GeForce RTX 4060, 595.79 | 2.55 → 1.81 µs/draw | 3.03 → 2.06 µs/draw |
| Intel UHD 770, 101.5973 | 2.61 → 1.99 µs/draw | 3.51 → 2.12 µs/draw |

The table gives median CPU recording time per draw. Each cell contains 8,064 measured draws per revision, collected in three independently started processes. The first three batches warmed the pipeline. The remaining 21 batches of 128 draws contributed the timings. Every process checked readback. Each multisample recipe workload recorded zero hits on the base and 8,064 on the candidate. The JSON includes ordinary draw controls and total time through the GPU wait.

The original 1× fast path has a separate driver-only comparison. Across 12 interleaved four-process blocks per adapter, the candidate/base geometric mean recording-time ratio was 1.012 on NVIDIA (95% bootstrap interval 0.935–1.097) and 0.954 on Intel (0.878–1.042). Four identical-binary control blocks per adapter gave 1.042 and 0.883 respectively. The spread and Intel control drift limit interpretation of small differences.

## Reproduction

Use the instructions and two fixture patches in the parent directory. Applying those patches to the revisions above reproduces the sources used here. The resulting direct benchmark source starts with SHA-256 `7b52423c`. Each arm's `binary.json` records the hashes of the test executable and libraries. The `single-control` directory contains the driver-only binary hashes, CPU affinity, individual process medians and paired intervals.

For command submission, each GPU ran both inline and pipelined PM4 draws under the Khronos validation layer. All eight runs produced the expected image and 64 relocated draw-cache hits. The base had zero recipe hits and the candidate had 64. Both GPU/arm directories include the pipelined trace, the emulator summary and the digest. The captures contain only the synthetic workload.

The two revisions had identical outcomes on all 145 shared CTests per adapter. The additional recipe case passed on both adapters. The RTX 4060 had one pre-existing failure and two skips. The UHD 770 had three pre-existing failures and one skip. The suite comparison lists their names. Apply `texture-replay/regression.patch` to the candidate to exercise texture retirement and reacquisition at the same guest address. Build `agc_driver_sample_multisample_target_tests` and run the three `agc_driver_texture_dcc_target_[248]` cases with `ANYPS5_REQUIRE_VULKAN=1`, the chosen `ANYPS5_GPU` and Khronos validation. Each case passed twice on both adapters.
