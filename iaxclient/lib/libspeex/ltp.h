/* Copyright (C) 2002 Jean-Marc Valin 
   File: ltp.h
   Long-Term Prediction functions

   Redistribution and use in source and binary forms, with or without
   modification, are permitted provided that the following conditions
   are met:
   
   - Redistributions of source code must retain the above copyright
   notice, this list of conditions and the following disclaimer.
   
   - Redistributions in binary form must reproduce the above copyright
   notice, this list of conditions and the following disclaimer in the
   documentation and/or other materials provided with the distribution.
   
   - Neither the name of the Xiph.org Foundation nor the names of its
   contributors may be used to endorse or promote products derived from
   this software without specific prior written permission.
   
   THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
   ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
   LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
   A PARTICULAR PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE FOUNDATION OR
   CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
   EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
   PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
   PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
   LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
   NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
   SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

#include "speex_bits.h"
#include "misc.h"

typedef struct ltp_params {
   signed char *gain_cdbk;
   int     gain_bits;
   int     pitch_bits;
} ltp_params;


void open_loop_nbest_pitch(spx_sig_t *sw, int start, int end, int len, int *pitch, float *gain, int N, char *stack);


/** Finds the best quantized 3-tap pitch predictor by analysis by synthesis */
int pitch_search_3tap(
spx_sig_t target[],                 /* Target vector */
spx_sig_t *sw,
spx_coef_t ak[],                     /* LPCs for this subframe */
spx_coef_t awk1[],                   /* Weighted LPCs #1 for this subframe */
spx_coef_t awk2[],                   /* Weighted LPCs #2 for this subframe */
spx_sig_t exc[],                    /* Overlapping codebook */
void *par,
int   start,                    /* Smallest pitch value allowed */
int   end,                      /* Largest pitch value allowed */
float pitch_coef,               /* Voicing (pitch) coefficient */
int   p,                        /* Number of LPC coeffs */
int   nsf,                      /* Number of samples in subframe */
SpeexBits *bits,
char *stack,
spx_sig_t *exc2,
spx_sig_t *r,
int   complexity,
int   cdbk_offset
);

/*Unquantize adaptive codebook and update pitch contribution*/
void pitch_unquant_3tap(
spx_sig_t exc[],                    /* Excitation */
int   start,                    /* Smallest pitch value allowed */
int   end,                      /* Largest pitch value allowed */
float pitch_coef,               /* Voicing (pitch) coefficient */
void *par,
int   nsf,                      /* Number of samples in subframe */
int *pitch_val,
float *gain_val,
SpeexBits *bits,
char *stack,
int lost,
int subframe_offset,
float last_pitch_gain,
int cdbk_offset
);

/** Forced pitch delay and gain */
int forced_pitch_quant(
spx_sig_t target[],                 /* Target vector */
spx_sig_t *sw,
spx_coef_t ak[],                     /* LPCs for this subframe */
spx_coef_t awk1[],                   /* Weighted LPCs #1 for this subframe */
spx_coef_t awk2[],                   /* Weighted LPCs #2 for this subframe */
spx_sig_t exc[],                    /* Excitation */
void *par,
int   start,                    /* Smallest pitch value allowed */
int   end,                      /* Largest pitch value allowed */
float pitch_coef,               /* Voicing (pitch) coefficient */
int   p,                        /* Number of LPC coeffs */
int   nsf,                      /* Number of samples in subframe */
SpeexBits *bits,
char *stack,
spx_sig_t *exc2,
spx_sig_t *r,
int complexity,
int cdbk_offset
);

/** Unquantize forced pitch delay and gain */
void forced_pitch_unquant(
spx_sig_t exc[],                    /* Excitation */
int   start,                    /* Smallest pitch value allowed */
int   end,                      /* Largest pitch value allowed */
float pitch_coef,               /* Voicing (pitch) coefficient */
void *par,
int   nsf,                      /* Number of samples in subframe */
int *pitch_val,
float *gain_val,
SpeexBits *bits,
char *stack,
int lost,
int subframe_offset,
float last_pitch_gain,
int cdbk_offset
);
