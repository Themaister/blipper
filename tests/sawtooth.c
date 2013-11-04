#include "../blipper.h"
#include <sndfile.h>
#include <stdlib.h>

int main(void)
{
   SF_INFO out_info = {0};
   SNDFILE *file;

   blipper_sample_t buffer[256];
   blipper_t *blip;

   unsigned i;
   unsigned taps = 256, decimation = 64;
   double cutoff = 0.90, beta = 10.0;

   out_info.samplerate = 44100;
#if BLIPPER_FIXED_POINT
   out_info.format = SF_FORMAT_WAV | SF_FORMAT_PCM_16;
#else
   out_info.format = SF_FORMAT_WAV | SF_FORMAT_FLOAT;
#endif
   out_info.channels = 1;

   file = sf_open("sawtooth.wav", SFM_WRITE, &out_info);
   if (!file)
      return EXIT_FAILURE;

   blip = blipper_new(taps, cutoff, beta, decimation, 2048, NULL);
   if (!blip)
      return EXIT_FAILURE;

#if BLIPPER_FIXED_POINT
#define BLIPPER_DELTA 0xfffe
#else
#define BLIPPER_DELTA 2.0
#endif
#define BLIPPER_PERIOD (44100 * 64 / 200)

   blipper_set_ramp(blip, BLIPPER_DELTA, BLIPPER_PERIOD);
   blipper_push_delta(blip, -BLIPPER_DELTA / 2, 0);

   for (i = 0; i < 1000; i++)
   {
      while (blipper_read_avail(blip) < 256)
         blipper_push_delta(blip, -BLIPPER_DELTA, BLIPPER_PERIOD);

      blipper_read(blip, buffer, 256, 1);

#if BLIPPER_FIXED_POINT
      sf_writef_short(file, buffer, 256);
#else
      sf_writef_float(file, buffer, 256);
#endif
   }

   sf_close(file);
   blipper_free(blip);
   return EXIT_SUCCESS;
}

