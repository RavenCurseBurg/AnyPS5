"""Compile the PR's Darwin poll predicates verbatim against the runner's real sockets and pipes."""
from pathlib import Path
import os
import subprocess

here=Path(__file__).resolve().parent
out=Path(os.environ["RUNNER_TEMP"])
lib=Path("core/libs/prx/libkernel/Socket/src")
def function(src, signature):
    start=src.index("\n"+signature)+1
    end=src.index("\n}",start)+2
    return src[start:end]
udp=(lib/"Udp.cpp").read_text()
opts=(lib/"SocketOptions.cpp").read_text()
prefix="""
#include <libproc.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <netinet/in.h>
#include <fcntl.h>
#include <unistd.h>
#include <poll.h>
#include <cerrno>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <stdexcept>
using NativeSocket = int;
constexpr short NativePollIn = POLLIN;
int NativeError() { return errno; }
"""
code=[prefix,"namespace GuestSockets {",function(opts,"bool NativeListening("),"}",
      function(udp,"static bool PollDataQueued("),function(udp,"static bool SocketErrorPending("),
      (here/"predicates.cpp").read_text()]
source=out/"native-poll-predicates.cpp"
source.write_text("\n".join(code))
binary=out/"native-poll-predicates"
subprocess.run(["clang++","-std=c++20","-O2","-Wall",source,"-o",binary],check=True)
for i in range(5):
    subprocess.run([binary],check=True,timeout=20)
