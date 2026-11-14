// vst_host_test.cpp
// A minimal VST 2.4 host that loads the built plugin binary through its entry
// point and checks it behaves: opcodes, parameters, processing, latency,
// sample-rate changes, in-place buffers and shutdown.
//
//   vst_host_test <path to plugin binary>
// On macOS pass the binary inside the bundle:
//   "TB M50 Dual Channel Strip.vst/Contents/MacOS/TBM50DualChannelStrip"

#include "../Source/VST2/vst2_abi.h"
#include "../Source/DSP/M50.h"
#include "../Source/VST2/Params.h"

#include <dlfcn.h>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <algorithm>
#include <vector>

static int failures = 0;
static void check (bool ok, const char* what)
{
    std::printf ("  %s  %s\n", ok ? "ok  " : "FAIL", what);
    if (! ok) ++failures;
}

static int automateCalls = 0;
static VstIntPtr VST_CALL hostCallback (AEffect*, VstInt32 opcode, VstInt32, VstIntPtr, void*, float)
{
    switch (opcode)
    {
        case audioMasterVersion:   return 2400;
        case audioMasterAutomate:  ++automateCalls; return 0;
        default:                   return 0;
    }
}

typedef AEffect* (*EntryProc) (audioMasterCallback);

static VstIntPtr call (AEffect* e, VstInt32 op, VstInt32 idx = 0, VstIntPtr val = 0, void* ptr = 0, float opt = 0.0f)
{
    return e->dispatcher (e, op, idx, val, ptr, opt);
}

static bool finite (const std::vector<float>& x)
{
    for (size_t i = 0; i < x.size(); ++i)
        if (! (x[i] == x[i]) || std::fabs (x[i]) > 100.0f) return false;
    return true;
}

static void runBlocks (AEffect* e, std::vector<float>& l, std::vector<float>& r,
                       std::vector<float>& ol, std::vector<float>& orr, int block)
{
    for (size_t pos = 0; pos < l.size(); pos += block)
    {
        const int n = (int) ((l.size() - pos) < (size_t) block ? l.size() - pos : block);
        float* in[2]  = { &l[pos], &r[pos] };
        float* out[2] = { &ol[pos], &orr[pos] };
        e->processReplacing (e, in, out, n);
    }
}

int main (int argc, char** argv)
{
    if (argc < 2) { std::printf ("usage: %s <plugin binary>\n", argv[0]); return 2; }

    std::printf ("Loading %s\n", argv[1]);
    void* lib = dlopen (argv[1], RTLD_NOW | RTLD_LOCAL);
    if (! lib) { std::printf ("dlopen failed: %s\n", dlerror()); return 1; }

    EntryProc entry = (EntryProc) dlsym (lib, "VSTPluginMain");
    check (entry != 0, "exports VSTPluginMain");
#if defined(__APPLE__)
    check (dlsym (lib, "main_macho") != 0, "exports main_macho (older Mac hosts)");
#endif
    if (! entry) return 1;

    AEffect* e = entry (hostCallback);
    check (e != 0, "entry point returns an effect");
    if (! e) return 1;

    std::printf ("\nIdentity\n");
    check (e->magic == kEffectMagic, "magic is 'VstP'");
    check (e->numInputs == 2 && e->numOutputs == 2, "stereo in/out");
    check (e->numParams == tbm50::kNumParams && tbm50::kNumParams == 25, "25 parameters (11 per channel, master, bypass, link)");
    check ((e->flags & effFlagsCanReplacing) != 0, "processReplacing supported");
    check ((e->flags & effFlagsIsSynth) == 0, "not flagged as an instrument");
    check (e->initialDelay == 0, "no latency");
    check (e->uniqueID == VST_FOURCC ('T', 'B', 'm', '5'), "unique ID 'TBm5'");
    char buf[256];
    std::memset (buf, 0, sizeof (buf));
    call (e, effGetEffectName, 0, 0, buf);
    std::printf ("  name: %s, latency %d samples\n", buf, e->initialDelay);
    check (std::strcmp (buf, "TB M50 Dual Channel Strip") == 0, "effect name");
    check (call (e, effGetPlugCategory) == kPlugCategEffect, "category: effect");
    check (call (e, effGetVstVersion) == 2400, "VST 2.4");
    check (call (e, effCanDo, 0, 0, (void*) "receiveVstMidiEvent") == -1, "declines MIDI");

    call (e, effOpen);
    call (e, effSetSampleRate, 0, 0, 0, 44100.0f);
    call (e, effSetBlockSize, 0, 512);

    std::printf ("\nParameters\n");
    const char* expectDisp[25] = {
        "Line", "+0.0", "In", "6.12k", "+0.0", "1.58k", "+0.0", "224", "+0.0", "L100", "+0.0",
        "Line", "+0.0", "In", "6.12k", "+0.0", "1.58k", "+0.0", "224", "+0.0", "R100", "+0.0",
        "+0.0", "Off", "Off" };
    for (int i = 0; i < tbm50::kNumParams; ++i)
    {
        char name[64] = { 0 }, disp[64] = { 0 }, label[64] = { 0 };
        call (e, effGetParamName, i, 0, name);
        call (e, effGetParamDisplay, i, 0, disp);
        call (e, effGetParamLabel, i, 0, label);
        std::printf ("  %-8s = %-6s %-3s (%.2f)\n", name, disp, label, e->getParameter (e, i));
        check (std::strcmp (name, tbm50::kParamNames[i]) == 0 && std::strlen (name) <= 8 && std::strlen (disp) <= 8,
               "name and display string length");
        check (std::strcmp (disp, expectDisp[i]) == 0, "default value as on the panel");
    }
    e->setParameter (e, 1, 0.25f);
    check (std::fabs (e->getParameter (e, 1) - 0.25f) < 1e-6f, "set/get round trip");
    e->setParameter (e, 1, 7.0f);
    check (e->getParameter (e, 1) == 1.0f, "out-of-range values are clamped");
    e->setParameter (e, 1, 0.5f);
    struct { int p; float v; const char* want; const char* what; } shows[] = {
        { tbm50::kCh1 + tbm50::kCInput,  0.5f, "D.I.",   "Input switch shows D.I." },
        { tbm50::kCh1 + tbm50::kCTrim,   0.0f, "+12.0",  "D.I. trim starts at +12 dB" },
        { tbm50::kCh1 + tbm50::kCInput,  0.0f, "Mic",    "Input switch shows Mic" },
        { tbm50::kCh1 + tbm50::kCTrim,   1.0f, "+60.0",  "MIC trim tops out at +60 dB" },
        { tbm50::kCh1 + tbm50::kCInput,  1.0f, "Line",   "back to Line" },
        { tbm50::kCh1 + tbm50::kCTrim,   0.0f, "-15.0",  "LINE trim bottoms out at -15 dB" },
        { tbm50::kCh1 + tbm50::kCLoFreq, 0.0f, "50",     "LOW sweeps from 50 Hz" },
        { tbm50::kCh1 + tbm50::kCMidFreq,1.0f, "5.00k",  "MID sweeps to 5 kHz" },
        { tbm50::kCh1 + tbm50::kCHiFreq, 1.0f, "15.0k",  "HIGH sweeps to 15 kHz" },
        { tbm50::kCh1 + tbm50::kCHiGain, 0.0f, "-15.0",  "cut -15 dB" },
        { tbm50::kCh1 + tbm50::kCPan,    0.5f, "C",      "PAN centre shows C" },
        { tbm50::kCh1 + tbm50::kCFader,  1.0f, "+8.0",   "fader at 10 shows +8 dB" },
        { tbm50::kCh1 + tbm50::kCFader,  0.0f, "-inf",   "fader down shows -inf" },
        { tbm50::kCh1 + tbm50::kCBypass, 1.0f, "Bypass", "EQ bypass" },
    };
    for (size_t k = 0; k < sizeof (shows) / sizeof (shows[0]); ++k)
    {
        e->setParameter (e, shows[k].p, shows[k].v);
        std::memset (buf, 0, sizeof (buf));
        call (e, effGetParamDisplay, shows[k].p, 0, buf);
        if (std::strcmp (buf, shows[k].want) != 0) std::printf ("  got '%s'\n", buf);
        check (std::strcmp (buf, shows[k].want) == 0, shows[k].what);
    }
    std::memset (buf, 0, sizeof (buf));
    call (e, effGetParamLabel, tbm50::kCh1 + tbm50::kCHiFreq, 0, buf);
    check (std::strcmp (buf, "Hz") == 0, "frequency label is Hz");
    for (int i = 0; i < tbm50::kNumParams; ++i) e->setParameter (e, i, tbm50::kDefaults[i]);
    e->setParameter (e, 99, 0.5f);   // must be ignored, not crash
    check (e->getParameter (e, 99) == 0.0f, "invalid parameter index is harmless");

    std::printf ("\nProcessing\n");
    call (e, effMainsChanged, 0, 1);
    const int sr = 44100;
    std::vector<float> l (sr), r (sr), ol (sr), orr (sr);
    for (int i = 0; i < sr; ++i)
        l[i] = r[i] = (float) (0.2 * std::sin (2.0 * 3.14159265358979 * 196.0 * i / sr));

    // Reference: the DSP core directly, same settings as the plugin defaults
    tbm50::M50* refp = new tbm50::M50();
    tbm50::M50& ref = *refp;
    ref.prepare (sr);
    std::vector<float> refL (sr), refR (sr);
    ref.process (&l[0], &r[0], &refL[0], &refR[0], sr);
    delete refp;

    runBlocks (e, l, r, ol, orr, 512);
    double maxDiff = 0.0, maxLR = 0.0, pk = 0.0, ein = 0.0;
    for (int i = 0; i < sr; ++i)
    {
        maxDiff = std::max (maxDiff, (double) std::fabs (ol[i] - refL[i]));
        maxDiff = std::max (maxDiff, (double) std::fabs (orr[i] - refR[i]));
        if (i > sr / 2) { maxLR = std::max (maxLR, (double) std::fabs (ol[i] - orr[i])); ein += (double) ol[i] * ol[i] - (double) l[i] * l[i]; }
        pk = std::max (pk, (double) std::fabs (ol[i]));
    }
    std::printf ("  output peak %.3f, max diff vs DSP core %.2g, vs input %.2g\n", pk, maxDiff, ein);
    check (pk > 0.1, "produces output");
    check (maxDiff < 1e-6, "matches the DSP core sample for sample");
    {
        double si = 0.0; for (int i = sr / 2 + 1; i < sr; ++i) si += (double) l[i] * l[i];
        std::printf ("  level vs input: %+.3f dB\n", 10.0 * std::log10 ((si + ein) / si));
        check (std::fabs (10.0 * std::log10 ((si + ein) / si)) < 0.1, "defaults pass the source at unity");
    }
    check (maxLR < 0.001, "both sides alike at the defaults (channel 1 left, channel 2 right)");
    check (finite (ol) && finite (orr), "output finite");

    // Left feeds channel 1, right channel 2: a left-only source stays left
    {
        std::vector<float> a (sr), b (sr, 0.0f), oa (sr), ob (sr);
        for (int i = 0; i < sr; ++i) a[i] = (float) (0.2 * std::sin (2.0 * 3.14159265358979 * 330.0 * i / sr));
        runBlocks (e, a, b, oa, ob, 256);
        double pl = 0.0, pr = 0.0;
        for (int i = sr / 2; i < sr; ++i) { pl = std::max (pl, (double) std::fabs (oa[i])); pr = std::max (pr, (double) std::fabs (ob[i])); }
        std::printf ("  left-only input: left %.3f, right %.4f\n", pl, pr);
        check (std::fabs (pl - 0.2) < 0.005 && pr < 0.001, "stereo kept: left -> channel 1 -> left");
    }

    // Channel 2's fader down: only the left side
    {
        e->setParameter (e, tbm50::kCh2 + tbm50::kCFader, 0.0f);
        std::vector<float> a (l), b (r), oa (sr), ob (sr);
        runBlocks (e, a, b, oa, ob, 512);
        double pl = 0.0, pr = 0.0;
        for (int i = sr / 2; i < sr; ++i) { pl = std::max (pl, (double) std::fabs (oa[i])); pr = std::max (pr, (double) std::fabs (ob[i])); }
        check (pl > 0.1 && pr < 0.001, "channel 2 fader down leaves the right side empty");
        e->setParameter (e, tbm50::kCh2 + tbm50::kCFader, tbm50::kDefaults[tbm50::kCh2 + tbm50::kCFader]);
    }

    // In-place processing (output buffers are the input buffers)
    {
        std::vector<float> a (l), b (r);
        for (size_t pos = 0; pos < a.size(); pos += 333)
        {
            const int n = (int) ((a.size() - pos) < 333 ? a.size() - pos : 333);
            float* io[2] = { &a[pos], &b[pos] };
            e->processReplacing (e, io, io, n);
        }
        check (finite (a) && finite (b), "in-place processing, odd block size");
    }

    // Tiny and huge blocks
    {
        std::vector<float> a (l), b (r), oa (sr), ob (sr);
        runBlocks (e, a, b, oa, ob, 1);
        check (finite (oa) && finite (ob), "1-sample blocks");
        runBlocks (e, a, b, oa, ob, 8192);
        check (finite (oa) && finite (ob), "8192-sample blocks");
    }

    // Silence in -> settles to the console's hiss
    {
        std::vector<float> z (sr * 2, 0.0f), oa (sr * 2), ob (sr * 2);
        std::vector<float> z2 (sr * 2, 0.0f);
        runBlocks (e, z, z2, oa, ob, 512);
        double tail = 0.0;
        for (int i = sr; i < sr * 2; ++i) tail = std::max (tail, (double) std::fabs (oa[i]));
        std::printf ("  residual after 1 s of silence: %.2g (hiss)\n", tail);
        check (tail < 1e-3, "silence settles to the noise floor");
    }

    // BYPASS: the dry input, exactly, once the fade is over; and back again
    {
        std::vector<float> a (sr), b (sr), oa (sr), ob (sr);
        for (int i = 0; i < sr; ++i) { a[i] = (float) (0.3 * std::sin (0.031 * i)); b[i] = (float) (0.2 * std::sin (0.017 * i)); }
        e->setParameter (e, tbm50::kCh1 + tbm50::kCHiGain, 1.0f);       // something to hear when it is in
        e->setParameter (e, tbm50::kBypass, 1.0f);
        runBlocks (e, a, b, oa, ob, 512);
        double d = 0.0;
        for (int i = sr / 10; i < sr; ++i) d = std::max (d, (double) std::max (std::fabs (oa[i] - a[i]), std::fabs (ob[i] - b[i])));
        std::printf ("  BYPASS: largest difference from the input %.2g\n", d);
        check (d == 0.0, "BYPASS passes the input untouched");
        std::memset (buf, 0, sizeof (buf));
        call (e, effGetParamDisplay, tbm50::kBypass, 0, buf);
        check (std::strcmp (buf, "On") == 0, "Bypass shows On");
        e->setParameter (e, tbm50::kBypass, 0.0f);
        runBlocks (e, a, b, oa, ob, 512);
        d = 0.0;
        for (int i = sr / 10; i < sr; ++i) d = std::max (d, (double) std::fabs (oa[i] - a[i]));
        check (d > 0.01, "and out again (the EQ is heard)");
        // not tied to the host's bypass
        check (call (e, effCanDo, 0, 0, (void*) "bypass") != 1, "doesn't take over the host's bypass");
        call (e, effSetBypass, 0, 1);
        check (e->getParameter (e, tbm50::kBypass) == 0.0f, "the host's bypass leaves the BYPASS button alone");
        call (e, effSetBypass, 0, 0);
        e->setParameter (e, tbm50::kCh1 + tbm50::kCHiGain, 0.5f);
    }

    // LINK: channel 2 follows channel 1, pans apart
    {
        using namespace tbm50;
        e->setParameter (e, kCh1 + kCHiGain, 0.8f);
        e->setParameter (e, kCh1 + kCInput, 0.5f);
        e->setParameter (e, kCh1 + kCPan, 0.3f);
        e->setParameter (e, kLink, 1.0f);
        check (e->getParameter (e, kCh2 + kCHiGain) == 0.8f && e->getParameter (e, kCh2 + kCInput) == 0.5f,
               "LINK on copies channel 1 into channel 2");
        check (e->getParameter (e, kCh2 + kCPan) == 1.0f, "but leaves channel 2's PAN alone");
        e->setParameter (e, kCh1 + kCTrim, 0.6f);
        check (e->getParameter (e, kCh2 + kCTrim) == 0.6f, "channel 1's TRIM drives channel 2's");
        e->setParameter (e, kCh2 + kCTrim, 0.1f);
        check (e->getParameter (e, kCh2 + kCTrim) == 0.6f, "channel 2's own TRIM is ignored while linked");
        e->setParameter (e, kCh2 + kCPan, 0.7f);
        check (e->getParameter (e, kCh2 + kCPan) == 0.7f && e->getParameter (e, kCh1 + kCPan) == 0.3f, "the pans stay independent");
        // linked: a stereo source in comes out identical on both channels' paths
        e->setParameter (e, kCh1 + kCPan, 0.0f); e->setParameter (e, kCh2 + kCPan, 1.0f);
        std::vector<float> a (sr), oa (sr), ob (sr);
        for (int i = 0; i < sr; ++i) a[i] = (float) (0.1 * std::sin (0.05 * i));
        std::vector<float> b (a);
        runBlocks (e, a, b, oa, ob, 512);
        double d = 0.0;
        for (int i = sr / 2; i < sr; ++i) d = std::max (d, (double) std::fabs (oa[i] - ob[i]));
        std::printf ("  linked: largest L-R difference %.2g\n", d);
        check (d < 0.003, "linked channels process alike (apart from each channel's own hiss)");
        e->setParameter (e, kLink, 0.0f);
        e->setParameter (e, kCh2 + kCTrim, 0.2f);
        check (e->getParameter (e, kCh2 + kCTrim) == 0.2f && e->getParameter (e, kCh1 + kCTrim) == 0.6f, "LINK off: channel 2 is its own again");
        std::memset (buf, 0, sizeof (buf));
        call (e, effGetParamDisplay, kLink, 0, buf);
        check (std::strcmp (buf, "Off") == 0, "Link shows Off");
        for (int i = 0; i < kNumParams; ++i) e->setParameter (e, i, kDefaults[i]);
    }

    // processAccumulate adds
    {
        std::vector<float> a (512), b (512), oa (512, 1.0f), ob (512, 1.0f);
        for (int i = 0; i < 512; ++i) a[i] = b[i] = (float) (0.2 * std::sin (2.0 * 3.14159265358979 * 440.0 * i / 44100.0));
        float* in[2] = { &a[0], &b[0] };
        float* out[2] = { &oa[0], &ob[0] };
        e->process (e, in, out, 512);
        float mx = 0.0f; for (int i = 0; i < 512; ++i) mx = std::max (mx, oa[i]);
        check (mx > 1.1f, "process() accumulates into the outputs");
    }

    std::printf ("\nSample-rate change to 96 kHz\n");
    call (e, effMainsChanged, 0, 0);
    call (e, effSetSampleRate, 0, 0, 0, 96000.0f);
    call (e, effMainsChanged, 0, 1);
    {
        const int sr2 = 96000;
        std::vector<float> a (sr2), b (sr2), oa (sr2), ob (sr2);
        for (int i = 0; i < sr2; ++i) { a[i] = (float) (0.2 * std::sin (2.0 * 3.14159265358979 * 196.0 * i / sr2)); b[i] = 0.5f * a[i]; }
        std::clock_t t0 = std::clock();
        runBlocks (e, a, b, oa, ob, 512);
        const double secs = (double) (std::clock() - t0) / CLOCKS_PER_SEC;
        check (finite (oa) && finite (ob), "runs at 96 kHz");
        std::printf ("  CPU at 96 kHz (stereo source): %.1f%% of one core\n", 100.0 * secs);
    }

    std::printf ("\nAutomation while processing\n");
    {
        std::vector<float> a (sr), b (sr), oa (sr), ob (sr);
        for (int i = 0; i < sr; ++i) a[i] = b[i] = (float) (0.3 * std::sin (2.0 * 3.14159265358979 * 110.0 * i / sr));
        for (size_t pos = 0; pos < a.size(); pos += 64)
        {
            const float t = (float) pos / a.size();
            for (int p = 0; p < tbm50::kNumParams; ++p) e->setParameter (e, p, 0.5f + 0.5f * std::sin (6.2831853f * (p + 1) * t));
            const int n = (int) ((a.size() - pos) < 64 ? a.size() - pos : 64);
            float* in[2]  = { &a[pos], &b[pos] };
            float* out[2] = { &oa[pos], &ob[pos] };
            e->processReplacing (e, in, out, n);
        }
        double amax = 0.0; int nans = 0;
        for (size_t i = 0; i < oa.size(); ++i) { if (! (oa[i] == oa[i])) ++nans; else amax = std::max (amax, (double) std::fabs (oa[i])); }
        std::printf ("  automation: peak %.2f, NaNs %d\n", amax, nans);
        check (finite (oa), "all parameters automated together");
    }

    call (e, effMainsChanged, 0, 0);
    call (e, effClose);
    check (true, "effClose");
    dlclose (lib);

    std::printf ("\n%s (%d failure%s)\n", failures ? "FAILED" : "ALL HOST CHECKS PASSED", failures, failures == 1 ? "" : "s");
    return failures ? 1 : 0;
}
