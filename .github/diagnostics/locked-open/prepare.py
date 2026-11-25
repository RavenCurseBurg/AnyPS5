from pathlib import Path
import subprocess
import sys

implementation, fixture = sys.argv[1:]
source = "core/libs/prx/libkernel/File/src/Open.cpp"
tests = "core/libs/tests"


def blob(ref, path):
    return subprocess.check_output(["git", "show", f"{ref}:{path}"]).decode()


Path(source).write_text(blob(implementation, source))
fs = blob(fixture, tests + "/GuestFilesystem.cpp")
ks = blob(fixture, tests + "/GuestKernelSys.cpp")
first = fs.index('    {\n        const auto lockedFile = root / "open-lock.txt";')
end = fs.index('        Require(unlink_nid_postfix(lockedName.c_str()) == 0);\n    }', first)
end = fs.index("    }", end) + 5
at_start = ks.index("    constexpr int sharedNonblocking", ks.index("    const int aliased = _openat_nid_postfix"))
at_end = ks.index("    const int aliasMade = _openat_nid_postfix", at_start)
headers = '#include "prx/libkernel_sys/include/Api.hpp"\n' + fs[:fs.index("static bool ChflagsThrows")]
setup = '''
static int Error() { return *__error_nid_postfix(); }
static void exercise() {
    const auto root = std::filesystem::path("guest-lock-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    const auto rootName = root.string();
    const auto missingName = (root / "missing").string();
    Require(mkdir_nid_postfix(rootName.c_str(), 0700) == 0);
'''
openat_setup = '''
    const auto file = (root / "file.txt").string();
    std::ofstream(file).put('x');
    const int base = sceKernelOpen(rootName.c_str(), SCE_KERNEL_O_RDONLY | SCE_KERNEL_O_DIRECTORY, 0);
    Require(base >= 0);
'''
end_main = '''
    Require(sceKernelClose(base) == 0);
    Require(unlink_nid_postfix(file.c_str()) == 0 && rmdir_nid_postfix(rootName.c_str()) == 0);
}
int main() {
    try {
        exercise();
        std::puts("guest open, _open, sceKernelOpen, openat and _openat locking assertions passed");
        return 0;
    } catch (const std::runtime_error& e) {
        std::fprintf(stderr, "guest locking exception: %s\\n", e.what());
        return 85;
    }
}
'''
Path(tests, "GuestLockedOpenFocused.cpp").write_text(headers + setup + fs[first:end] + openat_setup + ks[at_start:at_end] + end_main)
cmake = Path("core/libs/prx/libkernel/CMakeLists.txt")
target = "guest_locked_open_focused"
if target not in cmake.read_text():
    with cmake.open("a") as f:
        f.write('''
add_test_executable(guest_locked_open_focused ${LIBS_INCLUDE_DIR}/tests/GuestLockedOpenFocused.cpp)
target_include_directories(guest_locked_open_focused PRIVATE ${LIBS_INCLUDE_DIR})
target_link_libraries(guest_locked_open_focused PRIVATE libkernel_sys libkernel libc)
add_test(NAME guest_locked_open_focused COMMAND guest_locked_open_focused)
set_tests_properties(guest_locked_open_focused PROPERTIES TIMEOUT 25)
''')
print("Implementation", implementation, "source fixtures", fixture, "filesystem and openat assertions",
      fs[first:end].count("Require("), ks[at_start:at_end].count("Require("), flush=True)
