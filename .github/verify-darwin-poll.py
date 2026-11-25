import os
from pathlib import Path
import subprocess
import sys

directory = Path("core/libs/tests")
poll = directory / "GuestPoll.cpp"
forward = directory / "GuestNetKernelForwards.cpp"
mode = sys.argv[1]

if mode == "prepare":
    trace = r'''
static int ObservedPoll(GuestPollDescriptor* entries, std::uint32_t count, int timeout) {
    const int result = poll_nid_postfix(entries, count, timeout);
    if (entries && count) std::fprintf(stderr, "events=%#x timeout=%d result=%d revents=%#x\n", unsigned(entries[0].events), timeout, result, unsigned(entries[0].revents));
    return result;
}
'''
    source = poll.read_text().replace("poll_nid_postfix(", "ObservedPoll(")
    marker = "static void Require(bool value) { if (!value) std::abort(); }"
    assert source.count(marker) == 1
    source = source.replace(marker, trace + '\n#define Require(value) do { if (!(value)) { std::fprintf(stderr, "guest_poll assertion: %s\\n", #value); std::abort(); } } while (false)')
    condition = "Require(poll_nid_postfix(&wait, 1, 0) == 1 && wait.revents == Hup);".replace("poll_nid_postfix","ObservedPoll")
    start = source.index("static void PollsAResetStream()")
    stop = source.index("\n}", start)
    original = source[start:stop]
    assert original.count(condition) == 1
    source = source[:start]+original.replace(condition, 'if (!std::getenv("OMIT_ZERO_INTEREST")) '+condition)+source[stop:]
    cases = {"reset":"PollsAResetStream", "fifo":"PollsIgnoringEof", "queued":"PollsIgnoringEofByWhatIsQueued"}
    source = source.replace("int main() {", "int main(int argc, char** argv) {\n"+ "".join(f'    if (argc == 2 && std::strcmp(argv[1], "{key}") == 0) {{ {name}(); return 0; }}\n' for key,name in cases.items()))
    poll.write_text(source)
    source = forward.read_text().replace("poll_nid_postfix(", "ObservedPoll(")
    offset = source.index("#define Require(")
    source = source[:offset] + trace + source[offset:]
    assert source.count("Require(shutdown_nid_postfix(stream.accepted, 2) == 0);") == 1
    source = source.replace("Require(shutdown_nid_postfix(stream.accepted, 2) == 0);", 'Require(shutdown_nid_postfix(stream.accepted, std::getenv("SHUT_REMAINING_HALF") ? 1 : 2) == 0);')
    source = source.replace("int main() {", 'int main(int argc, char** argv) {\n    if (argc == 2) { CheckPollIgnoringEof(std::strcmp(argv[1], "sceNet") == 0); return 0; }')
    forward.write_text(source)

elif mode == "run":
    commands = [
        ("guest-poll/reset", "guest_poll_tests", "reset", {}),
        ("guest-poll/reset-remaining-assertions", "guest_poll_tests", "reset", {"OMIT_ZERO_INTEREST":"1"}),
        ("guest-poll/fifo", "guest_poll_tests", "fifo", {}),
        ("guest-poll/queued", "guest_poll_tests", "queued", {}),
        ("kernel/shutdown", "guest_net_kernel_forwards_tests", "kernel", {}),
        ("sceNet/shutdown", "guest_net_kernel_forwards_tests", "sceNet", {}),
        ("kernel/shutdown-remaining-half", "guest_net_kernel_forwards_tests", "kernel", {"SHUT_REMAINING_HALF":"1"}),
        ("sceNet/shutdown-remaining-half", "guest_net_kernel_forwards_tests", "sceNet", {"SHUT_REMAINING_HALF":"1"}),
    ]
    for description, target, arg, env in commands:
        binary, = (p for p in Path("build").rglob(target) if p.is_file() and os.access(p, os.X_OK))
        for iteration in range(1, 4):
            result = subprocess.run([str(binary.resolve()), arg], env=os.environ | env, timeout=35, stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
            print(f"{description} trial={iteration} status={result.returncode}", flush=True)
            if iteration == 1:
                print(result.stdout.decode(errors="replace")[-4900:], flush=True)
