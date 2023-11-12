# Copyright (C) 2023  Tuomo Kriikkula
# This program is free software: you can redistribute it and/or modify
#     it under the terms of the GNU Lesser General Public License as published
# by the Free Software Foundation, either version 3 of the License, or
# (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
#     but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU Lesser General Public License for more details.
#
# You should have received a copy of the GNU Lesser General Public License
#     along with this program.  If not, see <https://www.gnu.org/licenses/>.
import argparse
import asyncio
import enum
import glob
import json
import os
import re
import shutil
import sys
import threading
import time
from dataclasses import asdict
from dataclasses import dataclass
from dataclasses import field
from pathlib import Path
from traceback import print_exc
from typing import IO

import httpx
import py7zr
import tqdm
import watchdog.events
import watchdog.observers
from udk_configparser import UDKConfigParser

import defaults

# TODO: rename this script.
# TODO: leverage pytest?

SCRIPT_DIR = Path(__file__).parent
CACHE_DIR = SCRIPT_DIR / ".cache/"
UMB_TESTS_TCP_LINK_PATH = SCRIPT_DIR / "UMBTestsTcpLink.uc"
UMB_TESTS_MACROS_PATH = SCRIPT_DIR / "UMBTestsMacros.uci"

UDK_TEST_TIMEOUT = defaults.UDK_TEST_TIMEOUT

LOG_RE = re.compile(r"^\[[\d.]+]\s(\w+):(.*)$")


class State(enum.StrEnum):
    NONE = enum.auto()
    BUILDING = enum.auto()
    TESTING = enum.auto()


class LogWatcher(watchdog.events.FileSystemEventHandler):
    def __init__(
            self,
            building_event: threading.Event,
            testing_event: threading.Event,
            log_file: Path,
    ):
        self._building_event = building_event
        self._testing_event = testing_event
        self._log_file = log_file
        self._log_filename = log_file.name
        self._fh: IO | None = None
        self._pos = 0
        self._state = State.NONE
        self._warnings = []
        self._errors = []

    @property
    def warnings(self) -> list[str]:
        return self._warnings

    @property
    def errors(self) -> list[str]:
        return self._errors

    @property
    def state(self) -> State:
        return self._state

    @state.setter
    def state(self, state: State):
        print(f"setting state: {state}")
        self._state = state

    def on_any_event(self, event: watchdog.events.FileSystemEvent):
        if Path(event.src_path).name == self._log_filename:
            print(f"fs event: {event.event_type}, {event.src_path}")
            self._fh = open(self._log_file, errors="replace")

    # TODO: better state handling, this is a mess ATM.
    def on_modified(self, event: watchdog.events.FileSystemEvent):
        path = Path(event.src_path)
        if path.name == self._log_filename:
            if not self._fh:
                raise RuntimeError("no log file handle")

            size = self._log_file.stat().st_size
            if size == 0:
                print("log file cleared")
                self._pos = 0
                return

            self._fh.seek(self._pos)

            log_end = False
            # TODO: detect partial lines. Just read the entire
            #   file again in the end to get fully intact data?
            while line := self._fh.readline():
                self._pos = self._fh.tell()

                if match := LOG_RE.match(line):
                    if match.group(1).lower() == "error":
                        if match.group(2):
                            self._errors.append(line)
                    elif match.group(1).lower() == "warning":
                        if match.group(2):
                            self._warnings.append(line)

                print(line.strip())

                if self._state == State.BUILDING:
                    if "Log file closed" in line:
                        log_end = True
                elif self._state == State.TESTING:
                    if "Exit: Exiting" in line:
                        log_end = True

            if log_end:
                print("setting stop event")
                if self._state == State.BUILDING:
                    self._building_event.set()
                elif self._state == State.TESTING:
                    self._testing_event.set()

    def __del__(self):
        if self._fh:
            try:
                self._fh.close()
            except OSError:
                pass


@dataclass
class Cache:
    udk_lite_tag: str = ""
    pkg_archive: str = ""
    pkg_archive_extracted_files: list[str] = field(default_factory=list)


def resolve_script_path(path: str) -> Path:
    p = Path(path)
    if not p.is_absolute():
        p = SCRIPT_DIR / p
    return p.resolve()


def write_cache(file: Path, cache: Cache):
    print(f"writing cache file: '{file}'")
    file.write_text(json.dumps(asdict(cache), indent=2))


def load_cache(path: Path) -> Cache:
    with path.open() as f:
        cache = json.load(f)
    return Cache(**cache)


def download_file(url: str, out_file: Path, progress_bar: bool = True):
    print(f"downloading '{url}'")
    with out_file.open("wb") as f:
        with httpx.stream(
                "GET",
                url,
                timeout=120.0,
                follow_redirects=True) as resp:
            resp.raise_for_status()
            total = int(resp.headers["Content-Length"])

            # TODO: refactor this.
            if progress_bar:
                with tqdm.tqdm(total=total, unit_scale=True, unit_divisor=1024,
                               unit="B") as progress:
                    num_bytes_downloaded = resp.num_bytes_downloaded
                    for data in resp.iter_bytes(chunk_size=4096):
                        f.write(data)
                        progress.update(resp.num_bytes_downloaded - num_bytes_downloaded)
                        num_bytes_downloaded = resp.num_bytes_downloaded

            else:
                for data in resp.iter_bytes(chunk_size=4096):
                    f.write(data)

    print("download finished")


def remove_old_extracted(cache: Cache):
    print("removing old extracted files, if any")

    dirs = []

    for file in cache.pkg_archive_extracted_files:
        p = Path(file).resolve()
        if p.exists() and p.is_file():
            print(f"removing '{p}'")
            p.unlink(missing_ok=True)
        elif p.is_dir():
            dirs.append(p)

    for d in dirs:
        is_empty = not any(d.iterdir())
        if is_empty:
            shutil.rmtree(d)
        else:
            print(f"not removing non-empty directory: '{d}'")

    print("remove_old_extracted done")


def already_extracted(archive_file: str, out_dir: Path, cache: Cache) -> bool:
    cached = cache.pkg_archive_extracted_files

    # Files in the archive are relative to the archive root.
    # Make them relative to the extraction target directory
    # for comparisons. Cached paths are absolute.
    extracted_file = (out_dir / archive_file).resolve()
    is_cached = str(extracted_file) in cached
    return extracted_file.exists() and is_cached


# Convince UDK.exe to flush the log file.
# TODO: this is fucking stupid.
def poke_file(file: Path, event: threading.Event):
    while not event.is_set():
        file.stat()
        time.sleep(1)


async def run_udk_build(
        watcher: LogWatcher,
        udk_lite_root: Path,
        building_event: threading.Event,
        use_shell: bool = False,
) -> int:
    print("starting UDK build phase")

    udk_exe = (udk_lite_root / "Binaries/Win64/UDK.exe").resolve()

    watcher.state = State.BUILDING

    if use_shell:
        proc = await asyncio.create_subprocess_shell(
            cmd=f'powershell.exe Start-Process -NoNewWindow -FilePath "{udk_exe}" '
                f'-ArgumentList "make","-useunpublished","-silent","-UNATTENDED",'
                f'"-VERBOSE","-WAITFORDEBUGSERVER","-NOPAUSE","-FORCELOGFLUSH","-DEBUG",'
                f'"-ResX=0","-ResY=0"',
        )
    else:
        proc = await asyncio.create_subprocess_exec(
            udk_exe,
            *["make", "-useunpublished", "-log", "-UNATTENDED"],
        )

    print(f"proc: {proc}")

    ok = building_event.wait(timeout=UDK_TEST_TIMEOUT)

    if use_shell:
        await (await asyncio.create_subprocess_exec(
            *["taskkill", "/IM", "UDK.exe", "/F"]
        )).wait()

    await (await asyncio.create_subprocess_exec(
        *["taskkill", "/pid", str(proc.pid)]
    )).wait()

    if not ok:
        raise RuntimeError("timed out waiting for UDK.exe (building_event) stop event")

    print("UDK.exe finished")

    ec = await proc.wait()
    print(f"UDK.exe exited with code: {ec}")

    if ec != 0:
        raise RuntimeError(f"UDK.exe error: {ec}")

    return ec


async def run_udk_server(
        watcher: LogWatcher,
        udk_lite_root: Path,
        testing_event: threading.Event,
        udk_args: str,
        use_shell: bool = False,
) -> int:
    print("starting UDK testing phase")

    udk_exe = (udk_lite_root / "Binaries/Win64/UDK.exe").resolve()

    watcher.state = State.TESTING
    if use_shell:
        test_proc = await asyncio.create_subprocess_shell(
            cmd=f'powershell.exe Start-Process -NoNewWindow -FilePath "{udk_exe}" '
                f'-ArgumentList "server","{udk_args}","-silent","-UNATTENDED",'
                f'"-VERBOSE","-WAITFORDEBUGSERVER","-NOPAUSE","-FORCELOGFLUSH","-DEBUG",'
                f'"-ResX=0","-ResY=0"',
        )
    else:
        test_proc = await asyncio.create_subprocess_exec(
            udk_exe,
            *[
                "server",
                udk_args,
                "-UNATTENDED",
                "-log",
            ],
        )

    print(f"proc: {test_proc}")

    ok = testing_event.wait(timeout=UDK_TEST_TIMEOUT)

    if use_shell:
        await (await asyncio.create_subprocess_exec(
            *["taskkill", "/IM", "UDK.exe", "/F"]
        )).wait()

    await (await asyncio.create_subprocess_exec(
        *["taskkill", "/pid", str(test_proc.pid)]
    )).wait()

    if not ok:
        raise RuntimeError("timed out waiting for UDK.exe (testing_event) stop event")

    test_ec = await test_proc.wait()
    print(f"UDK.exe UMB test run exited with code: {test_ec}")

    return test_ec


async def main():
    global UDK_TEST_TIMEOUT

    pwsh_path = Path(r"C:\Windows\System32\WindowsPowerShell\v1.0\powershell.exe")
    print(f"pwsh_path: '{pwsh_path}', exists={pwsh_path.exists()}")

    os.putenv(
        "COMSPEC",
        str(pwsh_path),
    )

    ap = argparse.ArgumentParser()
    ap.add_argument(
        "--no-progress-bar",
        help="disable progress bar",
        action="store_true",
    )
    ap.add_argument(
        "--use-shell",
        help="start subprocesses using shell",
        action="store_true",
    )

    args = ap.parse_args()
    progress_bar = not args.no_progress_bar
    use_shell = args.use_shell

    hard_reset = False
    cache = Cache()

    cache_file = Path(CACHE_DIR / ".cache.json").resolve()
    if cache_file.exists():
        try:
            cache = load_cache(cache_file)
        except Exception as e:
            print(f"error loading cache: {type(e).__name__}: {e}")
            print(f"delete the cache file '{cache_file}' "
                  f"or cache directory '{CACHE_DIR}' to force a hard reset")
            raise
    else:
        hard_reset = True

    if hard_reset:
        print("hard reset requested")
        print(f"removing '{CACHE_DIR}'")
        shutil.rmtree(CACHE_DIR, ignore_errors=True)
        CACHE_DIR.mkdir(parents=True, exist_ok=True)
        write_cache(cache_file, cache)

    UDK_TEST_TIMEOUT = int(os.environ.get("UDK_TEST_TIMEOUT", defaults.UDK_TEST_TIMEOUT))
    udk_lite_tag = os.environ.get("UDK_LITE_TAG", defaults.UDK_LITE_TAG)
    udk_lite_root = Path(os.environ.get("UDK_LITE_ROOT", defaults.UDK_LITE_ROOT))
    udk_lite_release_url = os.environ.get("UDK_LITE_RELEASE_URL", defaults.UDK_LITE_RELEASE_URL)
    uscript_message_files = os.environ.get("USCRIPT_MESSAGE_FILES", defaults.USCRIPT_MESSAGE_FILES)

    if not udk_lite_root.is_absolute():
        udk_lite_root = (SCRIPT_DIR / udk_lite_root).resolve()

    print(f"UDK_LITE_TAG={udk_lite_tag}")
    print(f"UDK_LITE_ROOT={udk_lite_root}")
    print(f"UDK_LITE_RELEASE_URL={udk_lite_release_url}")
    print(f"USCRIPT_MESSAGE_FILES={uscript_message_files}")

    input_script_msg_files = [
        resolve_script_path(path) for path in
        glob.glob(uscript_message_files, recursive=True)
    ]

    if not UMB_TESTS_TCP_LINK_PATH.exists():
        raise RuntimeError(f"'{UMB_TESTS_TCP_LINK_PATH}' does not exist")
    input_script_msg_files.append(UMB_TESTS_TCP_LINK_PATH)

    if not UMB_TESTS_MACROS_PATH.exists():
        raise RuntimeError(f"'{UMB_TESTS_MACROS_PATH}' does not exist")
    input_script_msg_files.append(UMB_TESTS_MACROS_PATH)

    if not input_script_msg_files:
        raise RuntimeError("no input script files found")

    # Check if cache tag, url, etc. match current ones, if not -> reinit.
    # Check if we have all pkg files in place. Compare to cache.

    dl_pkg_archive = False
    pkg_file = CACHE_DIR / Path(udk_lite_release_url).name

    if cache.udk_lite_tag != udk_lite_tag:
        print(f"cached UDK-Lite tag '{cache.udk_lite_tag}' does not match '{udk_lite_tag}'")
        dl_pkg_archive = True

    pkg_file_outdated = cache.pkg_archive != str(pkg_file)
    if pkg_file_outdated:
        dl_pkg_archive = True
        print(f"cached archive '{cache.pkg_archive}' does not match '{pkg_file}'")

    if not pkg_file.exists():
        print(f"'{pkg_file}' does not exist")
        dl_pkg_archive = True

    if dl_pkg_archive:
        download_file(udk_lite_release_url, pkg_file, progress_bar=progress_bar)
        cache.udk_lite_tag = udk_lite_tag
        remove_old_extracted(cache)
        cache.pkg_archive_extracted_files = []
    else:
        print(f"using cached archive: '{pkg_file}'")

    # TODO: make a cache that updates itself automatically
    #   when fields are assigned. Just use diskcache?
    cache.pkg_archive = str(pkg_file)
    write_cache(cache_file, cache)

    print(f"extracting '{pkg_file}'...")
    with py7zr.SevenZipFile(pkg_file) as pkg:
        filenames = pkg.getnames()
        targets = [
            fn for fn in filenames
            if not already_extracted(fn, out_dir=udk_lite_root, cache=cache)
        ]
        dirs = [
            t for t in targets
            if (udk_lite_root / t).is_dir()
        ]
        targets = [t for t in targets if t not in dirs]

        out_files = [
            str((udk_lite_root / target).resolve())
            for target in targets
        ]

        out_dirs = [
            str(udk_lite_root / d)
            for d in dirs
        ]

        for od in out_dirs:
            Path(od).mkdir(parents=True, exist_ok=True)

        if targets:
            print(f"extracting {len(targets)} targets to '{udk_lite_root}'")
            pkg.extract(udk_lite_root, targets)

        cache.pkg_archive_extracted_files += out_files + out_dirs
        cache.pkg_archive_extracted_files = list(set(cache.pkg_archive_extracted_files))
        write_cache(cache_file, cache)

    for script_file in input_script_msg_files:
        dst_dir = udk_lite_root / "Development/Src/UMBTests/Classes/"
        dst_dir.mkdir(parents=True, exist_ok=True)
        dst = dst_dir / script_file.name
        print(f"'{script_file}' -> '{dst}'")
        shutil.copy(script_file, dst)

    log_dir = udk_lite_root / "UDKGame/Logs/"
    log_file = log_dir / "Launch.log"

    building_event = threading.Event()
    testing_event = threading.Event()
    poker_event = threading.Event()

    obs = watchdog.observers.Observer()
    watcher = LogWatcher(building_event, testing_event, log_file)

    if not log_file.exists():
        print(f"'{log_file}' does not exist yet, touching...")
        log_file.parent.mkdir(parents=True, exist_ok=True)
        log_file.touch()

    obs.schedule(watcher, str(log_dir))
    obs.start()

    cfg_file = udk_lite_root / "UDKGame/Config/DefaultEngine.ini"

    poker = threading.Thread(
        target=poke_file,
        args=[log_file, poker_event],
    )
    poker.start()

    # if not cfg_file.exists():
    #     print(f"'{cfg_file}' does not exist, running dry-run server to create config files")
    #     _ = await run_udk_server(
    #         watcher=watcher,
    #         udk_lite_root=udk_lite_root,
    #         testing_event=testing_event,
    #         udk_args="Entry",
    #     )

    cfg = UDKConfigParser(comment_prefixes=";")
    cfg.read(cfg_file)

    pkg_name = "UMBTests"
    edit_packages = cfg["UnrealEd.EditorEngine"].getlist("+EditPackages")
    if pkg_name not in edit_packages:
        edit_packages.append(pkg_name)
        cfg["UnrealEd.EditorEngine"]["+EditPackages"] = "\n".join(edit_packages)

    with cfg_file.open("w") as f:
        cfg.write(f, space_around_delimiters=False)

    ec = await run_udk_build(
        watcher=watcher,
        udk_lite_root=udk_lite_root,
        building_event=building_event,
        use_shell=use_shell,
    )

    ec += await run_udk_server(
        watcher=watcher,
        udk_lite_root=udk_lite_root,
        testing_event=testing_event,
        udk_args="Entry?Mutator=UMBTests.UMBTestsMutator",
        use_shell=use_shell,
    )

    obs.stop()
    obs.join(timeout=UDK_TEST_TIMEOUT)

    if obs.is_alive():
        raise RuntimeError("timed out waiting for observer thread")

    poker_event.set()
    poker.join(timeout=5)

    if poker.is_alive():
        raise RuntimeError("timed out waiting for poker thread")

    if ec != 0:
        raise RuntimeError(f"UDK.exe error (sum of all exit codes): {ec}")

    print(f"finished with {len(watcher.warnings)} warnings")
    print(f"finished with {len(watcher.errors)} errors")

    print("WARNINGS:")
    for warn in watcher.warnings:
        print(warn.strip())

    print("ERRORS:")
    for err in watcher.errors:
        print(err.strip())

    if watcher.errors:
        raise RuntimeError("failed, errors detected")


if __name__ == "__main__":
    try:
        asyncio.run(main())
    except Exception as _e:
        print(f"error running main: {_e}")
        print_exc()
        sys.exit(1)
