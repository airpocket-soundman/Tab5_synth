# Tab5 Synth

Synthesizer firmware for M5Stack Tab5.

See [spec.md](spec.md) for the current implementation specification.

## LVGL Web Simulator

UI development uses [Web Simulator for LVGL](https://github.com/airpocket-soundman/web-simulator-for-lvgl).
The simulator implementation and toolchain configuration are maintained in that repository; this project keeps only
[lvgl-simulator.json](lvgl-simulator.json) with its display size, portable UI source, entry point, and assets.

Build from a local simulator checkout:

```powershell
D:\GitHub\web-simulator-for-lvgl\lvgl-sim.ps1 build D:\GitHub\Tab5_synth
```

Generated files:

```text
build/lvgl-simulator/tab5-synth.lvglsim
build/lvgl-simulator/tab5-synth.html
```

Open `tab5-synth.html` directly in a browser. No Docker, nginx, or local server is required. For installation,
configuration schema, SDK cache, and troubleshooting details, follow the simulator repository documentation.
