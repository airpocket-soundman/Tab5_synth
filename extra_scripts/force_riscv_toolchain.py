Import("env", "projenv")

import os
from pathlib import Path

print("[force_riscv_toolchain] script loaded")


def find_toolchain_dir() -> Path | None:
    package_dir = env.PioPlatform().get_package_dir("toolchain-riscv32-esp")
    if package_dir:
        return Path(package_dir)

    home = Path(os.environ.get("USERPROFILE", "")) / ".platformio" / "packages"
    if not home.exists():
        return None

    candidates = sorted(home.glob("toolchain-riscv32-esp*"), key=lambda p: p.name, reverse=True)
    for candidate in candidates:
        if (candidate / "bin" / "riscv32-esp-elf-g++.exe").exists():
            return candidate
    return None


def apply_toolchain(target_env, package_dir: Path, label: str) -> None:
    bin_dir = package_dir / "bin"
    bin_path = str(bin_dir)
    print(f"[force_riscv_toolchain] applying to {label}: {bin_path}")

    target_env.PrependENVPath("PATH", bin_path)
    target_env["ENV"]["PATH"] = bin_path + os.pathsep + target_env["ENV"].get("PATH", "")

    tools = {
        "AS": "riscv32-esp-elf-as.exe",
        "CC": "riscv32-esp-elf-gcc.exe",
        "CXX": "riscv32-esp-elf-g++.exe",
        "AR": "riscv32-esp-elf-gcc-ar.exe",
        "RANLIB": "riscv32-esp-elf-gcc-ranlib.exe",
        "SIZETOOL": "riscv32-esp-elf-size.exe",
    }

    for key, executable in tools.items():
        executable_path = bin_dir / executable
        if executable_path.exists():
            print(f"[force_riscv_toolchain] set {label} {key} -> {executable_path}")
            target_env.Replace(**{key: str(executable_path)})


package_dir = find_toolchain_dir()
print(f"[force_riscv_toolchain] package_dir={package_dir}")
if package_dir:
    apply_toolchain(env, package_dir, "env")
    apply_toolchain(projenv, package_dir, "projenv")
    os.environ["PATH"] = str(package_dir / "bin") + os.pathsep + os.environ.get("PATH", "")
