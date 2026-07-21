Import("env")

import os
from pathlib import Path
from typing import Optional

print("[force_riscv_toolchain] script loaded")


def find_toolchain_root() -> Optional[Path]:
    package_dir = env.PioPlatform().get_package_dir("toolchain-riscv32-esp")
    if package_dir:
        return Path(package_dir)

    home = Path(os.environ.get("USERPROFILE", "")) / ".platformio" / "packages"
    if not home.exists():
        return None

    for candidate in sorted(home.glob("toolchain-riscv32-esp*"), key=lambda p: p.name, reverse=True):
        if candidate.exists():
            return candidate
    return None


def resolve_bin_dir(root: Path) -> Optional[Path]:
    candidates = [
        root / "bin",
        root / "riscv32-esp-elf" / "bin",
    ]
    for p in candidates:
        if (p / "riscv32-esp-elf-g++.exe").exists():
            return p
    return None


def resolve_picolibc_include(root: Path) -> Optional[Path]:
    candidates = [
        root / "picolibc" / "include",
        root / "riscv32-esp-elf" / "picolibc" / "include",
    ]
    for p in candidates:
        if (p / "machine" / "_default_types.h").exists() and (p / "bits" / "c++config.h").exists():
            return p
    return None


def set_tool(name: str, bin_dir: Path, exe_name: str) -> None:
    exe = bin_dir / exe_name
    if exe.exists():
        env.Replace(**{name: str(exe)})
        print(f"[force_riscv_toolchain] set {name} -> {exe}")


root = find_toolchain_root()
print(f"[force_riscv_toolchain] root={root}")
if root is not None:
    bin_dir = resolve_bin_dir(root)
    print(f"[force_riscv_toolchain] bin_dir={bin_dir}")
    if bin_dir is not None:
        env.PrependENVPath("PATH", str(bin_dir))
        os.environ["PATH"] = str(bin_dir) + os.pathsep + os.environ.get("PATH", "")
        set_tool("AS", bin_dir, "riscv32-esp-elf-as.exe")
        set_tool("CC", bin_dir, "riscv32-esp-elf-gcc.exe")
        set_tool("CXX", bin_dir, "riscv32-esp-elf-g++.exe")
        set_tool("AR", bin_dir, "riscv32-esp-elf-gcc-ar.exe")
        set_tool("RANLIB", bin_dir, "riscv32-esp-elf-gcc-ranlib.exe")
        set_tool("SIZETOOL", bin_dir, "riscv32-esp-elf-size.exe")
        # PlatformIO invokes GNU as directly for library .S files. Match the
        # ESP32-P4 floating-point ABI used by the Arduino framework objects.
        env.AppendUnique(ASFLAGS=["-march=rv32imafc_zicsr_zifencei", "-mabi=ilp32f"])
