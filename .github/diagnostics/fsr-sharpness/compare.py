import itertools
from pathlib import Path
import random
import subprocess
import sys

inputs = ["", "0", "-0", "-0.", ".0", "-.0", "0e-999", "-0e-999", "0.", ".5", "2.", "2e+0",
          "1.2e-20", "1.1e-45", "7e-46", "8e-46", "1e-46", "1e-5000", "-1e-9999",
          "1.000000059604644775390625", "1.0000000596046447753906251",
          "1.0000000596046447753906249", "2.0000001", "2.0000002", "+1", " 1", "1 ", "0x1p0",
          "-0x0p0", "nan", "Infinity", "inf", "NaN(123)", "0e", "0e+", "0e-", "1e", "0,5",
          "1_0", "--.0", "-+0", ".", "-.", "-", "0\t", "00.e+99999", "1e-999999", "0e-999999",
          "1..2", "1e+"]
r = random.Random(5663)
for i in range(2400):
    mantissa = r.choice(["0.", "1.", "2.", "-0.", "", "."]) + "".join(str(r.randrange(10)) for _ in range(r.randrange(1, 68)))
    inputs.append(mantissa + "e" + str(r.randrange(-90, 8)))
for i in range(600):
    inputs.append("".join(r.choice("0123456789eExXpPnNaA.+-, \t") for _ in range(r.randrange(1, 11))))
data = ("\n".join(inputs) + "\n").encode()
path = Path(__file__).with_name("reference.txt")


def results(executable, rounding, locale):
    child = subprocess.run([executable, str(rounding), locale], input=data, capture_output=True, check=True)
    rows = child.stdout.decode().splitlines()
    assert len(rows) == len(inputs), (len(rows), len(inputs), child.stderr)
    return rows


if sys.argv[1] == "--write-reference":
    path.write_text("\n".join(results(sys.argv[2], 0, "classic")) + "\n")
else:
    baseline = path.read_text().splitlines()
    failed = False
    for rounding, locale in itertools.product(range(4), ("classic", "comma")):
        expected = results(sys.argv[2], rounding, locale) if len(sys.argv) > 2 else baseline
        observed = results(sys.argv[1], rounding, locale)
        differences = [(repr(s), a, b) for s, a, b in zip(inputs, expected, observed) if a != b]
        print(rounding, locale, len(inputs), "matched", len(inputs) - len(differences), "differences", len(differences),
              differences[:4], flush=True)
        failed |= bool(differences)
    raise SystemExit(failed)
