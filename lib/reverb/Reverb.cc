#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif
/*
        Reverb.cc

        Copyright 2002-14 Tim Goetze <tim@quitte.de>

        http://quitte.de/dsp/

        License: LGPL-2.1-or-later
*/

#include "Reverb.h"

#include <math.h>

using namespace JVRev;

static sample_t
getport(void* p) {
    return *(sample_t*)p;
}

void JVRev::set_t60(sample_t val) {
    t60 = val;
    val *= .001 * sample_rate;
    sample_t g = pow(10, -3. / val);

    for (uint i = 0; i < 4; i++)
        comb[i].set_gain(g);
}

void JVRev::setup(sample_rate_t sr, sample_t** p) {
    sample_rate = sr;
    ports = p;

    sample_t d[] = {.0297, .0371, .0411, .0437, .005, .0017};
    for (uint i = 0; i < 4; i++)
        comb[i].setup(d[i] * sr);
    for (uint i = 0; i < 2; i++)
        allpass[i].setup(d[i + 4] * sr);

    set_t60(getport(ports[1]));
}

void JVRev::cycle(uint frames) {
    sample_t bw = .005 + .994 * getport(0);
    bandwidth.set(exp(-M_PI * (1. - bw)));

    if (t60 != *ports[1])
        set_t60(getport(1));

    double wet = getport(2);
    double dry = 1. - wet;

    sample_t* in = ports[3];
    sample_t* out = ports[4];

    for (uint i = 0; i < frames; i++) {
        sample_t s = in[i];
        sample_t v = bandwidth.process(s);

        sample_t c = 0;
        for (uint j = 0; j < 4; j++)
            c += comb[j].process(v);

        sample_t a = c * .25;
        for (uint j = 0; j < 2; j++)
            a = allpass[j].process(a);

        out[i] = dry * s + wet * a;
    }
}

#if 0
void
Plate::cycle (uint frames)
{
	sample_t bw = .005 + .994*getport(0);
	input.bandwidth.set (exp (-M_PI * (1. - bw)));

	sample_t decay = .749*getport(1);
	double damping = .0005 + .9995*getport(2);
	sample_t d = exp (-M_PI * damping);

	for (uint i = 0; i < 4; i++)
	{
		loop[i].damp.set (d);
		loop[i].decay = decay;
	}

	double wet = getport(3);
	double dry = 1. - wet;

	sample_t * in = ports[4];
	sample_t * out = ports[5];

	for (uint i = 0; i < frames; i++)
	{
		sample_t s = in[i];
		sample_t v = input.bandwidth.process (s);
		v = input.diffuser[0].process (v);
		v = input.diffuser[1].process (v);

		// ...
	}
}
#endif
