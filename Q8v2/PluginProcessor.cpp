#include "PluginProcessor.h"
#include "PluginEditor.h"

using namespace juce;

const StringArray Q8Processor::hpfNames   { "Off", "40 Hz", "80 Hz", "120 Hz" };
const StringArray Q8Processor::lfNames    { "50", "140", "250", "400" };
const StringArray Q8Processor::mfNames    { "400", "700", "1.5k", "3k" };
const StringArray Q8Processor::hfNames    { "3k", "5k", "7.5k", "10k" };
const StringArray Q8Processor::ratioNames { "1.5:1", "2:1", "3:1", "4:1", "6:1", "10:1", "20:1" };

const float Q8Processor::hpfHz[]     { 0.f, 40.f, 80.f, 120.f };
// Centre frequencies and tuning capacitors read from the MM-310 BL schematic (drawing 3891):
// LF  S3: 50/140 Hz via C37 .15uF (L1), 250/400 Hz via C36 .1uF (L2)
// MF  S4: 400/700/1.5k/3k via C35 .1, C34 .068, C33 .03, C32 .015 uF (L3)
// HF  S5A: 3k/5k/7.5k/10k via C31 .015, C30 .01, C29 .0068, C28 .0047 uF (L4)
const float Q8Processor::lfHz[]  { 50.f, 140.f, 250.f, 400.f };
const float Q8Processor::lfCap[] { .15e-6f, .15e-6f, .1e-6f, .1e-6f };
const float Q8Processor::mfHz[]  { 400.f, 700.f, 1500.f, 3000.f };
const float Q8Processor::mfCap[] { .1e-6f, .068e-6f, .03e-6f, .015e-6f };
const float Q8Processor::hfHz[]  { 3000.f, 5000.f, 7500.f, 10000.f };
const float Q8Processor::hfCap[] { .015e-6f, .01e-6f, .0068e-6f, .0047e-6f };
const float Q8Processor::ratioVals[] { 1.5f, 2.f, 3.f, 4.f, 6.f, 10.f, 20.f };

AudioProcessorValueTreeState::ParameterLayout Q8Processor::createLayout()
{
    AudioProcessorValueTreeState::ParameterLayout l;
    auto dbRange = [] (float lo, float hi) { return NormalisableRange<float> (lo, hi, 0.1f); };
    auto pct = [] { return NormalisableRange<float> (0.f, 100.f, 0.1f); };

    // Preamp / transformer
    l.add (std::make_unique<AudioParameterBool>  (ParameterID { "preOn", 1 },  "Preamp On", true));
    l.add (std::make_unique<AudioParameterBool>  (ParameterID { "preUnity", 1 }, "Preamp Unity", false));
    l.add (std::make_unique<AudioParameterFloat> (ParameterID { "inGain", 1 }, "Input Gain", dbRange (-20.f, 40.f), 0.f));
    l.add (std::make_unique<AudioParameterFloat> (ParameterID { "xfmr", 1 },   "Transformer", pct(), 50.f));

    // Filter
    l.add (std::make_unique<AudioParameterChoice> (ParameterID { "hpf", 1 }, "High-Pass", hpfNames, 0));

    // Harmonics
    l.add (std::make_unique<AudioParameterFloat> (ParameterID { "harm", 1 },      "Harmonics", pct(), 0.f));
    l.add (std::make_unique<AudioParameterFloat> (ParameterID { "harmFocus", 1 }, "Harmonics Focus",
                                                  NormalisableRange<float> (800.f, 8000.f, 1.f, 0.5f), 2500.f));
    l.add (std::make_unique<AudioParameterFloat> (ParameterID { "harmBal", 1 },   "Even/Odd", pct(), 50.f));

    // EQ
    l.add (std::make_unique<AudioParameterBool>   (ParameterID { "eqOn", 1 }, "EQ On", true));
    l.add (std::make_unique<AudioParameterBool>   (ParameterID { "eqComp", 1 }, "EQ Gain Comp", false));
    l.add (std::make_unique<AudioParameterFloat>  (ParameterID { "lfG", 1 },  "LF Gain",  dbRange (-15.f, 15.f), 0.f));
    l.add (std::make_unique<AudioParameterChoice> (ParameterID { "lfF", 1 },  "LF Freq",  lfNames, 1));
    l.add (std::make_unique<AudioParameterBool>   (ParameterID { "soloLf", 1 }, "LF Solo", false));
    l.add (std::make_unique<AudioParameterFloat>  (ParameterID { "mfG", 1 },  "MF Gain",  dbRange (-15.f, 15.f), 0.f));
    l.add (std::make_unique<AudioParameterChoice> (ParameterID { "mfF", 1 },  "MF Freq",  mfNames, 1));
    l.add (std::make_unique<AudioParameterBool>   (ParameterID { "soloMf", 1 }, "MF Solo", false));
    l.add (std::make_unique<AudioParameterFloat>  (ParameterID { "hfG", 1 },  "HF Gain",  dbRange (-15.f, 15.f), 0.f));
    l.add (std::make_unique<AudioParameterChoice> (ParameterID { "hfF", 1 },  "HF Freq",  hfNames, 1));
    l.add (std::make_unique<AudioParameterBool>   (ParameterID { "soloHf", 1 }, "HF Solo", false));

    // Compressor
    l.add (std::make_unique<AudioParameterBool>   (ParameterID { "compOn", 1 }, "Comp On", false));
    l.add (std::make_unique<AudioParameterFloat>  (ParameterID { "thr", 1 },   "Threshold", dbRange (-40.f, 0.f), -18.f));
    l.add (std::make_unique<AudioParameterChoice> (ParameterID { "ratio", 1 }, "Ratio", ratioNames, 2));
    l.add (std::make_unique<AudioParameterFloat>  (ParameterID { "att", 1 },   "Attack",
                                                   NormalisableRange<float> (0.3f, 50.f, 0.01f, 0.4f), 10.f));
    l.add (std::make_unique<AudioParameterFloat>  (ParameterID { "rel", 1 },   "Release",
                                                   NormalisableRange<float> (40.f, 1500.f, 1.f, 0.5f), 250.f));
    l.add (std::make_unique<AudioParameterFloat>  (ParameterID { "makeup", 1 }, "Makeup", dbRange (0.f, 24.f), 0.f));
    l.add (std::make_unique<AudioParameterFloat>  (ParameterID { "mix", 1 },   "Comp Mix", pct(), 100.f));

    // Output
    l.add (std::make_unique<AudioParameterFloat>  (ParameterID { "outGain", 1 }, "Output", dbRange (-24.f, 12.f), 0.f));
    return l;
}

Q8Processor::Q8Processor()
    : AudioProcessor (BusesProperties().withInput ("Input", AudioChannelSet::stereo(), true)
                                       .withOutput ("Output", AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "STATE", createLayout())
{
}

bool Q8Processor::isBusesLayoutSupported (const BusesLayout& l) const
{
    auto in = l.getMainInputChannelSet(), out = l.getMainOutputChannelSet();
    return in == out && (in == AudioChannelSet::mono() || in == AudioChannelSet::stereo());
}

void Q8Processor::prepareToPlay (double sampleRate, int blockSize)
{
    sr = sampleRate;
    dsp::ProcessSpec spec { sampleRate, (uint32) blockSize, 2 };
    hpf.prepare (spec); hpf.reset();
    for (auto* b : { &bandLf, &bandMf, &bandHf, &soloLf, &soloMf, &soloHf, &exPre, &exPost1, &exPost2 }) b->reset();

    oversampling.initProcessing ((size_t) blockSize);
    oversampling.reset();
    setLatencySamples ((int) oversampling.getLatencyInSamples());

    for (int c = 0; c < 2; ++c) { coreState[c] = dcX[c] = dcY[c] = 0.f; }
    compEnvDb = 0.f;
    prevIn = Decibels::decibelsToGain (p ("inGain"));
    prevOut = Decibels::decibelsToGain (p ("outGain"));
    prevUnity = 1.f; prevEqComp = 1.f;
    smLf = smMf = smHf = 0.f; smEqCompDb = eqCompTargetDb = 0.f;
    lastHpf = lastLf = lastMf = lastHf = -1;
    lastLfG = lastMfG = lastHfG = 1e9f; lastFocus = -1.f;
    eqDirty = true;
}

// ---------------------------------------------------------------------------
// Nonlinear models
// ---------------------------------------------------------------------------

// Input transformer: the low band (below ~150 Hz) is pushed through a tanh "core"
// so bass saturates first and harder, which is the classic transformer behaviour.
float Q8Processor::transformer (float x, int ch, float amount) noexcept
{
    const float a = amount * 0.01f;
    const float coef = 1.f - std::exp (-MathConstants<float>::twoPi * 150.f / (float) (sr * 2.0));
    coreState[ch] += coef * (x - coreState[ch]);
    const float low = coreState[ch];
    const float high = x - low;
    // Core knee drops as the Transformer knob goes up (1.5 at 0% down to 0.3 at 100%), so bass saturates sooner.
    const float knee = 1.5f / (1.f + 4.f * a);
    const float shapedLow = knee * std::tanh (low / knee);
    return high + low + a * (shapedLow - low);
}

// Discrete class-A gain stage (AM-4B style): asymmetric tanh gives even + odd harmonics.
float Q8Processor::preampStage (float x) noexcept
{
    constexpr float ref = 1.5f, bias = 0.15f;   // knee ~ +3.5 dBFS: leaves headroom like a real console
    const float t = std::tanh (bias);
    const float norm = 1.f / (1.f - t * t);
    return ref * (std::tanh (x / ref + bias) - t) * norm;
}

// ---------------------------------------------------------------------------
// Filter design
// ---------------------------------------------------------------------------

// One EQ band from the MM-310 BL circuit. A2 is a differential amp (R11 = R12 = R13 = 10k, Rf = R14 = 24k).
// A series L-C branch (L derived from the schematic's tuning cap and switch frequency) is either
//  - boost: from the inverting leg to ground, so the stage gain is 1 + k*sC/D(s),  k = R14/(1+R14/R11)
//  - cut:   from the non-inverting leg to ground, forming a divider against R12||R13 = 5k:  D(s)/(D(s)+Rp*sC)
// with D(s) = LC s^2 + R C s + 1. R is solved for the requested peak gain; Qu models inductor loss (assumed).
void Q8Processor::designBand (Biquad& band, double fs, double f0, double capF, double gainDb, double Qu)
{
    if (std::abs (gainDb) < 0.05) { band.set (1, 0, 0, 1, 0, 0); return; }

    const double w0 = MathConstants<double>::twoPi * f0;
    const double L = 1.0 / (w0 * w0 * capF);
    const double Rloss = w0 * L / Qu;
    const double g = std::pow (10.0, gainDb / 20.0);
    const double A0 = 1.0 + 24000.0 / 10000.0, k = 24000.0 / A0, Rp = 5000.0;

    const double n2 = L * capF, d2 = n2;
    double n1, d1;
    if (gainDb > 0.0) { const double R = jmax (Rloss, k / (g - 1.0));       n1 = (R + k) * capF; d1 = R * capF; }
    else              { const double R = jmax (Rloss, Rp * g / (1.0 - g));  n1 = R * capF;       d1 = (R + Rp) * capF; }

    const double c = w0 / std::tan (w0 / (2.0 * fs)), c2 = c * c;
    band.set (n2 * c2 + n1 * c + 1.0, 2.0 * (1.0 - n2 * c2), n2 * c2 - n1 * c + 1.0,
              d2 * c2 + d1 * c + 1.0, 2.0 * (1.0 - d2 * c2), d2 * c2 - d1 * c + 1.0);
}

void Q8Processor::designBandpass (Biquad& b, double fs, double f0, double Q)
{
    const double w = MathConstants<double>::twoPi * f0 / fs, al = std::sin (w) / (2.0 * Q), c = std::cos (w);
    b.set (al, 0.0, -al, 1.0 + al, -2.0 * c, 1.0 - al);
}

void Q8Processor::designHighpass (Biquad& b, double fs, double f0, double Q)
{
    const double w = MathConstants<double>::twoPi * f0 / fs, al = std::sin (w) / (2.0 * Q), c = std::cos (w);
    b.set ((1.0 + c) * 0.5, -(1.0 + c), (1.0 + c) * 0.5, 1.0 + al, -2.0 * c, 1.0 - al);
}

// Average EQ level change across the spectrum, weighted equally per octave (like pink noise).
float Q8Processor::computeEqCompDb() const
{
    const int N = 96;
    double acc = 0.0; int cnt = 0;
    for (int k = 0; k < N; ++k)
    {
        const double f = 30.0 * std::pow (16000.0 / 30.0, (double) k / (double) (N - 1));
        if (f > sr * 0.45) break;
        const double w = MathConstants<double>::twoPi * f / sr;
        acc += bandLf.magSq (w) * bandMf.magSq (w) * bandHf.magSq (w);
        ++cnt;
    }
    if (cnt == 0 || acc <= 0.0) return 0.f;
    return (float) jlimit (-12.0, 12.0, -10.0 * std::log10 (acc / cnt));
}

void Q8Processor::updateFilters()
{
    const int h = (int) p ("hpf");
    if (h != lastHpf && h > 0)
        *hpf.state = *dsp::IIR::Coefficients<float>::makeHighPass (sr, hpfHz[h], 0.7071f);
    lastHpf = h;

    const int lfi = (int) p ("lfF"), mfi = (int) p ("mfF"), hfi = (int) p ("hfF");
    smLf += 0.35f * (p ("lfG") - smLf);
    smMf += 0.35f * (p ("mfG") - smMf);
    smHf += 0.35f * (p ("hfG") - smHf);

    if (lfi != lastLf || std::abs (smLf - lastLfG) > 0.01f)
    {
        designBand (bandLf, sr, lfHz[lfi], lfCap[lfi], smLf, 12.0);
        if (lfi != lastLf) designBandpass (soloLf, sr, lfHz[lfi], 1.0);
        lastLfG = smLf; lastLf = lfi; eqDirty = true;
    }
    if (mfi != lastMf || std::abs (smMf - lastMfG) > 0.01f)
    {
        designBand (bandMf, sr, mfHz[mfi], mfCap[mfi], smMf, 15.0);
        if (mfi != lastMf) designBandpass (soloMf, sr, mfHz[mfi], 1.0);
        lastMfG = smMf; lastMf = mfi; eqDirty = true;
    }
    if (hfi != lastHf || std::abs (smHf - lastHfG) > 0.01f)
    {
        const float fh = jmin (hfHz[hfi], (float) (sr * 0.45));
        designBand (bandHf, sr, fh, hfCap[hfi], smHf, 15.0);
        if (hfi != lastHf) designBandpass (soloHf, sr, fh, 1.0);
        lastHfG = smHf; lastHf = hfi; eqDirty = true;
    }
    if (eqDirty) { eqCompTargetDb = computeEqCompDb(); eqDirty = false; }
}

// ---------------------------------------------------------------------------

void Q8Processor::processBlock (AudioBuffer<float>& buffer, MidiBuffer&)
{
    ScopedNoDenormals noDenormals;
    const int n = buffer.getNumSamples();
    const int nCh = jmin (2, buffer.getNumChannels());
    for (int c = getTotalNumInputChannels(); c < getTotalNumOutputChannels(); ++c)
        buffer.clear (c, 0, n);

    const bool preOn = p ("preOn") > 0.5f, eqOn = p ("eqOn") > 0.5f, compOn = p ("compOn") > 0.5f;
    const bool unity = p ("preUnity") > 0.5f, eqComp = p ("eqComp") > 0.5f;
    const float harmAmt = p ("harm") * 0.01f, harmBal = p ("harmBal") * 0.01f, focus = p ("harmFocus");

    // 1. input trim
    const float inDb = p ("inGain");
    const float inG = Decibels::decibelsToGain (inDb);
    buffer.applyGainRamp (0, n, prevIn, inG);
    prevIn = inG;

    // 2. transformer + discrete preamp + harmonics generator (2x oversampled; always run so latency is constant)
    if (std::abs (focus - lastFocus) > 0.5f)
    {
        designHighpass (exPre, sr * 2.0, focus * 0.5, 0.7071);
        designHighpass (exPost1, sr * 2.0, focus, 0.7071);
        designHighpass (exPost2, sr * 2.0, focus, 0.7071);
        lastFocus = focus;
    }
    {
        dsp::AudioBlock<float> block (buffer);
        auto sub = block.getSubsetChannelBlock (0, (size_t) nCh);
        auto up = oversampling.processSamplesUp (sub);
        const float amt = p ("xfmr");
        // Harmonics: square (even) and cube (odd) of the signal above focus/2, high-passed so only
        // content above "focus" is added back. Scale constants set so full amount adds roughly -17 dBc at -12 dBFS.
        constexpr float kEven = 1.6f, kOdd = 8.f;
        for (int ch = 0; ch < nCh; ++ch)
        {
            auto* d = up.getChannelPointer ((size_t) ch);
            for (size_t i = 0; i < up.getNumSamples(); ++i)
            {
                float x = d[i];
                if (preOn) x = preampStage (transformer (x, ch, amt));
                if (harmAmt > 0.f)
                {
                    const float hh = exPre.process (x, ch);
                    const float e = hh * hh * kEven, o = hh * hh * hh * kOdd;
                    float hm = harmAmt * ((1.f - harmBal) * e + harmBal * o);
                    hm = exPost2.process (exPost1.process (hm, ch), ch);
                    x += hm;
                }
                d[i] = x;
            }
        }
        oversampling.processSamplesDown (sub);
    }
    if (preOn) // DC blocker (asymmetric stage creates offset), ~5 Hz
    {
        const float R = 1.f - MathConstants<float>::twoPi * 5.f / (float) sr;
        for (int ch = 0; ch < nCh; ++ch)
        {
            auto* d = buffer.getWritePointer (ch);
            for (int i = 0; i < n; ++i)
            {
                const float x = d[i];
                const float y = x - dcX[ch] + R * dcY[ch];
                dcX[ch] = x; dcY[ch] = y; d[i] = y;
            }
        }
    }

    // 2b. preamp unity: take back exactly what the input knob added
    const float unityDb = unity ? -inDb : 0.f;
    const float unityG = Decibels::decibelsToGain (unityDb);
    buffer.applyGainRamp (0, n, prevUnity, unityG);
    prevUnity = unityG;
    unityCompDb.store (unityDb);

    // 3. high-pass + EQ
    updateFilters();
    if (lastHpf > 0)
    {
        dsp::AudioBlock<float> block (buffer);
        auto sub = block.getSubsetChannelBlock (0, (size_t) nCh);
        dsp::ProcessContextReplacing<float> ctx (sub);
        hpf.process (ctx);
    }
    if (eqOn)
        for (int ch = 0; ch < nCh; ++ch)
        {
            auto* d = buffer.getWritePointer (ch);
            for (int i = 0; i < n; ++i)
                d[i] = bandHf.process (bandMf.process (bandLf.process (d[i], ch), ch), ch);
        }

    // 3b. EQ gain compensation (keeps average level steady as bands are boosted or cut)
    smEqCompDb += 0.3f * (eqCompTargetDb - smEqCompDb);
    const float eqCompDb = (eqOn && eqComp) ? smEqCompDb : 0.f;
    const float eqCompG = Decibels::decibelsToGain (eqCompDb);
    buffer.applyGainRamp (0, n, prevEqComp, eqCompG);
    prevEqComp = eqCompG;
    eqCompShownDb.store (eqCompDb);

    // 3c. band solo: listen to the region around each soloed band (after the EQ, so you hear its boost or cut)
    const bool sLf = p ("soloLf") > 0.5f, sMf = p ("soloMf") > 0.5f, sHf = p ("soloHf") > 0.5f;
    if (sLf || sMf || sHf)
        for (int ch = 0; ch < nCh; ++ch)
        {
            auto* d = buffer.getWritePointer (ch);
            for (int i = 0; i < n; ++i)
            {
                const float x = d[i];
                float y = 0.f;
                if (sLf) y += soloLf.process (x, ch);
                if (sMf) y += soloMf.process (x, ch);
                if (sHf) y += soloHf.process (x, ch);
                d[i] = y;
            }
        }

    // 4. compressor (stereo-linked, log-domain detector, soft knee)
    if (compOn)
    {
        const float thr = p ("thr"), ratio = ratioVals[(int) p ("ratio")];
        const float attC = std::exp (-1.f / (p ("att") * 0.001f * (float) sr));
        const float relC = std::exp (-1.f / (p ("rel") * 0.001f * (float) sr));
        const float mix = p ("mix") * 0.01f;
        const float mk = Decibels::decibelsToGain (p ("makeup"));
        const float slope = 1.f / ratio - 1.f, knee = 6.f;
        float grPeak = 0.f;

        for (int i = 0; i < n; ++i)
        {
            float det = 0.f;
            for (int ch = 0; ch < nCh; ++ch) det = jmax (det, std::abs (buffer.getReadPointer (ch)[i]));
            const float lvl = Decibels::gainToDecibels (det, -100.f);
            const float over = lvl - thr;
            float target = 0.f;
            if (2.f * over > knee)              target = slope * over;
            else if (2.f * over > -knee)        target = slope * square (over + knee * 0.5f) / (2.f * knee);
            const float c = target < compEnvDb ? attC : relC;
            compEnvDb = c * compEnvDb + (1.f - c) * target;
            const float g = Decibels::decibelsToGain (compEnvDb);
            grPeak = jmax (grPeak, -compEnvDb);
            for (int ch = 0; ch < nCh; ++ch)
            {
                auto* d = buffer.getWritePointer (ch);
                const float dry = d[i], wet = dry * g * mk;
                d[i] = dry * (1.f - mix) + wet * mix;
            }
        }
        grMeterDb.store (grPeak);
    }
    else { compEnvDb = 0.f; grMeterDb.store (0.f); }

    // 5. line-amp stage + output trim
    if (preOn)
        for (int ch = 0; ch < nCh; ++ch)
        {
            auto* d = buffer.getWritePointer (ch);
            for (int i = 0; i < n; ++i) d[i] = 2.f * std::tanh (d[i] * 0.5f) * 1.015f;   // line amp knee ~ +6 dBFS; 1.015 = calibration so -18 dBFS RMS program is unity
        }
    const float outG = Decibels::decibelsToGain (p ("outGain"));
    buffer.applyGainRamp (0, n, prevOut, outG);
    prevOut = outG;
}

AudioProcessorEditor* Q8Processor::createEditor() { return new Q8Editor (*this); }

void Q8Processor::getStateInformation (MemoryBlock& dest)
{
    if (auto xml = apvts.copyState().createXml()) copyXmlToBinary (*xml, dest);
}
void Q8Processor::setStateInformation (const void* data, int size)
{
    if (auto xml = getXmlFromBinary (data, size))
        if (xml->hasTagName (apvts.state.getType())) apvts.replaceState (ValueTree::fromXml (*xml));
}

AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new Q8Processor(); }
