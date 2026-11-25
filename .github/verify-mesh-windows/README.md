# Recording a 33-layer mesh draw on Windows

The comparison uses `c032ba7c5273e37018a58f7c4451e6585cbc58c7` (before the window-reuse change) and `d66dbe597981a366c260646ee9b53a6a3cbc39ed` (window reuse with the per-base pipeline). Both use the same layered-volume test source from the latter revision. Apply `measurement.patch` to that source in scratch checkouts and build `agc_driver_layered_volume_draw_tests`.

Run the executable with `--mesh-execution`, `VERIFY_MESH_LIMIT=8`, `16` or `33`, `ANYPS5_GPU=RTX 4060`, `ANYPS5_REQUIRE_VULKAN=1`, `ANYPS5_NO_SHADER_CACHE=1` and `APS5_NO_PIPELINE_JOURNAL=1`. The graphics context retains its real device and recorder and caps the output window for this draw. The physical limit on this GPU is 2048. Windows 11 build 26100, NVIDIA driver 595.79, WinLibs GCC 15.2 Release. Timing runs used no validation layer.

Each process recorded 14 batches of 16 draws, with a wait at the end of each batch. The recording timer stops before the wait. The first three batches were discarded. Scene geometry, shaders and per-slice pixel expectations are identical across both revisions. Each process checked readback after its final batch.

Six fresh processes per revision and window limit gave 1,056 steady draws per cell. Process pairs for the two revisions were interleaved and the order within each pair was balanced. The summary reports the median of per-process recording medians, the median paired ratio and a 95% bootstrap interval over the six pairs. Raw observations include both recording and wait-inclusive times. The measurements cover the synthetic draw on one GPU.
