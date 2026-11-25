# Native multisample draw replay measurements

The base is `6b59b8f5e76c8c0a431df66e981097998b3754e3` and the candidate is `fb12b1926065ab4fd4b4faa96c31a41c15adac4a`. Both ran on Windows build 26100, on an RTX 4060 (595.79) and a UHD 770 (101.5973). The workloads use the project's shaders and synthetic guest commands.

## Pixel and lifetime checks

Build `agc_driver_target_memo_tests` on the candidate and run `ctest -R "^agc_driver_multisample_color_recipe$" --output-on-failure` in the build directory. Set `ANYPS5_REQUIRE_VULKAN=1`, select a GPU through `ANYPS5_GPU` and enable `VK_INSTANCE_LAYERS=VK_LAYER_KHRONOS_validation`. On the base, copying the candidate's TargetStateMemo.cpp into the source tree and running the same executable with `--r3 multisample-recipe` fails at the first recipe creation assertion on each GPU. Each arm's GPU directory includes the results for the surrounding 11 test executables.

## Direct recording benchmark

In scratch checkouts of each revision, use the candidate's `core/libs/prx/libSceAgcDriver/tests/TargetStateMemo.cpp`, then apply `benchmark.patch` with `git apply`. Build `agc_driver_target_memo_tests` against the corresponding revision's libraries. Run with `--bench-msaa MODE`, selecting each of `ordinary`, `recipe`, `mrt-ordinary`, `mrt-recipe` and `single-sample-recipe`. The MRT workload preserves a second attachment with writes disabled. All workloads verify readback.

The timing runs used `APS5_DRAW_PIPELINE=1`, `APS5_NO_PIPELINE_JOURNAL=1`, `ANYPS5_NO_SHADER_CACHE=1` and `ANYPS5_REQUIRE_VULKAN=1`. Each process recorded 24 batches of 128 draws. The first three batches warmed the pipeline. Each cell in `summary.json` covers three processes and 8,064 measured draws. Parent and candidate processes were interleaved. `measurements.json` holds the batch observations. Recording time stops before the wait; total time includes it.

The additional single-sample experiment used the same executable and runtime libraries in both arms, changing only libSceAgcDriver. Processes were pinned to the 16 performance-core logical processors. The `single-control` directory contains binary hashes, four A/A and twelve A/B blocks per device, and the paired ratio with a 95% block-bootstrap interval.

## Command-stream replay and trace

Apply `pm4.patch` in each scratch checkout and build `agc_driver_draw_cache_relocation_tests`. Run that executable with `ANYPS5_REQUIRE_VULKAN=1` and select the GPU with `ANYPS5_GPU`. Set `EXPECT_RECIPE_HITS=1` on the candidate and leave it unset on the base. Run once with `APS5_DRAW_PIPELINE=1` and once with it unset. The fixture submits guest PM4 through sceAgcDriverSubmitDcb and verifies 64 cache hits under relocation. The validation runs used Khronos layer 1.4.369.

For the trace, enable the draw pipeline, set `APS5_TRACE_FRAMES=3:10` and set `APS5_TRACE_DIR` to the output directory. The fixture submits 18 batches of 128 draws and marks each batch boundary with TracePresent. It checks the recipe count for all 16 steady batches. The ten captured intervals measure synthetic queue submissions. Each GPU directory contains the original compressed trace, its summary line and the JSON output of `tools/trace_digest.py`. The dirty flag records the diagnostic fixture patches. The timings do not cover a game scene or display presentation.
