# Locked-open truncation error on Windows

This diagnostic exercises the review of `27fa36efd5c02b0d386622ad3d77b03a071d5a20` in AnyPS5 #27300. The patches are test fixtures for a scratch checkout. They use the real regular-file open, exclusive lock, close and descriptor bookkeeping.

On the unchanged revision, build `guest_filesystem_tests`, `guest_kernel_sys_tests` and `guest_kernel_app_archive_tests`. Run the four cases with the following command in a configured Windows build directory.

```sh
ctest -R '^guest_(filesystem|kernel_sys|kernel_app_archive_(archive|plain))$' --repeat until-fail:3 --output-on-failure
```

Apply `reviewed-probe.patch` to the reviewed revision. Rebuild `guest_filesystem_tests` and run `ctest -R '^guest_filesystem$' --repeat until-fail:3 -V`. The test creates a six-byte regular file and arms a marker around two calls. The Windows truncation backend then returns `ENOSPC` and the real close finishes by writing guest `EBADF`. The kernel open returned `0x8002001c`. The POSIX open returned `-1` with guest errno 28. The descriptor was immediately reusable for an exclusive lock and the file retained all six bytes in all three runs.

Then apply `late-read-control.patch` on top and repeat the build and test. The control reads guest errno after closing and the same assertion fails. The kernel open returned `0x80020009` and the POSIX open reported 9. The descriptor and file checks still passed. The runner used Windows 11 with MinGW GCC 15.2.
