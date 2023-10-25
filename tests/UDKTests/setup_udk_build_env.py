# TODO: read env vars.
#   - copy generated UScript files to UDK sources location

import glob
import json
import os
import shutil
from pathlib import Path

import httpx
import py7zr
import tqdm

import defaults

SCRIPT_DIR = Path(__file__).parent
CACHE_DIR = SCRIPT_DIR / ".cache/"


def resolve_script_path(path: str) -> Path:
    p = Path(path)
    if not p.is_absolute():
        p = SCRIPT_DIR / p
    return p.resolve()


def load_cache(path: Path) -> dict:
    with path.open() as f:
        cache = json.load(f)
    return cache


def main():
    hard_reset = False
    cache = {}

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
        cache_file.touch(exist_ok=True)

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

    input_msgs = [
        resolve_script_path(path) for path in
        glob.glob(uscript_message_files, recursive=True)
    ]

    if not input_msgs:
        raise RuntimeError("no input script files found")

    pkg_file = CACHE_DIR / Path(udk_lite_release_url).name
    if pkg_file.exists():
        print(f"using cached release: '{pkg_file}'")
    else:
        with pkg_file.open("wb") as f:
            print(f"downloading '{udk_lite_release_url}'")
            with httpx.stream(
                    "GET",
                    udk_lite_release_url,
                    timeout=60.0,
                    follow_redirects=True) as resp:
                resp.raise_for_status()
                total = int(resp.headers["Content-Length"])
                with tqdm.tqdm(total=total, unit_scale=True, unit_divisor=1024,
                               unit="B") as progress:
                    num_bytes_downloaded = resp.num_bytes_downloaded
                    for data in resp.iter_bytes():
                        f.write(data)
                        progress.update(resp.num_bytes_downloaded - num_bytes_downloaded)
                        num_bytes_downloaded = resp.num_bytes_downloaded

        with py7zr.SevenZipFile(pkg_file) as pkg:
            print(list(pkg.files))


if __name__ == "__main__":
    main()
