"""
    Name:        pack_devices.py
    Description: the release's packages of the devices, one each (Sebas, 09-11-2026: not inside the
                 computers' packages): outrunners-switch.zip, outrunners-android.zip and
                 outrunners-anbernic.zip, each the device's game without assets beside the tool that makes the
                 player's game of it, for Windows, macOS and Linux
    Version:     0.1.0
    Created:     09-11-2026
    Modified:    09-11-2026
    Author:      Sebastian Toyos

    license:BSD-3-Clause
    A package is a folder (outrunners-switch, ...) holding the device's README, the tool three times (its
    Windows .exe, and its macOS and Linux programs with the system in their names: the three sit beside the
    device's folder, which each finds there), the device's game in its folder (switch, android, anbernic),
    font.bmp (the tools' letters) and the licenses. The tools, font.bmp and the licenses come from each
    computer's kit (the CI's build job: dist/kit, packed as outrunners-OS-kit.tar.gz), given as a folder each
    under --kits: windows-x64, macos-universal and linux-x64. The zip keeps the programs' permissions (Unix
    entries), so that the macOS and Linux tools run as they are unpacked there.

    python3 ports/pack_devices.py --kits DIR --switch DIR --android DIR --anbernic DIR --out DIR
"""
import argparse
import os
import stat
import sys
import zipfile

HERE = os.path.dirname(os.path.abspath(__file__))
DOCS = os.path.join(HERE, "..", "docs", "package")

# device, the tool's name, the package's README
DEVICES = [
    ("switch", "OutRunners NSP", "README-switch.txt"),
    ("android", "OutRunners APK", "README-android.txt"),
    ("anbernic", "OutRunners Anbernic", "README-anbernic.txt"),
]

# each computer's kit: its folder under --kits, the tool's name in it and in the package
KITS = [
    ("windows-x64", "{tool}.exe", "{tool}.exe"),
    ("macos-universal", "{tool}", "{tool} (macOS)"),
    ("linux-x64", "{tool}", "{tool} (Linux)"),
]

PROGRAM = 0o755
FILE = 0o644


def add(archive, source, name, mode):
    """A file into the zip as a Unix entry with 'mode', so that unzip and macOS's Archive Utility keep it"""
    info = zipfile.ZipInfo.from_file(source, name)
    info.create_system = 3
    info.external_attr = (stat.S_IFREG | mode) << 16
    info.compress_type = zipfile.ZIP_DEFLATED
    with open(source, "rb") as data:
        archive.writestr(info, data.read(), compresslevel=9)


def add_folder(archive, source, name, programs=()):
    """A folder's files into the zip under 'name', in a stable order; 'programs' the names that run"""
    for root, folders, files in os.walk(source):
        folders.sort()
        for file in sorted(files):
            path = os.path.join(root, file)
            inside = name + "/" + os.path.relpath(path, source).replace(os.sep, "/")
            add(archive, path, inside, PROGRAM if file in programs else FILE)


def need(path):
    if not os.path.exists(path):
        sys.exit("pack_devices: missing " + path)
    return path


def pack(device, tool, readme, game, kits, out):
    top = "outrunners-" + device
    target = os.path.join(out, top + ".zip")
    shared = os.path.join(kits, "linux-x64", "kit")    # (font.bmp and the licenses: the same in every kit)
    with zipfile.ZipFile(target, "w") as archive:
        add(archive, need(os.path.join(DOCS, readme)), top + "/README.txt", FILE)
        add(archive, need(os.path.join(shared, "LICENSE.txt")), top + "/LICENSE.txt", FILE)
        add_folder(archive, need(os.path.join(shared, "licenses")), top + "/licenses")
        add(archive, need(os.path.join(shared, "font.bmp")), top + "/font.bmp", FILE)
        for kit, name, packed in KITS:
            source = need(os.path.join(kits, kit, "kit", name.format(tool=tool)))
            add(archive, source, top + "/" + packed.format(tool=tool), PROGRAM)
        add_folder(archive, need(game), top + "/" + device, programs=("OutRunners", "OutRunners.sh"))
    with zipfile.ZipFile(target) as archive:
        names = archive.namelist()
    print("%s: %d files, %d bytes" % (target, len(names), os.path.getsize(target)))
    for name in names:
        print("   " + name)


def main():
    parser = argparse.ArgumentParser(description="The devices' packages of a release")
    parser.add_argument("--kits", required=True, help="the computers' kits: windows-x64, macos-universal, linux-x64")
    parser.add_argument("--switch", required=True, help="the Switch's game: main, main.npdm, control.nacp, icon.jpg")
    parser.add_argument("--android", required=True, help="Android's game: outrunners-base.apk")
    parser.add_argument("--anbernic", required=True, help="Anbernic's game: OutRunners.sh and outrunners/")
    parser.add_argument("--out", required=True, help="where the zips go")
    arguments = parser.parse_args()
    os.makedirs(arguments.out, exist_ok=True)
    games = {"switch": arguments.switch, "android": arguments.android, "anbernic": arguments.anbernic}
    for device, tool, readme in DEVICES:
        pack(device, tool, readme, games[device], arguments.kits, arguments.out)


if __name__ == "__main__":
    main()
