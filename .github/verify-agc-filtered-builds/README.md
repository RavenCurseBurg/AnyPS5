# Compiling the placeholder for filtered AGC execution tests

The Windows CI filter enables three of the 736 execution tests in this source tree. Main at `cf76855f205da53a08e86feea930c75f7d2f217c` builds the other 733 executables from the same C++ source. Candidate `4c06c7c4b59418b97f3123a6d62aefe1214f69dd` keeps all 733 executables and shares one compiled object among them.

| Check on Windows 11, MinGW GCC 15.2, Ninja, CMake 4.3.1 | Base | Candidate |
| --- | ---: | ---: |
| Placeholder sources compiled after touching `ExecutionTestNotBuilt.cpp` | 733 | 1 |
| Executables relinked | 733 | 733 |
| Executables returning exit code 77 and the expected skip message | 733 | 733 |
| Executables with the embedded Windows `asInvoker` manifest | 733 | 733 |
| Direct CTest registrations reporting a skip | 1,312 | 1,312 |
| All registered CTests in this build | 7,035 | 7,035 |

The complete registration list matched in order, command and properties. The File API still identified the same 3,428 guest executables and associated the source with every filtered target. The Windows manifest list contained the same 3,582 paths. With all execution tests enabled, neither revision supplied a placeholder object. Selecting a single test produced 735 placeholder objects on the base and one on the candidate. Both revisions rejected an unknown test name. The project registration audit reported no unregistered tests or unbuilt exports.

In the initial full-group run, touching the placeholder and rebuilding took 354.12 seconds on the base and 261.53 seconds on the candidate. Other local verification work ran concurrently. A second set of full-group rebuilds used the order candidate, base, base, candidate. These took 150.26, 267.16, 261.90 and 171.94 seconds, respectively. One untimed setup link exited before the final repetition. The setup was repeated successfully before timing. Each timed build followed a successful build and a verified no-op and relinked all 733 executables at Ninja parallelism 3. These times cover only this group on this workstation. The timing data lists the 24-executable sample separately.

## Reproduce

Start with a normal Release MinGW/Ninja project build, initialized submodules and the toolchain on `PATH`. In each build directory, request CMake File API `codemodel-v2` before configuring. Use the project's usual Windows CI configure options with `BUILD_TESTING=ON` and this cache entry.

```text
ANYPS5_AGC_EXECUTION_TESTS=F16ToF32Unpack;F64FmaSubnormals;RegistrationPrepareShutdown
```

Run the supplied script once to emit the Ninja group and the executable and CTest lists. In this PowerShell example, `$buildDir` and `$reportDir` are absolute paths. Run these commands from the AnyPS5 source tree with the fixture's `inventory.py` in the current directory or adjust its path.

```powershell
python inventory.py $buildDir $reportDir
ninja -C $buildDir -f "$reportDir/filtered.ninja" -j3 all-filtered-executables
ninja -C $buildDir -f "$reportDir/filtered.ninja" -j3 all-filtered-executables
cmake -E touch core/libs/prx/libSceAgcDriver/tests/ExecutionTestNotBuilt.cpp
Measure-Command { ninja -C $buildDir -f "$reportDir/filtered.ninja" -j3 all-filtered-executables }
$env:PATH = "$buildDir/core/libs/libs/unpatched;$env:PATH"
python inventory.py $buildDir $reportDir --verify
```

The second Ninja invocation should compile and link nothing before the timed run. The verification step starts every executable, checks the embedded Windows manifests with the project's checker and runs the corresponding CTest registrations. It checks the JUnit results individually. The evidence includes the full mapping of filtered executables and test names, the timing samples, and the results of 40 selected CTests per GPU on the RTX 4060 and UHD 770. Both revisions had identical outcomes on each adapter.
