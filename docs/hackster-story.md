## Overview

TAB5 SYNTH transforms the M5Stack Tab5 into a self-contained polyphonic musical instrument.

Its seven-voice audio engine combines four oscillator waveforms, microphone sampling, external I2S audio input, an ADSR envelope, four effects, and independent LFO modulation for AMP and FX parameters.

The touchscreen becomes a two-octave keyboard, XY performance pad, rotary-style control panel, physical-style fader, preset selector, and real-time visual display. Sound generation and audio output run locally on the Tab5, so no computer or external synthesizer is required for performance.

<!-- Insert a photo or short video of the synthesizer running on the actual Tab5 here. -->

*TAB5 SYNTH running entirely on the M5Stack Tab5.*

## Why I Built It

The Tab5 has a large multi-touch display, an ESP32-P4, a built-in microphone, and a speaker. I wanted to use all of these features as parts of one instrument instead of treating the Tab5 as only a display or controller.

The goal was to create a synthesizer that is playable immediately after power-on while still providing enough modulation and sound-design controls for experimentation.

## How the Tab5 Is Used

The ESP32-P4 runs the audio engine, effects, envelope processing, LFO modulation, touch handling, and LVGL interface.

The Tab5 hardware provides:

- A 1280 x 720 multi-touch performance interface
- A built-in microphone for sample recording
- A built-in speaker for standalone output
- GPIO and I2S support for external digital audio
- Non-volatile storage for five user memories

## Sound Engine

TAB5 SYNTH provides sine, saw, square, and triangle oscillators. It also includes ten instrument-specific timbre presets and a random preset.

Holding the MIC button records between one and five seconds of audio from the built-in microphone. The recorded sound can then be played from the keyboard like an instrument.

The signal path includes the ADSR envelope, delay, chorus, distortion, bitcrusher, and Tab5 speaker output. An optional external device, such as an AtomS3 configured as an I2S transmitter, can also be used as a digital audio source.

![AMP controls and ADSR display](https://airpocket-soundman.github.io/Tab5_synth/assets/screens/amp.png)

*The AMP page combines five rotary controls, a physical-style fader, and a dot-matrix ADSR display.*

## Performance Interface

The lower part of the screen contains a two-octave multi-touch keyboard. Pressed keys physically sink and darken through LVGL animations.

The XY pad covers MIDI notes 48-96. Semitone mode quantizes the pitch, while continuous mode allows smooth pitch movement.

Parameters are edited with rotary-style knobs. Selecting a knob connects it to the shared physical-style fader, providing both direct knob control and precise horizontal adjustment.

## Modulation and Effects

Delay, chorus, distortion, and bitcrusher can be enabled independently.

A dedicated LFO can be configured for each of the 17 AMP and FX targets. Every target stores its own rate, depth, waveform, and enabled state. This makes it possible to animate envelope stages, volume, delay feedback, chorus depth, distortion tone, bitcrusher rate, and other parameters independently.

![Per-parameter LFO editor](https://airpocket-soundman.github.io/Tab5_synth/assets/screens/lfo.png)

*Each AMP and FX target stores independent LFO settings.*

## Presets and Memories

Ten presets provide instrument-oriented harmonic profiles for guitar, piano, organ, recorder, pad, pluck, bell, brass, bass, and synthesizer sounds. A random preset generates a new combination of timbre, envelope, and effects.

Five non-volatile memory slots store the complete working state, including source selection, parameters, effects, and LFO settings.

![Preset and memory bank](https://airpocket-soundman.github.io/Tab5_synth/assets/screens/bank.png)

*The BANK page provides ten timbres, a random preset, and five persistent memory slots.*

## Three LVGL Skins

The interface includes three switchable skins:

- Retro Wood: walnut, brass, and cream-colored keys
- Metal: brushed aluminum, silver controls, and hardware-style framing
- Neon: cyan, magenta, lime, amber, and red digital accents

The skins change only the visual appearance. Sound parameters and performance state remain unchanged.

![Retro Wood, Metal, and Neon skins](https://airpocket-soundman.github.io/Tab5_synth/assets/screens/skin-comparison.png)

*Three visual identities share the same controls, parameters, and performance state.*

## Development Challenges

One challenge was producing stable polyphony with the available speaker mixer channels. One channel is reserved as a silent keepalive so the amplifier does not restart and create a pop on every note.

Multi-touch performance also required stable voice reuse and note assignment so that held notes were not unnecessarily restarted when fingers moved.

The LVGL interface was separated from the hardware audio engine so the same UI source can run on the Tab5 and in a browser-based simulator.

## Build Instructions

Install PlatformIO, clone the repository, and run:

```bash
git clone https://github.com/airpocket-soundman/Tab5_synth.git
cd Tab5_synth
pio run
pio run -t upload
```

To open the serial monitor:

```bash
pio device monitor -b 115200
```

After flashing, the synthesizer starts automatically on the Tab5.

## Basic Operation

1. Select SINE, SAW, SQUARE, TRIANGLE, MIC, or I2S.
2. Play notes using the keyboard or XY pad.
3. Open AMP to edit volume and ADSR.
4. Open FX to enable and adjust effects.
5. Select a parameter, then open LFO to configure modulation.
6. Open BANK to load presets or use memory slots M1-M5.
7. Tap the title or SOURCE heading to switch UI skins.

## Browser Simulator

The LVGL interface can be explored without Tab5 hardware using the [interactive browser simulator](https://airpocket-soundman.github.io/Tab5_synth/simulator.html).

The simulator demonstrates the complete interface and animations. Audio recording and hardware output require the actual Tab5.

## Future Improvements

Possible future additions include MIDI input and output, external clock synchronization, persistent sample storage, and additional synthesis engines.

## Source Code and Documentation

- [Source code](https://github.com/airpocket-soundman/Tab5_synth)
- [English manual](https://airpocket-soundman.github.io/Tab5_synth/en/)
- [Japanese manual](https://airpocket-soundman.github.io/Tab5_synth/)
- [Simplified Chinese manual](https://airpocket-soundman.github.io/Tab5_synth/zh/)

The original application code and UI assets are released under the MIT License.
