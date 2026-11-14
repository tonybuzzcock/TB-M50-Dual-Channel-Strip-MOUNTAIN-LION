// Params.h
// Parameter table shared by the VST wrapper, the editor and the preset store.
//
// Channel 1, then channel 2, each top to bottom as on the M-50's input strip:
// INPUT (MIC / D.I. / LINE, as the real channels 1-2's MIC / INST / RMX), TRIM, EQUALIZER BYPASS, HIGH / MID / LOW (frequency, then
// boost / cut), PAN, fader. Then the STEREO MASTER fader, BYPASS (the whole
// plugin) and LINK (channel 1's controls drive channel 2, except PAN): buttons
// above the master fader, the user's additions.
// Left out at the user's request: MIC ATT, PHASE, the AUX sends, ASSIGN, MUTE,
// SOLO / PFL.

#ifndef TBM50_PARAMS_H
#define TBM50_PARAMS_H

namespace tbm50
{

enum ChannelParam
{
    kCInput = 0, kCTrim, kCBypass, kCHiFreq, kCHiGain, kCMidFreq, kCMidGain,
    kCLoFreq, kCLoGain, kCPan, kCFader,
    kNumChannelParams
};

enum Param
{
    kCh1 = 0,
    kCh2 = kNumChannelParams,
    kMaster = 2 * kNumChannelParams,
    kBypass,
    kLink,
    kNumParams
};

inline int chParam (int channel, int p) { return channel * kNumChannelParams + p; }

enum Kind { kMicLine, kTrimDb, kBypassSw, kHiHz, kMidHz, kLoHz, kEqDb, kPanPos, kFaderDb, kMasterDb, kBypassAll, kLinkSw };

const char* const kParamNames[kNumParams] = {
    "1 Input", "1 Trim", "1 EQ Byp", "1 HiFreq", "1 HiGain", "1 MdFreq", "1 MdGain", "1 LoFreq", "1 LoGain", "1 Pan", "1 Fader",
    "2 Input", "2 Trim", "2 EQ Byp", "2 HiFreq", "2 HiGain", "2 MdFreq", "2 MdGain", "2 LoFreq", "2 LoGain", "2 Pan", "2 Fader",
    "Master", "Bypass", "Link"
};

const int kParamKind[kNumParams] = {
    kMicLine, kTrimDb, kBypassSw, kHiHz, kEqDb, kMidHz, kEqDb, kLoHz, kEqDb, kPanPos, kFaderDb,
    kMicLine, kTrimDb, kBypassSw, kHiHz, kEqDb, kMidHz, kEqDb, kLoHz, kEqDb, kPanPos, kFaderDb,
    kMasterDb, kBypassAll, kLinkSw
};

// Defaults: LINE, TRIM at 0.375 (nominal, about 11 o'clock), EQ in and flat with the frequencies
// at 12 o'clock, channel 1 panned hard left and channel 2 hard right, faders at
// 7.5 (0 dB): the source passes at unity on both sides.
const float kDefaults[kNumParams] = {
    1.0f, 0.375f, 0.0f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.0f, 0.75f,
    1.0f, 0.375f, 0.0f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 1.0f, 0.75f,
    0.75f, 0.0f, 0.0f
};

inline bool isOn (float v) { return v >= 0.5f; }

// LINK: every channel control is linked except PAN
inline bool isChannel1 (int i) { return i >= kCh1 && i < kCh2; }
inline bool isChannel2 (int i) { return i >= kCh2 && i < kMaster; }
inline bool linkedControl (int channelParam) { return channelParam != kCPan; }

// INPUT selector: 0 MIC, 0.5 D.I., 1 LINE (the DSP's InputMode order)
inline int inputIndex (float v)
{
    const int k = (int) (v * 2.0f + 0.5f);
    return k < 0 ? 0 : (k > 2 ? 2 : k);
}
inline float inputValue (int k) { return (float) k * 0.5f; }

} // namespace tbm50

#endif
