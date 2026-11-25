from pathlib import Path
import os
import subprocess

here = Path(__file__).resolve().parent
output = Path(os.environ["RUNNER_TEMP"])
kernel = "core/libs/prx/libkernel/System/src/Process.cpp"
libc = Path("core/libs/prx/libc/src")


def definition(source, signature):
    start = source.index("\n" + signature) + 1
    end = source.index("\n}", start) + 2
    return source[start:end]


def read_revision(sha, path):
    return subprocess.check_output(["git", "show", sha + ":" + path], text=True)


def checked(*args):
    subprocess.run(args, check=True, timeout=120)


arena = (libc / "GuestArena.cpp").read_text()
process = (libc / "Process.cpp").read_text()
headers = """
#include "prx/libc/include/Api.hpp"
#include "prx/libc/include/GuestArena.hpp"
#include "prx/libc/include/GuestWriteWatch.hpp"
#include "prx/libkernel/include/Api.hpp"
#include "prx/libkernel/System/include/ProcessWait.hpp"
#include <atomic>
#include <cerrno>
#include <limits>
#include <stdexcept>
#include <system_error>
#include <mach/mach.h>
#include <mach/mach_vm.h>
#include <sys/resource.h>
#include <sys/wait.h>
#include <unistd.h>
"""
for name, sha in [("review", "c00db61baa9dd1799f41c1e0c4241a5460aeb9f3"), ("parent", "098c73473f2d4111bc05cf70ac8d1279afc5dab1")]:
    src = read_revision(sha, kernel)
    pieces = [headers, definition(process, "int* APS5_VABI __error_nid_postfix("), "namespace GuestArena {",
              next(line for line in arena.splitlines() if "hostWriteHook{" in line),
              definition(arena, "bool GuestArenaBeginHostWrite_nid_postfix("),
              definition(arena, "void GuestArenaEndHostWrite_nid_postfix("), "}"]
    for key in ["errnoInvalidArgument", "freebsdWaitOptions", "guestFault", "guestInvalid"]:
        pieces.append(next(line for line in src.splitlines() if line.startswith("constexpr ") and " " + key + " =" in line))
    for key in ["bool GuestRange(", "bool WriteGuest(", "static GuestResourceUsage ResourceUsage(", "int WaitProcess_nid_no_patch(", "int APS5_VABI getrusage_nid_postfix("]:
        pieces.append(definition(src, key))
    translation = output / (name + "-accounting.cpp")
    translation.write_text("\n".join(pieces))
    obj = output / (name + "-accounting.o")
    watch = output / "watch.o"
    flags = ["clang++", "-arch", "x86_64", "-std=c++20", "-O2", "-Icore/libs"]
    if not watch.is_file():
        checked(*flags, "-c", libc / "GuestWriteWatch.cpp", "-o", watch)
    checked(*flags, "-c", translation, "-o", obj)
    probe = output / (name + "-probe")
    checked(*flags, here / "sample.cpp", obj, watch, "-o", probe)
    for repetition in range(3):
        result = subprocess.run([probe], timeout=30)
        expected = 0 if name == "review" else 2
        if result.returncode != expected:
            raise RuntimeError((name, repetition, result.returncode, expected))
    if name == "review":
        test = output / "guest-getrusage"
        checked(*flags, "core/libs/tests/GuestGetrusage.cpp", obj, watch, "-o", test)
        result = subprocess.run([test], timeout=30)
        print("Standalone stock GuestGetrusage exit:", result.returncode, flush=True)
        fixture = Path("core/libs/tests/GuestGetrusage.cpp").read_text()
        marker = "static void Require(bool value) { if (!value) std::abort(); }"
        assert marker in fixture
        fixture = "#include <cstdio>\n#include <source_location>\n" + fixture.replace(marker,
            'static void Require(bool value, std::source_location at=std::source_location::current()) { if (!value) { std::fprintf(stderr,"accounting assertion %s:%u\\n",at.function_name(),at.line()); std::abort(); } }')
        fixture = fixture.replace("    Require(MaxResident(0) >= beforeTouch", '    std::fprintf(stderr, "allocation before=%lld after=%lld\\n", (long long)beforeTouch, (long long)MaxResident(0));\n    Require(MaxResident(0) >= beforeTouch')
        lower = fixture.replace("Require(MaxResident(0) >= beforeTouch + 60000);", "const auto afterTouch = MaxResident(0);\n    Require(afterTouch >= beforeTouch && afterTouch >= 60000);")
        touched = lower.replace("    Require(touched[touched.size() / 2] == 1);",
            "    volatile unsigned char* pages = touched.data();\n    for (std::size_t i = 0; i < touched.size(); i += 4096) pages[i] = 1;")
        assert fixture != lower != touched
        for variant, source in [("diagnostic", fixture), ("lower", lower), ("touched", touched)]:
            candidate = output/(variant+".cpp")
            candidate.write_text(source)
            checked(*flags, candidate, obj, watch, "-o", test)
            for i in range(3):
                result = subprocess.run([test], timeout=30)
                print("Darwin stock fixture variant", variant, i, "exit", result.returncode, flush=True)
