//
// Copyright (c) 2005, 2006 Wei Mingzhi <whistler@openoffice.org>
// All Rights Reserved.
//
// This program is free software; you can redistribute it and/or
// modify it under the terms of the GNU General Public License as
// published by the Free Software Foundation; either version 2 of
// the License, or (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program; if not, write to the Free Software
// Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
// 02110-1301, USA
//

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <malloc.h>
#include "SDL.h"

static int audio_len = 0;
static unsigned char *audio_pos = NULL;
static SDL_AudioSpec audio_spec;
static bool g_fSDLInit = false;
bool g_fAudioOpened = false;

// The audio function callback takes the following parameters:
// stream:  A pointer to the audio buffer to be filled
// len:     The length (in bytes) of the audio buffer
static void SOUND_FillAudio(void *, unsigned char *stream, int len)
{
   // Only play if we have data left
   if (audio_len == 0)
      return;
   // Mix as much data as possible
   len = (len > audio_len) ? audio_len : len;
   SDL_MixAudio(stream, audio_pos, len, SDL_MIX_MAXVOLUME);
   audio_pos += len;
   audio_len -= len;
}

int SOUND_OpenAudio(int freq, int channels, int samples)
{
   if (g_fAudioOpened) {
      return 0;
   }

   // Set the audio format
   audio_spec.freq = freq;
   audio_spec.format = AUDIO_S16;
   audio_spec.channels = channels; // 1 = mono, 2 = stereo
   audio_spec.samples = samples;
   audio_spec.callback = SOUND_FillAudio;
   audio_spec.userdata = NULL;

   // Initialize audio
   if (!SDL_WasInit(SDL_INIT_AUDIO)) {
      if (SDL_Init(SDL_INIT_AUDIO | SDL_INIT_NOPARACHUTE) == -1) { 
         fprintf(stderr, "FATAL ERROR: Could not initialize SDL: %s.\n", SDL_GetError());
         return -1;
      }
      g_fSDLInit = true;
   }

   // Open the audio device, forcing the desired format
   if (SDL_OpenAudio(&audio_spec, NULL) < 0) {
      fprintf(stderr, "WARNING: Couldn't open audio: %s\n", SDL_GetError());
      return -1;
   } else {
      g_fAudioOpened = true;
      return 0;
   }
}

int SOUND_CloseAudio()
{
   if (g_fAudioOpened) {
      SDL_CloseAudio();
      if (g_fSDLInit) {
         SDL_Quit();
         g_fSDLInit = false;
      }
   }
   return 0;
}

void *SOUND_LoadWAV(const char *filename)
{
   SDL_AudioCVT *wavecvt;
   SDL_AudioSpec wavespec, *loaded;
   unsigned char *buf;
   unsigned int len;

   if (!g_fAudioOpened) {
      return NULL;
   }

   wavecvt = (SDL_AudioCVT *)malloc(sizeof(SDL_AudioCVT));
   if (wavecvt == NULL) {
      return NULL;
   }

   loaded = SDL_LoadWAV_RW(SDL_RWFromFile(filename, "rb"), 1, &wavespec, &buf, &len);
   if (loaded == NULL) {
      free(wavecvt);
      return NULL;
   }

   // Build the audio converter and create conversion buffers
   if (SDL_BuildAudioCVT(wavecvt, wavespec.format, wavespec.channels, wavespec.freq,
      audio_spec.format, audio_spec.channels, audio_spec.freq) < 0) {
      SDL_FreeWAV(buf);
      free(wavecvt);
      return NULL;
   }
   int samplesize = ((wavespec.format & 0xFF) / 8) * wavespec.channels;
   wavecvt->len = len & ~(samplesize - 1);
   wavecvt->buf = (unsigned char *)malloc(wavecvt->len * wavecvt->len_mult);
   if (wavecvt->buf == NULL) {
      SDL_FreeWAV(buf);
      free(wavecvt);
      return NULL;
   }
   memcpy(wavecvt->buf, buf, len);
   SDL_FreeWAV(buf);

   // Run the audio converter
   if (SDL_ConvertAudio(wavecvt) < 0) {
      free(wavecvt->buf);
      free(wavecvt);
      return NULL;
   }

   return wavecvt;
}

void SOUND_FreeWAV(void *audio)
{
   if (audio == NULL) {
      return;
   }
   SDL_FreeWAV(((SDL_AudioCVT *)audio)->buf);
   free(audio);
}

void SOUND_PlayWAV(void *audio)
{
   if (audio == NULL) {
      audio_pos = NULL;
      audio_len = -1;
   } else {
      audio_pos = ((SDL_AudioCVT *)audio)->buf;
      audio_len = ((SDL_AudioCVT *)audio)->len * ((SDL_AudioCVT *)audio)->len_mult;
   }
   SDL_PauseAudio(0);
}
