/*
 * Mpeg Layer-1,2,3 audio decoder
 * ------------------------------
 * copyright (c) 1995,1996,1997 by Michael Hipp, All rights reserved.
 * See also 'README'
 *
 * slighlty optimized for machines without autoincrement/decrement.
 * The performance is highly compiler dependend. Maybe
 * the decode.c version for 'normal' processor may be faster
 * even for Intel processors.
 */

#include <stdlib.h>
#include <math.h>
#include <string.h>

#include "mpg123.h"
#include "mpglib.h"

#if defined(PANDORA) || defined(ODROID)
/* This is defined in assembler. */
int synth_1to1_neon_asm(short *window, short *b0, short *samples, int bo1);
int synth_1to1_s_neon_asm(short *window, short *b0l, short *b0r, short *samples, int bo1);
void dct64_neon(short *out0, short *out1, float *samples);
/* Hull for C mpg123 API */
int synth_1to1_neon(struct StaticData * psd, struct mpstr * gmp, float *bandPtr,int channel,unsigned char *out,int *pnt)
{
  short *samples = (short *) (out + *pnt);
  float *b0,(*buf)[0x110];
  int clip; 
  int bo1;

  if(!channel)
  {
    gmp->synth_bo--;
    gmp->synth_bo &= 0xf;
    buf = gmp->synth_buffs[0];
  }
  else
  {
    samples++;
    buf = gmp->synth_buffs[1];
  }

  if(gmp->synth_bo & 0x1) 
  {
    b0 = buf[0];
    bo1 = gmp->synth_bo;
    dct64_neon(buf[1]+((gmp->synth_bo+1)&0xf),buf[0]+gmp->synth_bo,bandPtr);
  }
  else
  {
    b0 = buf[1];
    bo1 = gmp->synth_bo+1;
    dct64_neon(buf[0]+gmp->synth_bo,buf[1]+gmp->synth_bo+1,bandPtr);
  }

  clip = synth_1to1_neon_asm((bo1&1)?psd->decwins1+1:psd->decwins, b0, samples, bo1);

  *pnt += 128;

  return clip;
}

int tsynth_1to1_neon(struct StaticData * psd, float *bandPtr,int channel,unsigned char *out,int *pnt)
{
  short *samples = (short *) (out + *pnt);
  float *b0,(*buf)[0x110];
  int clip; 
  int bo1;

  if(!channel)
  {
    psd->bo--;
    psd->bo &= 0xf;
    buf = psd->buffs[0];
  }
  else
  {
    samples++;
    buf = psd->buffs[1];
  }

  if(psd->bo & 0x1) 
  {
    b0 = buf[0];
    bo1 = psd->bo;
    dct64_neon(buf[1]+((psd->bo+1)&0xf),buf[0]+psd->bo,bandPtr);
  }
  else
  {
    b0 = buf[1];
    bo1 = psd->bo+1;
    dct64_neon(buf[0]+psd->bo,buf[1]+psd->bo+1,bandPtr);
  }

  clip = synth_1to1_neon_asm((bo1&1)?psd->decwins1+1:psd->decwins, b0, samples, bo1); // to avoid bus error

  *pnt += 128;

  return clip;
}
/*
int synth_1to1_stereo_neon(struct StaticData * psd, struct mpstr * gmp, float *bandPtr_l,float *bandPtr_r,int channel,unsigned char *out,int *pnt)
{
  short *samples = (short *) (out + *pnt);
  short *b0l, *b0r, **bufl, **bufr;
  int clip; 
  int bo1;

  gmp->synth_bo--;
  gmp->synth_bo &= 0xf;
  bufl = gmp->synth_buffs[0];
  bufr = gmp->synth_buffs[1];

  if(gmp->synth_bo & 0x1) 
  {
    b0l = bufl[0];
    b0r = bufr[0];
    bo1 = gmp->synth_bo;
    dct64_neon(bufl[1]+((gmp->synth_bo+1)&0xf),bufl[0]+gmp->synth_bo,bandPtr_l);
    dct64_neon(bufr[1]+((gmp->synth_bo+1)&0xf),bufr[0]+gmp->synth_bo,bandPtr_r);
  }
  else
  {
    b0l = bufl[1];
    b0r = bufr[1];
    bo1 = gmp->synth_bo+1;
    dct64_neon(bufl[0]+gmp->synth_bo,bufl[1]+gmp->synth_bo+1,bandPtr_l);
    dct64_neon(bufr[0]+gmp->synth_bo,bufr[1]+gmp->synth_bo+1,bandPtr_r);
  }

  clip = synth_1to1_s_neon_asm((short *)psd->decwin, b0l, b0r, samples, bo1);

  *pnt += 128;

  return clip;
}
*/
#endif

 /* old WRITE_SAMPLE */
#define WRITE_SAMPLE(samples,sum,clip) \
  if( (sum) > 32767.0) { *(samples) = 0x7fff; (clip)++; } \
  else if( (sum) < -32768.0) { *(samples) = -0x8000; (clip)++; } \
  else { *(samples) = sum; }

int synth_1to1_mono(struct StaticData * psd, struct mpstr * gmp, float *bandPtr,unsigned char *samples,int *pnt)
{
  short samples_tmp[64];
  short *tmp1 = samples_tmp;
  int i,ret;
  int pnt1 = 0;

  ret = synth_1to1(psd, gmp, bandPtr,0,(unsigned char *) samples_tmp,&pnt1);
  samples += *pnt;

  for(i=0;i<32;i++) {
    *( (short *) samples) = *tmp1;
    samples += 2;
    tmp1 += 2;
  }
  *pnt += 64;

  return ret;
}


int synth_1to1(struct StaticData * psd, struct mpstr * gmp, float *bandPtr,int channel,unsigned char *out,int *pnt)
{
#if 0//defined(PANDORA) || defined(ODROID)
  return synth_1to1_neon(psd, gmp, bandPtr, channel, out, pnt);
#else
  const int step = 2;

  int bo;

  short *samples = (short *) (out + *pnt);

  float *b0,(*buf)[0x110];
  int clip = 0;
  int bo1;

  bo = gmp->synth_bo;

  if(!channel) {
    bo--;
    bo &= 0xf;
    buf = gmp->synth_buffs[0];
  }
  else {
    samples++;
    buf = gmp->synth_buffs[1];
  }

  if(bo & 0x1) {
    b0 = buf[0];
    bo1 = bo;
    dct64(psd, buf[1]+((bo+1)&0xf),buf[0]+bo,bandPtr);
  }
  else {
    b0 = buf[1];
    bo1 = bo+1;
    dct64(psd, buf[0]+bo,buf[1]+bo+1,bandPtr);
  }

  gmp->synth_bo = bo;

  {
    register int j;
    float *window = psd->decwin + 16 - bo1;

    for (j=16;j;j--,b0+=0x10,window+=0x20,samples+=step)
    {
      float sum;
      sum  = window[0x0] * b0[0x0];
      sum -= window[0x1] * b0[0x1];
      sum += window[0x2] * b0[0x2];
      sum -= window[0x3] * b0[0x3];
      sum += window[0x4] * b0[0x4];
      sum -= window[0x5] * b0[0x5];
      sum += window[0x6] * b0[0x6];
      sum -= window[0x7] * b0[0x7];
      sum += window[0x8] * b0[0x8];
      sum -= window[0x9] * b0[0x9];
      sum += window[0xA] * b0[0xA];
      sum -= window[0xB] * b0[0xB];
      sum += window[0xC] * b0[0xC];
      sum -= window[0xD] * b0[0xD];
      sum += window[0xE] * b0[0xE];
      sum -= window[0xF] * b0[0xF];

      WRITE_SAMPLE(samples,sum,clip);
    }

    {
      float sum;
      sum  = window[0x0] * b0[0x0];
      sum += window[0x2] * b0[0x2];
      sum += window[0x4] * b0[0x4];
      sum += window[0x6] * b0[0x6];
      sum += window[0x8] * b0[0x8];
      sum += window[0xA] * b0[0xA];
      sum += window[0xC] * b0[0xC];
      sum += window[0xE] * b0[0xE];
      WRITE_SAMPLE(samples,sum,clip);
      b0-=0x10,window-=0x20,samples+=step;
    }
    window += bo1<<1;

    for (j=15;j;j--,b0-=0x10,window-=0x20,samples+=step)
    {
      float sum;
      sum = -window[-0x1] * b0[0x0];
      sum -= window[-0x2] * b0[0x1];
      sum -= window[-0x3] * b0[0x2];
      sum -= window[-0x4] * b0[0x3];
      sum -= window[-0x5] * b0[0x4];
      sum -= window[-0x6] * b0[0x5];
      sum -= window[-0x7] * b0[0x6];
      sum -= window[-0x8] * b0[0x7];
      sum -= window[-0x9] * b0[0x8];
      sum -= window[-0xA] * b0[0x9];
      sum -= window[-0xB] * b0[0xA];
      sum -= window[-0xC] * b0[0xB];
      sum -= window[-0xD] * b0[0xC];
      sum -= window[-0xE] * b0[0xD];
      sum -= window[-0xF] * b0[0xE];
      sum -= window[-0x0] * b0[0xF];

      WRITE_SAMPLE(samples,sum,clip);
    }
  }
  *pnt += 128;

  return clip;
#endif
}






int tsynth_1to1(struct StaticData * psd, float *bandPtr,int channel,unsigned char *out,int *pnt)
{
#if defined(PANDORA) || defined(ODROID)
  return tsynth_1to1_neon(psd, bandPtr, channel, out, pnt);
#else
  const int step = 2;

  short *samples = (short *) (out + *pnt);

  float *b0,(*buf)[0x110];
  int clip = 0;
  int bo1;

  if(!channel) {
    psd->bo--;
    psd->bo &= 0xf;
    buf = psd->buffs[0];
  }
  else {
    samples++;
    buf = psd->buffs[1];
  }

  if(psd->bo & 0x1) {
    b0 = buf[0];
    bo1 = psd->bo;
    dct64(psd, buf[1]+((psd->bo+1)&0xf),buf[0]+psd->bo,bandPtr);
  }
  else {
    b0 = buf[1];
    bo1 = psd->bo+1;
    dct64(psd, buf[0]+psd->bo,buf[1]+psd->bo+1,bandPtr);
  }

  {
    register int j;
    float *window = psd->decwin + 16 - bo1;

    for (j=16;j;j--,b0+=0x10,window+=0x20,samples+=step)
    {
      float sum;
      sum  = window[0x0] * b0[0x0];
      sum -= window[0x1] * b0[0x1];
      sum += window[0x2] * b0[0x2];
      sum -= window[0x3] * b0[0x3];
      sum += window[0x4] * b0[0x4];
      sum -= window[0x5] * b0[0x5];
      sum += window[0x6] * b0[0x6];
      sum -= window[0x7] * b0[0x7];
      sum += window[0x8] * b0[0x8];
      sum -= window[0x9] * b0[0x9];
      sum += window[0xA] * b0[0xA];
      sum -= window[0xB] * b0[0xB];
      sum += window[0xC] * b0[0xC];
      sum -= window[0xD] * b0[0xD];
      sum += window[0xE] * b0[0xE];
      sum -= window[0xF] * b0[0xF];

      WRITE_SAMPLE(samples,sum,clip);
    }

    {
      float sum;
      sum  = window[0x0] * b0[0x0];
      sum += window[0x2] * b0[0x2];
      sum += window[0x4] * b0[0x4];
      sum += window[0x6] * b0[0x6];
      sum += window[0x8] * b0[0x8];
      sum += window[0xA] * b0[0xA];
      sum += window[0xC] * b0[0xC];
      sum += window[0xE] * b0[0xE];
      WRITE_SAMPLE(samples,sum,clip);
      b0-=0x10,window-=0x20,samples+=step;
    }
    window += bo1<<1;

    for (j=15;j;j--,b0-=0x10,window-=0x20,samples+=step)
    {
      float sum;
      sum = -window[-0x1] * b0[0x0];
      sum -= window[-0x2] * b0[0x1];
      sum -= window[-0x3] * b0[0x2];
      sum -= window[-0x4] * b0[0x3];
      sum -= window[-0x5] * b0[0x4];
      sum -= window[-0x6] * b0[0x5];
      sum -= window[-0x7] * b0[0x6];
      sum -= window[-0x8] * b0[0x7];
      sum -= window[-0x9] * b0[0x8];
      sum -= window[-0xA] * b0[0x9];
      sum -= window[-0xB] * b0[0xA];
      sum -= window[-0xC] * b0[0xB];
      sum -= window[-0xD] * b0[0xC];
      sum -= window[-0xE] * b0[0xD];
      sum -= window[-0xF] * b0[0xE];
      sum -= window[-0x0] * b0[0xF];

      WRITE_SAMPLE(samples,sum,clip);
    }
  }
  *pnt += 128;

  return clip;
#endif
}



