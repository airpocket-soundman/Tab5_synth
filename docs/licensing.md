# Licensing

## Project code and assets

The Tab5 Synth original application code and original UI assets are licensed
under the MIT License in `LICENSE`. This includes the background images under
`ui/assets/`. MIT permits use, modification, redistribution, sublicensing, and
commercial use as long as the copyright and license notice is retained.

Third-party code, fonts, and other materials are not relicensed by the project
MIT license. The simulator manifest carries `LICENSE` and the scope in
`LICENSE_NOTICES.md` into generated `.lvglsim` packages and standalone HTML.

## Web preview

The web preview package contains `ui/lvgl_synth_ui.c` and the images under
`ui/assets/`. It does not contain the device audio engine or board support
libraries. Web Simulator for LVGL supplies LVGL and the WebAssembly runtime and
displays their complete notices before the Tab5 Synth notice.

## Device firmware dependencies

The PlatformIO firmware build resolves these direct dependencies:

| Component | Resolved version | License |
| --- | --- | --- |
| M5Unified | 0.2.13 (`a625672`) | MIT |
| M5GFX | 0.2.19 (`53a7184`) | MIT |
| LVGL | 9.2.2 | MIT |
| Arduino-ESP32 | 3.2.1 | LGPL 2.1 and component-specific licenses |
| ESP-IDF libraries | 5.4-based bundle (`858a988d6e`) | Apache 2.0 and component-specific licenses |

The build tools themselves are not distributed as part of the firmware image.
Before publishing a firmware binary, preserve the license and notice files from
the exact resolved framework and component packages. Arduino-ESP32's LGPL 2.1
terms also require the applicable corresponding source and relinking materials
or another Section 6-compliant distribution method; a notice alone is not
sufficient.

This document is an engineering summary, not legal advice.
