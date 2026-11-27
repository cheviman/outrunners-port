"""
    Name:        pack_devices.py
    Description: the release's packages of the devices, one each (Sebas, 09-11-2026: not inside the
                 computers' packages): outrunners-VERSION-switch.zip, -android.zip, -anbernic.zip,
                 -r36s.zip and -ios.zip, each the device's game without assets beside the tool that makes the
                 player's game of it, for Windows, macOS and Linux (iOS has no tool)
    Version:     0.3.0
    Created:     09-11-2026
    Modified:    25-11-2026
    Author:      Sebastian Toyos

    license:BSD-3-Clause
    A package is a folder (outrunners-switch, ...) holding the device's README, the tool three times (its
    Windows .exe, and its macOS and Linux programs with the system in their names: the three sit beside the
    device's folder, which each finds there), the device's game in its folder (switch, android, anbernic, r36s),
    font.bmp (the tools' letters) and the licenses. The tools, font.bmp and the licenses come from each
    computer's kit (the CI's build job: dist/kit, packed as outrunners-OS-kit.tar.gz), given as a folder each
    under --kits: windows-x64, macos-universal and linux-x64. The zip keeps the programs' permissions (Unix
    entries), so that the macOS and Linux tools run as they are unpacked there. The zip's name carries the
    release's version (--version: v1.4.1, the CI's "dev" else), the folder in it does not.

    iOS has no tool: outrunners-ios.zip holds the unsigned .ipa (--ios, the CI's ios job: ports/ios/ios.cmake),
    which the player's sideloading tool signs as it installs it, with its README and the licenses.

    python3 ports/pack_devices.py --kits DIR --switch DIR --android DIR --anbernic DIR --r36s DIR [--version V]
        [--ios DIR] --out DIR
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
    ("r36s", "OutRunners R36S", "README-r36s.txt"),
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


def pack(device, tool, readme, game, kits, out, version):
    top = "outrunners-" + device
    target = os.path.join(out, "outrunners-%s-%s.zip" % (version, device) if version else top + ".zip")
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


def pack_ios(ipas, kits, out, version):
    """outrunners-ios.zip: the README, the licenses and the .ipa"""
    top = "outrunners-ios"
    target = os.path.join(out, "outrunners-%s-ios.zip" % version if version else top + ".zip")
    shared = os.path.join(kits, "linux-x64", "kit")    # (the licenses: the same in every kit)
    found = sorted(name for name in os.listdir(need(ipas)) if name.endswith(".ipa"))
    if found != ["OutRunners.ipa"]:
        sys.exit("pack_devices: %s has %s, not OutRunners.ipa" % (ipas, found))
    with zipfile.ZipFile(target, "w") as archive:
        add(archive, need(os.path.join(DOCS, "README-ios.txt")), top + "/README.txt", FILE)
        add(archive, need(os.path.join(shared, "LICENSE.txt")), top + "/LICENSE.txt", FILE)
        add_folder(archive, need(os.path.join(shared, "licenses")), top + "/licenses")
        add(archive, os.path.join(ipas, "OutRunners.ipa"), top + "/OutRunners.ipa", FILE)
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
    parser.add_argument("--r36s", required=True, help="the R36S's game: OutRunners.sh and outrunners/")
    parser.add_argument("--version", default="", help="the release's version in the zips' names (v1.4.1)")
    parser.add_argument("--ios", help="iOS's game: OutRunners.ipa (none: no iOS package)")
    parser.add_argument("--out", required=True, help="where the zips go")
    arguments = parser.parse_args()
    os.makedirs(arguments.out, exist_ok=True)
    games = {"switch": arguments.switch, "android": arguments.android, "anbernic": arguments.anbernic,
             "r36s": arguments.r36s}
    for device, tool, readme in DEVICES:
        pack(device, tool, readme, games[device], arguments.kits, arguments.out, arguments.version)
    if arguments.ios:
        pack_ios(arguments.ios, arguments.kits, arguments.out, arguments.version)


if __name__ == "__main__":
    main()
