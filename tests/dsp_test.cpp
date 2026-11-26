// Usage: dsp_test <in.f32> <out.f32> <sampleRate> <source> <delayPct> <mixPct> <blockSize>
// Input/output: interleaved stereo float32.
#include "../Source/PseudoStereoDSP.h"
#include <cstdio>
#include <cstdlib>
#include <vector>

int main (int argc, char** argv)
{
    if (argc < 8) return 2;
    const double sr = std::atof (argv[3]);
    const int source = std::atoi (argv[4]);
    const double delayPct = std::atof (argv[5]), mixPct = std::atof (argv[6]);
    const int block = std::atoi (argv[7]);

    FILE* f = std::fopen (argv[1], "rb");
    std::vector<float> inter;
    float tmp[4096];
    size_t n;
    while ((n = std::fread (tmp, sizeof (float), 4096, f)) > 0) inter.insert (inter.end(), tmp, tmp + n);
    std::fclose (f);

    const int frames = (int) (inter.size() / 2);
    std::vector<float> L (frames), R (frames);
    for (int i = 0; i < frames; ++i) { L[i] = inter[2 * i]; R[i] = inter[2 * i + 1]; }

    pseudostereo::PseudoStereo dsp;
    dsp.prepare (sr);
    for (int start = 0; start < frames; start += block)
    {
        const int len = std::min (block, frames - start);
        dsp.setParameters (source, delayPct, mixPct);
        dsp.process (L.data() + start, R.data() + start, len);
    }

    FILE* o = std::fopen (argv[2], "wb");
    for (int i = 0; i < frames; ++i) { std::fwrite (&L[i], sizeof (float), 1, o); std::fwrite (&R[i], sizeof (float), 1, o); }
    std::fclose (o);
    return 0;
}
