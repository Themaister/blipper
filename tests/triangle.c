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

   file = sf_open("triangle.wav", SFM_WRITE, &out_info);
   if (!file)
      return EXIT_FAILURE;

   blip = blipper_new(taps, cutoff, beta, decimation, 2048, NULL);
   if (!blip)
      return EXIT_FAILURE;

#if BLIPPER_FIXED_POINT
#define BLIPPER_DELTA (0xfffe / 20)
#else
#define BLIPPER_DELTA 0.05
#endif
#define BLIPPER_PERIOD (44100 * 64 / (2 * 440))

   {
      unsigned s;
      blipper_long_sample_t integrator = 0; 
      blipper_long_sample_t delta = BLIPPER_DELTA;
      blipper_push_delta(blip, -BLIPPER_DELTA / 2, 0);


      for (i = 0; i < 1000; i++)
      {
         while (blipper_read_avail(blip) < 256)
         {
            blipper_push_delta(blip, delta, BLIPPER_PERIOD);
            delta = -delta;
         }

         blipper_read(blip, buffer, 256, 1);

         /* Leaky integrate a square wave to get a triangle. */
         for (s = 0; s < 256; s++)
         {
            blipper_long_sample_t res = buffer[s] + 0.999f * integrator;
            buffer[s] = integrator = res;
         }

#if BLIPPER_FIXED_POINT
         sf_writef_short(file, buffer, 256);
#else
         sf_writef_float(file, buffer, 256);
#endif
      }
   }

   sf_close(file);
   blipper_free(blip);
   return EXIT_SUCCESS;
}

