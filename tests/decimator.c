#include "../blipper.h"
#include <sndfile.h>
#include <stdio.h>
#include <stdlib.h>

int main(int argc, char *argv[])
{
   SF_INFO in_info = {0}, out_info = {0};
   SNDFILE *in_file, *out_file;
   blipper_sample_t *input_buffer, *output_buffer;
   blipper_t *blip[8];

   unsigned c, channels;
   unsigned taps = 32, decimation = 64;
   double cutoff = 0.85, beta = 6.5;

   if (argc < 3)
   {
      fprintf(stderr, "Usage: %s <in-file> <out-file> [taps = 32] [decimation = 64] [cutoff = 0.85] [beta = 8.0]\n", argv[0]);
      return EXIT_FAILURE;
   }

   in_file = sf_open(argv[1], SFM_READ, &in_info);
   if (!in_file)
      return EXIT_FAILURE;

   if (argc > 3)
      taps = strtoul(argv[3], NULL, 0);
   if (argc > 4)
      decimation = strtoul(argv[4], NULL, 0);
   if (argc > 5)
      cutoff = strtod(argv[5], NULL);
   if (argc > 6)
      beta = strtod(argv[6], NULL);

   channels = in_info.channels;

   out_info.samplerate = in_info.samplerate / decimation;
   out_info.channels = in_info.channels;
   out_info.format = in_info.format;

   out_file = sf_open(argv[2], SFM_WRITE, &out_info);
   if (!out_file)
      return EXIT_FAILURE;

   for (c = 0; c < channels; c++)
   {
      blip[c] = blipper_new(taps, cutoff, beta, decimation, 1024);
      if (!blip[c])
         return EXIT_FAILURE;
   }

   input_buffer = malloc(1024 * decimation * channels * sizeof(*input_buffer));
   output_buffer = malloc(1024 * channels * sizeof(*output_buffer));
   if (!input_buffer || !output_buffer)
      return EXIT_FAILURE;

   for (;;)
   {
      unsigned avail;
      unsigned read_frames = sf_readf_short(in_file, input_buffer, 1024);
      if (!read_frames)
         break;

      for (c = 0; c < channels; c++)
         blipper_push_samples(blip[c], input_buffer + c, read_frames, channels);

      avail = blipper_read_avail(blip[0]);
      for (c = 0; c < channels; c++)
         blipper_read(blip[c], output_buffer + c, avail, channels);

      sf_writef_short(out_file, output_buffer, avail);
   }

   sf_close(in_file);
   sf_close(out_file);

   for (c = 0; c < channels; c++)
      blipper_free(blip[c]);

   return EXIT_SUCCESS;
}

