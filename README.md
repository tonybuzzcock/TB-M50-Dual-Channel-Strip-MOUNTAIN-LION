# TB M50 Dual Channel Strip (Mountain Lion VST)

Two input channels of the **TASCAM M-50** mixing console and its **stereo master**, as a
**VST 2.4, 64-bit Intel plugin for Mac OS X 10.8.5** (Pro Tools 11 through Blue Cat PatchWork).
Non-commercial, for personal use.

It is meant as a **stereo insert**: a drum buss, an aux, or the mix buss, the way tracks
returning from tape were mixed through the M-50. The **left input feeds channel 1, the right
input channel 2**, panned hard left and right by default, so the stereo image is kept. A mono
source works too (both channels get the same signal).

There is no component schematic. The model follows the M-50 operation and maintenance manual
(block diagram, level diagram, specifications, the EQ curves on page 27) and a photo of the
input strip (`Docs/M-50_channel_strip.jpg`).

The modern Mac version (CLAP / VST3 / AU) will live in `TB-M50-Dual-Channel-Strip-2026-MAC`.

## Panel

A portrait panel (a 722 x 920 face in a lacquered mahogany frame with mitred corners, 770 x 968
in all) in the console's own colours (warm grey face, black lettering, charcoal knurled
knobs with a red TRIM cap, orange EQ rings, a cream PAN cap, cream fader caps, the red lip along
the front). The two channels sit on the left; the STEREO MASTER fader and its L / R VUs are on
the right, with empty face between, like a two-channel M-50. A clear band of face separates the
faders' 0 from the preset strip. Only the user's TB electronic logo is
used (no TASCAM artwork).

Each channel, top to bottom:

| Control | Range |
|---|---|
| VU | channel level after the fader; 0 VU = nominal (-10 dBV = -18 dBFS) |
| INPUT | MIC / D.I. / LINE three-position slide switch (the real channels 1-2 select MIC / INST / RMX); the lever glows at the engaged position |
| TRIM | MIC +20..+60 dB, D.I. +12..+48 dB, LINE -15..+25 dB. Nominal in every position at 0.375 (about 11 o'clock) |
| OVERLOAD LED | lights 25 dB above nominal, as the manual says (see below) |
| EQUALIZER BYPASS | push button, lit red (with its LED) when bypassed |
| HIGH / MID / LOW | concentric: orange ring = FREQUENCY (HIGH 2.5-15 kHz, MID 500 Hz-5 kHz, LOW 50 Hz-1 kHz), dark cap = BOOST / CUT +-15 dB |
| PAN | L .. R, centre -3 dB |
| Fader | marked 0-10: 7.5 = 0 dB, 10 = +8 dB (the channel buffer amp), 5 = -10, 2.5 = -25, 1 = -40, 0 = off |

STEREO MASTER: one fader for both sides (same law), L and R VUs.

BYPASS (the user's addition, above the master fader): the whole plugin, lit red when in. It fades
to the dry input over about 10 ms (no click) and then passes it untouched; it also answers the
host's own bypass. Presets don't change it.

Left off at the user's request: MIC ATT, PHASE, the AUX sends, ASSIGN, MUTE, SOLO / PFL.

Bottom strip: the TB electronic logo, SAVE and 8 preset keys with names (the one in use glows red) (SAVE, then a slot, type a
name, Return; click the readout to rename the preset in use), and the tan readout.

## The OVERLOAD LED and the drive

On the user's desk, things sound best when the red OVERLOAD LED flashes now and then. The model
keeps that relationship: the LED's comparator watches the preamp's output and lights at **25 dB
over nominal** (8 V peak), held for 120 ms so each flash shows. The preamp's op-amps run on +-15 V
rails and swing about **+13.5 / -12.5 V**, so the rails are about 4 dB above the LED. The rounding
starts gently just under the LED's point, so an occasional flash means the peaks are being shaped
(slightly lopsided, so a touch of even harmonics); pushed well past it, the preamp clips. A 1 kHz
test tone gives about -51 dB of third harmonic at the LED's point and -17 dB 6 dB hotter.

TRIM's wider top end (LINE to +25 dB) lets an ordinary DAW track (peaks around -6 dBFS) reach the
LED with TRIM at about 3 o'clock.

## How it's modelled

- **Levels**: 1.0 in the host = 2.5 V peak, so the M-50's nominal -10 dBV is a -18 dBFS rms sine
  and reads 0 VU. At the defaults (LINE, TRIM nominal, EQ flat, faders at 7.5, pans hard over) the
  plugin passes the source at unity.
- **MIC**: low-noise preamp with hiss referred to its input (-120 dBV) and the op-amp's
  gain-bandwidth closing in the top end at high gain (about -4 dB at 15 kHz at full TRIM). A
  -18 dBFS track stands in for a -45 dBV microphone.
- **D.I.**: the 100k instrument input. A guitar pickup and cable loaded by 100k (with the guitar's
  250k volume pot) instead of the 1M of a typical interface: its resonance is damped and the top
  end softens (about -7 dB at 5 kHz relative to the track). A -18 dBFS track stands in for a
  -35.5 dBV instrument.
- **LINE**: the line amp, with a little output hiss (-92 dBV).
- **EQ**: three sweepable peaking sections, +-15 dB, constant width.
- **Fader and buffer amp**, **PAN** (sine / cosine), **summing amps** with buss hiss, the
  **STEREO MASTER** fader and output amps. Every amp has its own soft-kneed swing.
- **VUs**: rectified average with two poles, about 300 ms to full reading.
- Input / output coupling high-passes, no latency, plain C++98, no allocation while processing.

### Estimates (not given by the manual)

All in `Source/DSP/M50.cpp` as named constants. **Tune these against your desk:**

- `kVoltsPerUnit` = 2.5 V: where -18 dBFS sits (nominal).
- `kMicCalDb`, `kDiCalDb`: what level a track stands in for on MIC and D.I.
- `kLineTrimMinDb` / `kLineTrimMaxDb` = -15 / +25 dB (the manual gives MIC +20..+60 and INST
  +12..+48 from its input specs).
- `kPreKneePosV` / `kPreSwingPosV` = 6.5 / 13.5 V and `kPreKneeNegV` / `kPreSwingNegV` =
  6.0 / 12.5 V: where the preamp starts rounding and its rails. `kOlHoldS` = 120 ms.
- `kEqQ` = 0.9: the bells' width, read from the manual's curves.
- `kMicGbwHz` = 20 MHz, `kMicEinVrms`, `kDiEinVrms`, `kLineNoiseVrms`, `kBusNoiseVrms`: bandwidth
  and hiss.
- The D.I. pickup: `kPickupL` 2.5 H, `kPickupR` 6.5k, `kPickupC` 470 pF, `kGuitarPotOhm` 250k.
- The fader law between the marks (`kFaderMarks`, `kFaderDb`).
- The pan law (-3 dB centre).
- The EQ's order on the strip (HIGH at the top).

## Build

GitHub Actions builds the bundle on an Intel macOS runner with `-mmacosx-version-min=10.8.5`, checks
that the binary declares 10.8.5 and links only libraries that 10.8.5 has, loads it in a test host,
drives the panel offscreen and publishes screenshots to the `gui-preview` branch.

Local tests (Linux or Mac): `make -C Tests run vst-run`, sanitizers: `make -C Tests asan`.

## Install

Actions -> latest run -> Artifacts -> `TB-M50-Dual-Channel-Strip-mountain-lion-vst`. Unzip it
(twice), copy `TB M50 Dual Channel Strip.vst` to `/Library/Audio/Plug-Ins/VST/`, then rescan in
PatchWork.
