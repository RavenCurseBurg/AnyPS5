# Two Windows adapter results for #27379

I built the two failing AGC targets from the same unmodified main commit, `cf76855f20`, and ran them individually on two other Windows adapters. These were the actual execution targets. Every run logged the requested GPU. I used Windows 11, WinLibs GCC 15.2 Release, `ANYPS5_REQUIRE_VULKAN=1` and `ANYPS5_NO_SHADER_CACHE=1`.

| Adapter | Validation | Linear target import creation | Store over untracked memory |
| --- | --- | ---: | ---: |
| RTX 4060 (595.79) | on | 3 fail | 3 fail |
| RTX 4060 (595.79) | off | 3 fail | 3 fail |
| UHD 770 (101.5973) | on | 3 pass | 3 pass |
| UHD 770 (101.5973) | off | 3 pass | 3 pass |

Each cell contains 3 separate CTest executions. The validation-on logs show the Khronos layer inserted (1.4.369). I found no VUID diagnostics in these runs.

On the RTX 4060 the import test reported the same private-gate message as on the RTX 3090 Ti, and the store test reported the same overwrite of CPU-written bytes. Each failure repeated in all six runs of that test, with the validation layer both on and off. The UHD 770 passed all six runs of each test on the same Windows installation and binaries. Neither adapter logged a synchronization hazard.

The configured execution-source list was `F16ToF32Unpack;F64FmaSubnormals;RegistrationPrepareShutdown;LinearTargetImportCreation;StoreOverUntracked`. Build `agc_driver_linear_target_import_creation_tests` and `agc_driver_store_over_untracked_tests` from the issue's baseline commit. With each adapter selected through `ANYPS5_GPU`, run the following CTest command three times, first with `VK_INSTANCE_LAYERS=VK_LAYER_KHRONOS_validation` and `VK_LOADER_DEBUG=layer`, then with both unset. Supply the validation layer's directory through `VK_LAYER_PATH` when required.

```text
ctest --test-dir build -V -j1 --no-tests=error -R "^(agc_driver_linear_target_import_creation|agc_driver_store_over_untracked)$"
```

The JSON contains the executable hashes, per-test device selection, validation state, outcome and diagnostic from the full verbose CTest log. CTest's JUnit result supplies the pass/fail outcome. The short JUnit log for successful tests can omit the device line.
