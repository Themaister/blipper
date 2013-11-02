#include <stdlib.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <math.h>
#include <stdbool.h>

#include <sndfile.h>
#include <time.h>

typedef struct blipper blipper_t;

struct blipper
{
   float *output_buffer;
   unsigned output_avail;
   unsigned output_buffer_samples;

   float *filter_bank;

   unsigned phase;
   unsigned phases;
   unsigned phases_log2;
   unsigned taps;

   float integrator;
   float amp;
   float last_sample;
};

void blipper_free(blipper_t *blip)
{
   if (blip)
   {
      free(blip->filter_bank);
      free(blip->output_buffer);
      free(blip);
   }
}

static inline double besseli0(double x)
{
   double sum = 0.0;

   double factorial = 1.0;
   double factorial_mult = 0.0;
   double x_pow = 1.0;
   double two_div_pow = 1.0;
   double x_sqr = x * x;

   // Approximate. This is an infinite sum.
   // Luckily, it converges rather fast.
   for (unsigned i = 0; i < 18; i++)
   {
      sum += x_pow * two_div_pow / (factorial * factorial);

      factorial_mult += 1.0;
      x_pow *= x_sqr;
      two_div_pow *= 0.25;
      factorial *= factorial_mult;
   }

   return sum;
}

static inline double sinc(double v)
{
   if (fabs(v) < 0.00001)
      return 1.0;
   else
      return sin(v) / v;
}

// index range = [-1, 1)
static inline double kaiser_window(double index, double beta)
{
   return besseli0(beta * sqrt(1.0 - index * index));
}

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

static float *blipper_create_sinc(unsigned phases, unsigned taps, double cutoff, double beta)
{
   float *filter = malloc(phases * taps * sizeof(*filter));
   if (!filter)
      return NULL;

   double sidelobes = taps / 2.0;
   double window_mod = 1.0 / kaiser_window(0.0, beta);
   unsigned filter_len = phases * taps;
   for (unsigned i = 0; i < filter_len; i++)
   {
      double window_phase = (double)i / filter_len; // [0, 1)
      window_phase = 2.0 * window_phase - 1.0; // [-1, 1)
      double sinc_phase = window_phase * sidelobes; // [-taps / 2, taps / 2)

      filter[i] = cutoff * sinc(M_PI * sinc_phase * cutoff) * kaiser_window(window_phase, beta) * window_mod;
   }

   return filter;
}

// We differentiate and integrate at different sample rates.
// Differentiation is D(z) = 1 - z^-1 and happens when delta impulses are convolved.
// Integration step after decimation by D is 1 / (1 - z^-D).
//
// If our sinc filter is S(z) we'd have a response of S(z) * (1 - z^-1) / (1 - z^-D) after blipping.
// Compensate by prefiltering S(z) with the inverse (1 - z^-D) / (1 - z^-1). This filtering creates a finite length filter, albeit slightly longer.
//
// phases is the same as decimation rate.
static float *blipper_prefilter_sinc(float *filter, unsigned phases, unsigned *out_taps)
{
   unsigned taps = *out_taps;
   float *new_filter = malloc((phases * taps + phases) * sizeof(*filter));
   if (!new_filter)
      goto error;

   filter = realloc(filter, (phases * taps + phases) * sizeof(*filter));
   if (!filter)
      goto error;

   // Integrate.
   new_filter[0] = filter[0];
   for (unsigned i = 1; i < phases * taps; i++)
      new_filter[i] = new_filter[i - 1] + filter[i];
   for (unsigned i = phases * taps; i < phases * taps + phases; i++)
      new_filter[i] = new_filter[phases * taps - 1];

   // Differentiate with offset of D.
   memcpy(filter, new_filter, phases * sizeof(*filter));
   for (unsigned i = phases; i < phases * taps + phases; i++)
      filter[i] = new_filter[i] - new_filter[i - phases];

   *out_taps = taps + 1;
   free(new_filter);
   return filter;

error:
   free(new_filter);
   free(filter);
   return NULL;
}

// Creates a polyphase filter bank. Interleaves the filter for cache coherency and possibilities for SIMD processing.
static float *blipper_interleave_sinc(float *filter, unsigned phases, unsigned taps)
{
   float *new_filter = malloc(phases * taps * sizeof(*filter));
   if (!new_filter)
      goto error;

   for (unsigned t = 0; t < taps; t++)
      for (unsigned p = 0; p < phases; p++)
         new_filter[p * taps + t] = filter[t * phases + p];

   free(filter);
   return new_filter;

error:
   free(new_filter);
   free(filter);
   return NULL;
}

static bool blipper_create_filter_bank(blipper_t *blip, unsigned taps, double cutoff, double beta)
{
   float *sinc_filter = blipper_create_sinc(blip->phases, taps, cutoff, beta);
   if (!sinc_filter)
      return false;

   sinc_filter = blipper_prefilter_sinc(sinc_filter, blip->phases, &taps);
   if (!sinc_filter)
      return false;
   sinc_filter = blipper_interleave_sinc(sinc_filter, blip->phases, taps);
   if (!sinc_filter)
      return false;

   blip->filter_bank = sinc_filter;
   blip->taps = taps;
   return true;
}

blipper_t *blipper_new(unsigned taps, double cutoff, double beta, unsigned decimation, unsigned buffer_samples)
{
   if ((decimation & (decimation - 1)) != 0)
   {
      fprintf(stderr, "[blipper]: Decimation factor must be POT.\n");
      return NULL;
   }

   blipper_t *blip = calloc(1, sizeof(*blip));
   if (!blip)
      return NULL;

   blip->phases = decimation;
   blip->phases_log2 = (unsigned)log2(decimation);
   blip->amp = 1.0f / decimation;

   if (!blipper_create_filter_bank(blip, taps, cutoff, beta))
      goto error;

   blip->output_buffer = calloc(buffer_samples + blip->taps, sizeof(*blip->output_buffer));
   if (!blip->output_buffer)
      goto error;
   blip->output_buffer_samples = buffer_samples + blip->taps;

   return blip;

error:
   blipper_free(blip);
   return NULL;
}

void blipper_push_delta(blipper_t *blip, float delta, unsigned clocks_step)
{
   blip->phase += clocks_step;

   unsigned target_output = (blip->phase + blip->phases - 1) >> blip->phases_log2;

   unsigned filter_phase = (target_output << blip->phases_log2) - blip->phase;
   const float *response = blip->filter_bank + blip->taps * filter_phase;

   float *target = blip->output_buffer + target_output;
   unsigned taps = blip->taps;
   for (unsigned i = 0; i < taps; i++)
      target[i] += delta * response[i];

   blip->output_avail = target_output;
}

void blipper_push_samples(blipper_t *blip, const float *data, unsigned samples,
      unsigned stride)
{
   unsigned clocks_skip = 0;
   float last = blip->last_sample;

   for (unsigned s = 0; s < samples; s++, data += stride)
   {
      float val = *data;
      if (val != last)
      {
         blipper_push_delta(blip, val - last, clocks_skip + 1);
         clocks_skip = 0;
         last = val;
      }
      else
         clocks_skip++;
   }

   blip->phase += clocks_skip;
   blip->output_avail = (blip->phase + blip->phases - 1) >> blip->phases_log2;
   blip->last_sample = last;
}

unsigned blipper_read_avail(blipper_t *blip)
{
   return blip->output_avail;
}

void blipper_read(blipper_t *blip, float *output, unsigned samples, unsigned stride)
{
   float sum = blip->integrator;
   const float *out = blip->output_buffer;

   for (unsigned s = 0; s < samples; s++, output += stride)
   {
      sum += out[s];
      *output = blip->amp * sum;
   }

   // Don't bother with ring buffering.
   // The entire buffer should be read out ideally anyways.
   memmove(blip->output_buffer, blip->output_buffer + samples, (blip->output_avail + blip->taps - samples) * sizeof(*out));
   memset(blip->output_buffer + blip->taps, 0, samples * sizeof(*out));
   blip->output_avail -= samples;
   blip->phase -= samples << blip->phases_log2;

   blip->integrator = sum;
}

static double get_time(void)
{
   struct timespec tv;
   clock_gettime(CLOCK_MONOTONIC, &tv);
   return tv.tv_sec + tv.tv_nsec / 1000000000.0;
}

int main(int argc, char *argv[])
{
   if (argc != 3)
   {
      fprintf(stderr, "Usage: %s <infile> <outfile>\n", argv[0]);
      return EXIT_FAILURE;
   }

   blipper_t *blip_l = blipper_new(64, 0.85, 8.0, 64, 1024);
   blipper_t *blip_r = blipper_new(64, 0.85, 8.0, 64, 1024);
   if (!blip_l || !blip_r)
      return EXIT_FAILURE;

   SF_INFO in_info;
   SNDFILE *in_file = sf_open(argv[1], SFM_READ, &in_info);
   if (!in_file)
      return EXIT_FAILURE;

   SF_INFO info = {
      .samplerate = in_info.samplerate / 64,
      .channels = 2,
      .format = SF_FORMAT_WAV | SF_FORMAT_FLOAT,
   };
   SNDFILE *file = sf_open(argv[2], SFM_WRITE, &info);
   if (!file)
      return EXIT_FAILURE;

   float output_buffer[8 * 1024 / 64];
   float input_buffer[8 * 1024];

   double time_total = 0.0;
   unsigned read_frames = 0;

   for (;;)
   {
      unsigned read_count = sf_readf_float(in_file, input_buffer, 4 * 1024);
      if (!read_count)
         break;

      read_frames += read_count;

      double time_start = get_time();
      blipper_push_samples(blip_l, input_buffer + 0, read_count, 2);
      blipper_push_samples(blip_r, input_buffer + 1, read_count, 2);

      unsigned avail = blipper_read_avail(blip_l);

      blipper_read(blip_l, output_buffer + 0, avail, 2);
      blipper_read(blip_r, output_buffer + 1, avail, 2);
      time_total += get_time() - time_start;

      sf_writef_float(file, output_buffer, avail);
   }

   fprintf(stderr, "Processed %f seconds of input in %f seconds.\n",
         (double)read_frames / in_info.samplerate, time_total);

   sf_close(file);
   sf_close(in_file);
   blipper_free(blip_l);
   blipper_free(blip_r);
}

