# Copyright (C) 2023-2024  Tuomo Kriikkula
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

from pathlib import Path

_FILE_DIR = Path(__file__).parent

UDK_TEST_TIMEOUT = 300
UDK_LITE_TAG = "1.0.1"
UDK_LITE_ROOT = "./UDK-Lite/"
UDK_LITE_RELEASE_URL = (f"https://github.com/tuokri/UDK-Lite/releases/download/{UDK_LITE_TAG}/UDK"
                        f"-Lite-{UDK_LITE_TAG}.7z")
UDK_USCRIPT_MESSAGE_FILES = _FILE_DIR / "../../cmake-build-debug/generated/*.uc"
