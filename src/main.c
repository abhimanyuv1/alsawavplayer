/* main.c
 *
 * Copyright 2019 Abhimanyu V
 *
 * Permission is hereby granted, free of charge, to any person obtaining
 * a copy of this software and associated documentation files (the
 * "Software"), to deal in the Software without restriction, including
 * without limitation the rights to use, copy, modify, merge, publish,
 * distribute, sublicense, and/or sell copies of the Software, and to
 * permit persons to whom the Software is furnished to do so, subject to
 * the following conditions:
 *
 * The above copyright notice and this permission notice shall be
 * included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT. IN NO EVENT SHALL THE X CONSORTIUM BE LIABLE FOR ANY
 * CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
 * TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
 * SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 *
 * Except as contained in this notice, the name(s) of the above copyright
 * holders shall not be used in advertising or otherwise to promote the sale,
 * use or other dealings in this Software without prior written
 * authorization.
 */

#include <stdio.h>
#include <stdint.h>
#include <alsa/asoundlib.h>

#define CCCC(c1, c2, c3, c4)  ((c4 << 24) | (c3 << 16) | (c2 << 8) | c1)

typedef enum headerState_e {
  RIFF,
  FMT,
  HDR_DATA,
  DATA
} headerState_t;

typedef struct wavRiff_s {
  uint32_t  chunkID;
  uint32_t  chunkSize;
  uint32_t  format;
} wavRiff_t;

typedef struct wavProperties_s {
  uint32_t  chunkID;
  uint32_t  chunkSize;
  uint16_t  audioFormat;
  uint16_t  numChannels;
  uint32_t  sampleRate;
  uint32_t  byteRate;
  uint16_t  blockAlign;
  uint16_t  bitsPerSample;
} wavProperties_t;

#define DEFAULT_DEVICE  "default"
static snd_pcm_t *playback_handle;
static snd_pcm_uframes_t frames = 512;

int open_audio_device() {
  int err = snd_pcm_open(&playback_handle, DEFAULT_DEVICE, SND_PCM_STREAM_PLAYBACK, 0);
  if (err < 0) {
    printf("Can't open audio %s: %s\n", DEFAULT_DEVICE, snd_strerror(err));
  }
  return err;
}

int configure_audio_device(int bitsPerSample, int channels, int sampleRate) {
  snd_pcm_format_t fmt;
  switch (bitsPerSample) {
		case 8:
			fmt = SND_PCM_FORMAT_U8;
			break;

		case 16:
			fmt = SND_PCM_FORMAT_S16;
			break;

		case 24:
			fmt = SND_PCM_FORMAT_S24;
			break;

		case 32:
			fmt = SND_PCM_FORMAT_S32;
			break;
	}

  int err = snd_pcm_set_params(playback_handle, fmt, SND_PCM_ACCESS_RW_INTERLEAVED,
                               channels, sampleRate, 1, 500000);
  if (err < 0) {
    printf("Failed to configure audio device, err: %s\n", snd_strerror (err));
  }

  return err;
}

void play_audio(int8_t *data, int32_t size) {
	snd_pcm_uframes_t	count;

  if ((count = snd_pcm_writei(playback_handle, data, size)) == -EPIPE) {
    printf("XRUN.\n");
    snd_pcm_prepare(playback_handle);
  } else if (frames < 0) {
    printf("ERROR. Can't write to PCM device. %s\n", snd_strerror(count));
  }
}

int main (int   argc, char *argv[]) {
  FILE *fp = fopen(argv[1], "rb");
  if (!fp) {
    printf ("File %s not found", argv[1]);
    return -1;
  }

  printf("Playing %s \n", argv[1]);

  // Open audio device
  int err = open_audio_device ();
  if (err < 0) {
    printf("Failed to open audio device\n");
    return -1;
  }

  // Parse and play wav file
  headerState_t state = RIFF;
  wavProperties_t wavProperties;

  fseek (fp, 0, SEEK_SET);

  while (1) {
    if (feof (fp)) {
      printf ("End of file\n");
      break;
    }

    switch (state) {
      case RIFF: {
        wavRiff_t wavRiff;
        int n = fread (&wavRiff, 1, sizeof (wavRiff_t), fp);
        if (n == sizeof (wavRiff_t)) {
          if (wavRiff.chunkID == CCCC('R','I','F','F') && wavRiff.format == CCCC('W','A','V','E')) {
            state = FMT;
          }
        }
      }
      break;

      case FMT: {
        int n = fread (&wavProperties, 1, sizeof(wavProperties_t), fp);
        if (n == sizeof (wavProperties_t)) {
          printf("Sample Rate: %u, Channels: %u, bps: %u\n", wavProperties.sampleRate,\
                 wavProperties.numChannels, wavProperties.bitsPerSample);
          // Configure audio device
          configure_audio_device (wavProperties.bitsPerSample, wavProperties.numChannels,
                                  wavProperties.sampleRate);
          state = HDR_DATA;
        }
      }
      break;

      case HDR_DATA: {
        uint32_t chunkID, chunkSize;
        int n = fread (&chunkID,1, sizeof (chunkID), fp);
        if(n ==  sizeof (chunkID)) {
          if (chunkID == CCCC('d','a','t','a')) {
            n = fread (&chunkSize, 1, sizeof (chunkSize), fp);
            if (n == sizeof (chunkSize)) {
              state = DATA;
            }
          }
        }
      }
      break;

      case DATA: {
        int size = frames * wavProperties.numChannels * 2;
        int8_t data[size];
        int n = fread (data, 1, size, fp);
        if (n) {
          printf("Read data: %d\n", n);
          play_audio(data, frames);
        }
      }
      break;
    }
  }

  fclose (fp);

  return 0;
}
