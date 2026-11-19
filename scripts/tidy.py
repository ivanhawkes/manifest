#!/usr/bin/env python3
"""Run clang-tidy on project sources, excluding the fetched engine and dependencies, skipping a unit
that came out clean before and has not changed since.

A unit's key is a digest of what decides clang-tidy's verdict on it: clang-tidy's version,
.clang-tidy, the unit's compile command and the contents of every file it includes, as the build's
dependency record lists them (`ninja -t deps`). Only clean units are recorded, under
<build>/tidy-clean, so a finding is reported on every run until it is fixed. A unit with no record,
or with a file changed since its object was built (whose includes the record may not list), is
always checked. Paths under /nix/store are hashed by name, the store being immutable. The method is
Jettison's scripts/tidy.py.

tidy.py <build directory>: prints each failing unit's output; exits 1 on any failure.
"""
import concurrent.futures
import hashlib
import json
import os
from pathlib import Path
import subprocess
import sys

root = Path(__file__).resolve().parents[1]
build = Path(sys.argv[1]).resolve()
commands = json.loads((build / 'compile_commands.json').read_text())
entries = {entry['file']: entry for entry in commands
           if Path(entry['file']).is_relative_to(root / 'source')}
if not entries:
    sys.exit('No Manifest translation units in the compile database')


def dependency_records():
    """Each object path to the files it was built from, for every record ninja holds as valid."""
    output = subprocess.run(['ninja', '-C', str(build), '-t', 'deps'],
                            capture_output=True, text=True, check=True).stdout
    records = {}
    current = None
    for line in output.splitlines():
        if not line.strip():
            current = None
        elif not line.startswith(' '):
            target, _, status = line.partition(': #deps')
            current = [] if status.rstrip().endswith('(VALID)') else None
            if current is not None:
                records[str((build / target).resolve())] = current
        elif current is not None:
            current.append(line.strip())
    return records


digests = {}


def digest_of(name):
    if name not in digests:
        path = (build / name).resolve()
        if str(path).startswith('/nix/store/'):
            digests[name] = str(path)
        else:
            try:
                digests[name] = str(path) + '\0' + hashlib.sha256(path.read_bytes()).hexdigest()
            except OSError:
                digests[name] = str(path) + '\0missing'
    return digests[name]


def modified(name):
    try:
        return (build / name).stat().st_mtime_ns
    except OSError:
        return None


def unit_key(entry, records, preamble):
    """Nothing when the unit's includes are not known for certain."""
    if 'output' not in entry:
        return None
    output = str(Path(entry['directory'], entry['output']).resolve())
    deps = records.get(output)
    built = modified(output)
    if deps is None or built is None or any(
            (time := modified(dep)) is None or time > built for dep in deps):
        return None
    digest = hashlib.sha256(preamble)
    digest.update(json.dumps(entry.get('arguments') or entry['command']).encode())
    digest.update(entry['file'].encode())
    for dep in deps:
        digest.update(digest_of(dep).encode())
    return digest.hexdigest()


def check(source):
    result = subprocess.run(['clang-tidy', '-p', str(build), '--warnings-as-errors=*', source],
                            stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True)
    if result.returncode:
        return result.stdout
    return ''


version = subprocess.run(['clang-tidy', '--version'], capture_output=True, text=True,
                         check=True).stdout
preamble = (version + (root / '.clang-tidy').read_text()).encode()
records = dependency_records()
clean = build / 'tidy-clean'
clean.mkdir(exist_ok=True)
pending = []
for source, entry in sorted(entries.items()):
    key = unit_key(entry, records, preamble)
    if key is None or not (clean / key).exists():
        pending.append((source, key))

with concurrent.futures.ThreadPoolExecutor(max_workers=int(os.environ.get('MANIFEST_TIDY_JOBS', 2))) as pool:
    failures = list(pool.map(check, [source for source, _ in pending]))
for (source, key), failure in zip(pending, failures):
    if failure:
        print(failure)
    elif key:
        (clean / key).touch()
print(f'tidy: {len(pending)} of {len(entries)} units checked, '
      f'{len(entries) - len(pending)} unchanged since clean')
sys.exit(any(failures))
