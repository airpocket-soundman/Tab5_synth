Import("env")

from pathlib import Path


def set_tool(name: str, package_dir: Path, executable: str) -> None:
    exe_path = package_dir / "bin" / executable
    if exe_path.exists():
        env.Replace(**{name: str(exe_path)})


riscv_dir = env.PioPlatform().get_package_dir("toolchain-riscv32-esp")
if riscv_dir:
    riscv_path = Path(riscv_dir)
    env.PrependENVPath("PATH", str(riscv_path / "bin"))
    set_tool("CC", riscv_path, "riscv32-esp-elf-gcc.exe")
    set_tool("CXX", riscv_path, "riscv32-esp-elf-g++.exe")
    set_tool("AR", riscv_path, "riscv32-esp-elf-gcc-ar.exe")
    set_tool("RANLIB", riscv_path, "riscv32-esp-elf-gcc-ranlib.exe")
    set_tool("GDB", riscv_path, "riscv32-esp-elf-gdb.exe")
