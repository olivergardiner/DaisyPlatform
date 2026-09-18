#ifndef PERSPECTIVE_PLATFORM_H
#define PERSPECTIVE_PLATFORM_H

// Build target selection. Choose with `make PLATFORM=stereo` (default) or
// `make PLATFORM=amp`.
//
// PERSPECTIVE_PLATFORM_STEREO
//   The stereo effects platform. The selected effect processes both channels
//   through ProcessStereo(), and every effect registered on this target keeps
//   independent per-channel state.
//
// PERSPECTIVE_PLATFORM_AMP
//   Mono FX chain plus cab simulation. Channel 0 carries the effect chain
//   through the mono Process(); channel 1 is passed through. This target also
//   registers the amp-chain effects — noise gate, drive, tone stack, cab sim
//   and the Sandman compound.
//
//   Those effects are mono by construction: their biquad and envelope state is
//   single-channel, so the default Effect::ProcessStereo(), which runs the same
//   state over L and then R, would corrupt it. That is why they are registered
//   on this target only, rather than being available everywhere.

#if defined(PERSPECTIVE_PLATFORM_STEREO) && defined(PERSPECTIVE_PLATFORM_AMP)
#error "Define only one of PERSPECTIVE_PLATFORM_STEREO / PERSPECTIVE_PLATFORM_AMP"
#endif

#if !defined(PERSPECTIVE_PLATFORM_STEREO) && !defined(PERSPECTIVE_PLATFORM_AMP)
// Default to the stereo platform so an unqualified build is unchanged.
#define PERSPECTIVE_PLATFORM_STEREO 1
#endif

#endif // PERSPECTIVE_PLATFORM_H
