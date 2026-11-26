"""
Compares the C++ DSP core against a literal transcription of Pseudostereo.ny
(each lp/hp/feedback-delay call written out separately, using Nyquist's TONE/ATONE
difference equations) on the same test signal.

Run:  g++ -O2 -std=c++17 tests/dsp_test.cpp -o dsp_test && python3 tests/compare_with_ny.py ./dsp_test
"""
import subprocess, sys, numpy as np
from scipy.signal import lfilter

exe = sys.argv[1] if len(sys.argv) > 1 else "./dsp_test"

def coeffs(hz, sr):
    b = 2.0 - np.cos(2 * np.pi * hz / sr)
    c2 = b - np.sqrt(b * b - 1.0)
    return 1.0 - c2, c2

def lp(x, hz, sr):                      # Nyquist TONE:   y[n] = c1*x[n] + c2*y[n-1]
    c1, c2 = coeffs(hz, sr)
    return lfilter([c1], [1.0, -c2], x)

def hp(x, hz, sr):                      # Nyquist ATONE:  y[n] = c2*(y[n-1] + x[n] - x[n-1])
    _, c2 = coeffs(hz, sr)
    return lfilter([c2, -c2], [1.0, -c2], x)

def fdelay(x, seconds, sr):             # (feedback-delay x seconds 0): pure delay, zero-filled
    d = int(round(seconds * sr))
    out = np.zeros_like(x)
    if d < len(x):
        out[d:] = x[:len(x) - d]
    return out

def reference(x, sr, source, df_pct, mix_pct):
    df, mix = df_pct / 100.0, mix_pct / 100.0
    s = x[:, source].astype(np.float64)
    def split(s, d2, d4):
        f1 = lp(s, 300, sr)
        f2 = fdelay(lp(hp(s, 300, sr), 600, sr), df * d2, sr)
        f3 = lp(hp(s, 600, sr), 1200, sr)
        f4 = fdelay(lp(hp(s, 1200, sr), 2400, sr), df * d4, sr)
        f5 = hp(s, 2400, sr)
        return f1 + f2 + f3 + f4 + f5
    L = mix * split(s, 0.2, 0.08) + (1 - mix) * s
    R = mix * split(s, 0.14, 0.06) + (1 - mix) * s
    return np.stack([L, R], axis=1)

def run_cpp(x, sr, source, df, mix, block):
    x.astype(np.float32).tofile("/tmp/in.f32")
    subprocess.check_call([exe, "/tmp/in.f32", "/tmp/out.f32", str(sr), str(source), str(df), str(mix), str(block)])
    return np.fromfile("/tmp/out.f32", dtype=np.float32).reshape(-1, 2).astype(np.float64)

rng = np.random.default_rng(1)
sr = 48000
n = sr * 2
t = np.arange(n) / sr
sig = (0.3 * rng.standard_normal(n) + 0.4 * np.sin(2 * np.pi * 440 * t) + 0.2 * np.sin(2 * np.pi * 1700 * t))
x = np.stack([sig, 0.5 * rng.standard_normal(n)], axis=1)   # R differs, to check source selection
x[0, :] = 1.0  # include an impulse at t=0

ok = True
# delays below land on whole samples at 48 kHz, so the C++ fractional delay == Nyquist's integer delay
for source, df, mix in [(0, 30, 80), (1, 100, 100), (0, 0, 50), (1, 60, 20)]:
    ref = reference(x, sr, source, df, mix)
    for block in (n, 512, 37):          # block-size independence
        out = run_cpp(x, sr, source, df, mix, block)
        err = np.max(np.abs(out - ref))
        status = "OK " if err < 1e-5 else "FAIL"
        ok &= err < 1e-5
        print(f"{status} source={source} delay={df:3d}% mix={mix:3d}% block={block:6d}  max|err|={err:.2e}")

# filter sanity: TONE should be -3.01 dB at its cutoff
for hz in (300, 2400):
    c1, c2 = coeffs(hz, sr)
    w = 2 * np.pi * hz / sr
    mag = abs(c1 / (1 - c2 * np.exp(-1j * w)))
    print(f"lp {hz} Hz gain at cutoff: {20*np.log10(mag):.2f} dB")

# sample rate below 4800 Hz: pass the chosen source to both channels
out = run_cpp(x[:2000], 4000, 1, 30, 80, 256)
byp = np.allclose(out[:, 0], out[:, 1]) and np.allclose(out[:, 0], x[:2000, 1], atol=1e-6)
print("OK " if byp else "FAIL", "low-sample-rate bypass")
ok &= byp

# NaN / inf check on a loud, long signal with a parameter change
loud = np.stack([rng.uniform(-1, 1, sr * 5)] * 2, axis=1)
out = run_cpp(loud, 44100, 0, 100, 100, 480)
fin = np.isfinite(out).all()
print("OK " if fin else "FAIL", "finite output, peak =", float(np.max(np.abs(out))))
ok &= fin

print("\nALL PASSED" if ok else "\nFAILURES")
sys.exit(0 if ok else 1)
