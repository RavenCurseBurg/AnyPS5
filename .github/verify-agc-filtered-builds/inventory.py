"""Describe the filtered executables and their direct CTests in an AnyPS5 Ninja build."""
import argparse
from concurrent.futures import ThreadPoolExecutor
import json
from pathlib import Path
import re
import subprocess
import sys
import xml.etree.ElementTree as ET

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument("build", type=Path)
parser.add_argument("output", type=Path)
parser.add_argument("--verify", action="store_true", help="run each built executable and its registered CTests")
args = parser.parse_args()
build = args.build.resolve()
out = args.output.resolve()
out.mkdir(parents=True, exist_ok=True)
cache = (build / "CMakeCache.txt").read_text(encoding="utf8")
source = Path(re.search(r"^CMAKE_HOME_DIRECTORY:INTERNAL=(.+)$", cache, re.M)[1])
sys.path.insert(0, str(source / "tools"))
import build_changed_guest_tests as guest

root, targets = guest.host_only.read_reply(build)
stub = root / "core/libs/prx/libSceAgcDriver/tests/ExecutionTestNotBuilt.cpp"
artifacts = {}
for target in targets.values():
    if target.get("type") != "EXECUTABLE":
        continue
    if any(not src.get("isGenerated") and root / src["path"] == stub for src in target.get("sources", [])):
        artifact, = (item for item in target["artifacts"] if Path(item["path"]).name == target["nameOnDisk"])
        artifacts[target["name"]] = (build / artifact["path"]).resolve()
if not artifacts:
    raise SystemExit("No filtered executables found. Configure with ANYPS5_AGC_EXECUTION_TESTS and the codemodel-v2 query.")
paths = set(artifacts.values())
registered = guest.registered_test_commands(build)
selected = sorted(name for name, cmd in registered.items() if cmd and Path(cmd[0]).is_absolute() and Path(cmd[0]).resolve() in paths)
assert selected
ninja = (build / "build.ninja").read_text(encoding="utf8")
compile_edges = [line for line in ninja.splitlines() if line.startswith("build ") and "CXX_COMPILER" in line
                 and re.search(r"/ExecutionTestNotBuilt\.cpp\.(?:obj|o):",line)]
assert compile_edges
names = sorted(artifacts)
assert all(re.fullmatch(r"[A-Za-z0-9_]+",name) for name in names)
(out/"filtered.ninja").write_text("include build.ninja\nbuild all-filtered-executables: phony "+" ".join(names)+"\n",encoding="utf8")
(out/"executables.txt").write_text("".join(artifacts[n].as_posix()+"\n" for n in names),encoding="utf8")
(out/"ctest-names.txt").write_text("\n".join(selected)+"\n",encoding="utf8")
result = dict(filtered_executables=len(names),placeholder_compile_edges=len(compile_edges),ctest_registrations=len(selected))

if args.verify:
    def verify_executable(path):
        process = subprocess.run([path],capture_output=True,text=True,timeout=30)
        if process.returncode != 77 or process.stdout.strip() != "skipped, this build leaves the test out with ANYPS5_AGC_EXECUTION_TESTS":
            raise RuntimeError(f"{path.name} returned {process.returncode}, stdout={process.stdout!r}")

    with ThreadPoolExecutor(max_workers=4) as pool:
        list(pool.map(verify_executable, paths))
    if sys.platform == "win32":
        subprocess.run(["cmake","-DEXECUTABLES="+str(out/"executables.txt"),"-P",root/"cmake/ExecutableManifests.cmake"],check=True)
    junit = out/"ctest.xml"
    with (out/"ctest.log").open("wb") as stream:
        subprocess.run(["ctest","--test-dir",build,"--tests-from-file",out/"ctest-names.txt","--parallel","4",
                        "--no-tests=error","--output-on-failure","--output-junit",junit],stdout=stream,stderr=subprocess.STDOUT,check=True)
    cases = {case.get("name"):case for case in ET.parse(junit).iter("testcase")}
    assert set(cases)==set(selected) and all(case.find("skipped") is not None for case in cases.values())
    result.update(direct_skip_exit_codes=len(paths),ctest_skips=len(cases))
print(json.dumps(result,indent=2))
