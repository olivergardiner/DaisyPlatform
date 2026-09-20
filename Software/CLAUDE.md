# Perspective — Daisy Seed guitar pedal firmware

C++ firmware for a guitar pedal built on the Electrosmith Daisy Seed (STM32H750, `libDaisy` + `DaisySP`). Lives under `Software/`; `dependencies/` holds the vendored libDaisy/DaisySP/CMSIS trees — don't edit those.

## Build

`make` from `Software/`. Two platform variants, selected by `PLATFORM` (default `stereo`), each with its own build dir so switching can't mix objects:

```sh
make PLATFORM=stereo   # -DPERSPECTIVE_PLATFORM_STEREO, build-stereo/
make PLATFORM=amp      # -DPERSPECTIVE_PLATFORM_AMP,    build-amp/
```

"Pedal mode" = stereo platform (two independent I/O jacks). "Amp mode" = amp platform, embedded in amp hardware, mono FX chain + always-in-circuit cab sim on channel 2.

`CPP_SOURCES` in the `Makefile` is an **explicit file list**, not a glob — new source files must be added there or they silently won't build.

### Syntax-checking a single file without a full build

Useful when iterating on one file — much faster than a full link, and catches header/type errors immediately. Grab the real per-file compile command with a dry run, then add `-fsyntax-only`:

```sh
make -n | grep -m1 <somefile>.cpp   # copy the exact arm-none-eabi-g++ invocation
# append -fsyntax-only, drop -o and the .lst/.d flags if you like
```

The toolchain is `arm-none-eabi-g++`/`gcc` (ARM GNU Toolchain), built with `-std=gnu++20`, RTTI and exceptions disabled (`-fno-rtti -fno-exceptions`) — no `dynamic_cast`, no `throw`.

## Parameter system (`parameters/`)

A parameter's **kind** and its **physical control binding** are orthogonal, set independently:

- `ParameterKind` (`EffectParameter::GetKind()`): `VALUE` (continuous float), `ENUM` (named option, stored as index `[0, count)` — also used for booleans as a 2-option enum), `TIME` (ms, optionally BPM-synced), `INTEGER` (whole number).
- `ControlBinding` (`EffectParameter::GetControlBinding()`): `NONE`, `POTENTIOMETER`, `ENCODER`, `BUTTON` — set via `BindPotentiometer(index, curve)` / `BindEncoder(index, stepSize, reversed)` / `BindButton(index)` in the owning effect's `Init()`, independent of kind.

Concrete classes: `ValueParameter`, `EnumParameter`, `TimeParameter`, `IntegerParameter` (all in `parameters/`). There is no separate toggle or discrete-pot class — those collapsed into `EnumParameter` (2-option enum for booleans; any option count for discrete pots/encoders, with a per-instance `SetWrap(bool)`, default wrap).

`TimeParameter` doesn't store its own tempo/time mode — it derives `GetDisplayMode()` live from a linked mode-toggle `EnumParameter` (`SetModeToggle`/`GetModeToggle`), option index 1 = tempo mode. Every effect with a `TimeParameter` must link a mode toggle in `Init()` or it's permanently in time mode.

UI routing (`perspective.cpp`) dispatches on `GetControlBinding()` to decide *whether* an event applies to a parameter, then calls the uniform virtuals (`SetNormalizedValueWithCurve`, `Increment`/`Decrement`, `OnButtonPress`) that every kind implements — it never dispatches on `GetKind()` for that. `GetKind()` is used where the *edit semantics* differ (e.g. `AdjustSelectedParameter`'s encoder-2 "edit selected parameter" path).

`ApplyPotCurve(PotCurve, normalized)` (in `effectparameter.h/.cpp`) is the free function for pot tapers — use it instead of hand-rolling a curve.

## Presets (`preset.h`, save/load in `perspective.cpp`)

Presets store parameters as **bare floats, positional by `AddParameter()` call order** in each effect's `Init()` — no name, type, or physical index is serialized. This means:

- Reordering or adding/removing `AddParameter()` calls in an existing effect silently corrupts old flash-saved presets for that effect (position `i` maps to a different logical parameter).
- Enum/toggle values round-trip fine across the old and new parameter system because both store the option index as a raw float over `[0, count-1]`.
- There's a `kPresetVersion` blob-format guard in flash, but it does not track per-effect parameter layout — bumping it wipes *all* presets, it can't selectively invalidate one effect.

## Physical control indices (`controls.h`)

Plain `#define` constants (`KNOB_1_IDX`, `ENCODER_1_IDX`, `MACRO_KNOB_MIX_IDX`, etc.) — note `KNOB_1_IDX == ENCODER_1_IDX == 0` (they're separate index spaces disambiguated by which `UIEventType` fired, not by value). Any file using these must `#include "controls.h"` (or `"../controls.h"` from `effects/`) directly — it is *not* pulled in transitively through the parameter headers.

Knobs 1–5 are statically dedicated to macro roles (Mix/Depth/Rate/Feedback/Subdivision) via `MacroRole` tagging on a parameter (`SetMacroRole`) — the pairing between a macro role and its physical knob is convention (matching index + role in each effect's `Init()`), not enforced by the type system.
