# MSAA replay after the upstream draw key and texture ownership changes

Windows results compare upstream `80781a0e9407b5b93b8b5535c6c2fa101092a0b1` with `6344fbd21396d18234ef197c337cae441f71f20b`. The values are median CPU recording time in µs/draw for a synthetic 4× MSAA scene, ordered from upstream to candidate.

| Adapter | One color attachment | Two color attachments |
| --- | ---: | ---: |
| NVIDIA RTX 4060 | 1.85 → 1.20 | 2.27 → 1.50 |
| Intel UHD 770 | 1.82 → 1.18 | 2.24 → 1.46 |

Each cell covers 8,064 measured draws per revision across three processes. Each process verified readback. The JSON files include the warm-up-excluded batches, ordinary and single-sample controls, and binary hashes. Follow the direct recording benchmark instructions in the parent directory, using these revisions and the same `benchmark.patch`. The resulting fixture's SHA-256 is `a137a6fb7850ba162e4620b1ddc88aef6eac941af994dce99f8bc2e72a015547`.

Across the 11 surrounding Vulkan test executables, all 151 shared CTests matched their baseline outcome on each adapter. The additional recipe regression passed on both. The suite comparison lists skips, baseline failures and validation diagnostics. Timing a game scene on the reporting RX 6700 XT remains pending.
