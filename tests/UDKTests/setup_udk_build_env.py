import glob
import json
import os
import shutil
from dataclasses import asdict
from dataclasses import dataclass
from dataclasses import field
from pathlib import Path
from pprint import pprint

import httpx
import py7zr
import tqdm

import defaults

SCRIPT_DIR = Path(__file__).parent
CACHE_DIR = SCRIPT_DIR / ".cache/"


@dataclass
class Cache:
    pkg_archive: str = ""
    pkg_archive_extracted_files: list[str] = field(default_factory=list)


def resolve_script_path(path: str) -> Path:
    p = Path(path)
    if not p.is_absolute():
        p = SCRIPT_DIR / p
    return p.resolve()


def write_cache(file: Path, cache: Cache):
    file.write_text(json.dumps(asdict(cache), indent=2))


def load_cache(path: Path) -> Cache:
    with path.open() as f:
        cache = json.load(f)
    return Cache(**cache)


def download_file(url: str, out_file: Path):
    with out_file.open("wb") as f:
        print(f"downloading '{url}'")
        with httpx.stream(
                "GET",
                url,
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


def remove_old_extracted(cache: Cache):
    for file in cache.pkg_archive_extracted_files:
        p = Path(file).resolve()
        if p.exists():
            print(f"removing '{p}'")
            p.unlink(missing_ok=True)


def already_extracted(archive_file: str, out_dir: Path, cache: Cache) -> bool:
    cached = cache.pkg_archive_extracted_files

    # Files in the archive are relative to the archive root.
    # Make them relative to the extraction target directory
    # for comparisons. Cached paths are absolute.
    extracted_file = (out_dir / archive_file).resolve()
    is_cached = str(extracted_file) in cached
    return extracted_file.exists() and is_cached


def main():
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

    # Check if cache tag, url, etc. match current ones, if not -> reinit.
    # Check if we have all pkg files in place. Compare to cache.

    dl_pkg_archive = False
    pkg_file = CACHE_DIR / Path(udk_lite_release_url).name

    pkg_file_outdated = cache.pkg_archive != str(pkg_file)
    if pkg_file_outdated:
        dl_pkg_archive = True
        print(f"cached archive '{cache.pkg_archive}' does not match '{pkg_file}'")

    if not pkg_file.exists():
        print(f"'{pkg_file}' does not exist")
        dl_pkg_archive = True

    if dl_pkg_archive:
        download_file(udk_lite_release_url, pkg_file)
        remove_old_extracted(cache)
        cache.pkg_archive_extracted_files = []
    else:
        print(f"using cached archive: '{pkg_file}'")

    # TODO: make a cache that updates itself automatically
    #   when fields are assigned. Just use diskcache?
    cache.pkg_archive = str(pkg_file)
    write_cache(cache_file, cache)

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
        pprint(dirs)
        targets = [t for t in targets if t not in dirs]

        for d in dirs:
            Path(d).mkdir(parents=True, exist_ok=True)

        out_files = [
            str((udk_lite_root / target).resolve())
            for target in targets
        ]

        out_dirs = [
            str(udk_lite_root / d)
            for d in dirs
        ]

        if targets:
            print(f"extracting {len(targets)} targets to '{udk_lite_root}'")
            pkg.extract(udk_lite_root, targets)

        cache.pkg_archive_extracted_files += out_files + out_dirs
        cache.pkg_archive_extracted_files = list(set(cache.pkg_archive_extracted_files))
        write_cache(cache_file, cache)

        pprint(targets)


if __name__ == "__main__":
    main()
