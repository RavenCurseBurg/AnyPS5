import builtins
import runpy
import shlex
import sys

builtins.exit = sys.exit
icu_python_path = sys.argv.pop(1)
paths = sys.argv[1:5]
del sys.argv[1:5]
sys.path.insert(0, icu_python_path)

import icutools.databuilder.renderers.common_exec as common_exec

original_run_shell_command = common_exec.run_shell_command


def normalize(path, platform):
    return path.replace("/", chr(92)) if platform == "windows" else path


def quote(path, platform):
    value = normalize(path, platform)
    return '"' + value + '"' if platform == "windows" else shlex.quote(value)


def run_shell_command(command_line, platform, verbose):
    for path in sorted(paths, key=len, reverse=True):
        command_line = command_line.replace(normalize(path, platform), quote(path, platform))
    return original_run_shell_command(command_line, platform, verbose)


common_exec.run_shell_command = run_shell_command
runpy.run_module("icutools.databuilder", run_name="__main__")
