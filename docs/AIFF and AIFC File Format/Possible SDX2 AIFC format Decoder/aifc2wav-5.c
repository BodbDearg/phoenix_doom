/* public domain
 * ima4, sowt and sdx2 handled
 * sdx2 not hardly tested, may fail, contact me if problems
 * endian-independant code
 * sed@free.fr for comments/suggestions/anything
 * http://sed.free.fr/aifc2wav.html
 * Wed, 22 Jan 2003
 * Last update: Fri, 30 Mar 2007
 *
 * algo, tables ripped from http://www.pcisys.net/~melanson/codecs/adpcm.txt
 * the above link looks dead (2007-03-30), try the following link
 *   http://wiki.multimedia.cx/index.php?title=Category:ADPCM_Audio_Codecs
 *
 * file format in : ftp://ftp.sgi.com/sgi/aiff-c.9.26.91.ps.Z
 *
 * http://www.mactech.com/articles/mactech/Vol.06/06.01/SANENormalized/
 * helped for handling extended numbers
 *
 * http://ewhac.best.vwh.net/3DO-old/music/
 * for sdx2 code (see aif2raw.c there)
 */

/* handled chunks types */
#define FORM	1
#define COMM	2
#define FVER	3
#define SSND	4

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int index_table[16] = {
    -1, -1, -1, -1, 2, 4, 6, 8,
    -1, -1, -1, -1, 2, 4, 6, 8,
};

static int step_table[89] = {
    7, 8, 9, 10, 11, 12, 13, 14, 16, 17,
    19, 21, 23, 25, 28, 31, 34, 37, 41, 45,
    50, 55, 60, 66, 73, 80, 88, 97, 107, 118,
    130, 143, 157, 173, 190, 209, 230, 253, 279, 307,
    337, 371, 408, 449, 494, 544, 598, 658, 724, 796,
    876, 963, 1060, 1166, 1282, 1411, 1552, 1707, 1878, 2066,
    2272, 2499, 2749, 3024, 3327, 3660, 4026, 4428, 4871, 5358,
    5894, 6484, 7132, 7845, 8630, 9493, 10442, 11487, 12635, 13899,
    15289, 16818, 18500, 20350, 22385, 24623, 27086, 29794, 32767
};

/* parse a chunk - waits a 34 bytes buffer, returns a little endian buffer */
void decode_chunk(unsigned char *input, unsigned char *output)
{
  char d;
  int i;
  short predictor;
  short step_index;
  int hnb, lnb;
  int diff;
  int sign, delta;
  short step;

  d = *input++;
  predictor=d << 8;
  d = *input++;
  step_index = d & 127;
  predictor |= d&128;

  /* is this necessary ? */
  if (step_index < 0) step_index = 0;
  if (step_index > 88) step_index = 88;

  step = step_table[step_index];

  for (i=0; i<32; i++, input++) {
    d = *input;
    hnb = (d>>4) & 15;
    lnb = d & 15;

    /* decode lnb */
    step_index += index_table[lnb];
    /* necessary ? */
    if (step_index<0) step_index = 0;
    if (step_index>88) step_index = 88;

    sign = lnb & 8;
    delta = lnb & 7;
    diff = step >> 3;
    if (delta & 4) diff += step;
    if (delta & 2) diff += step >> 1;
    if (delta & 1) diff += step >> 2;
    /* take care of clamping (Adam Sampson <ats@offog.org>) */
    if (sign) {
      int p = ((int) predictor) - diff;
      if (p < -32768)
        predictor = -32768;
      else
        predictor = p;
    } else {
      int p = ((int) predictor) + diff;
      if (p > 32767)
        predictor = 32767;
      else
        predictor = p;
    }

    *output = predictor&255;
    output++;
    *output = (predictor>>8)&255;
    output++;

    step = step_table[step_index];

    /* decode hnb */
    step_index += index_table[hnb];
    if (step_index<0) step_index = 0;
    if (step_index>88) step_index = 88;

    sign = hnb & 8;
    delta = hnb & 7;
    diff = step >> 3;
    if (delta & 4) diff += step;
    if (delta & 2) diff += step >> 1;
    if (delta & 1) diff += step >> 2;
    /* take care of clamping (Adam Sampson <ats@offog.org>) */
    if (sign) {
      int p = ((int) predictor) - diff;
      if (p < -32768)
        predictor = -32768;
      else
        predictor = p;
    } else {
      int p = ((int) predictor) + diff;
      if (p > 32767)
        predictor = 32767;
      else
        predictor = p;
    }

    *output = predictor&255;
    output++;
    *output = (predictor>>8)&255;
    output++;

    step = step_table[step_index];
  }
}

/* reads the FILE, return the type of the chunk, size too */
/* return -1 on error, -2 if unhandled chunk, -3 if EOF */
int get_type(FILE *f, long *size)
{
  int ret;

  unsigned char buf[8];

  ret=fread(buf, 1, 8, f);

  if (ret == 0 && feof(f))
    return -3;

  if (ferror(f) || ret != 8)
    return -1;

  *size = (unsigned)(buf[4]<<24) + (buf[5]<<16) + (buf[6]<<8) + buf[7];

  if (!strncmp((char*)buf, "FORM", 4)) {
    /* let's check if it's AIFC or not */
    if (fread(buf, 1, 4, f) != 4)
      return -1;

    if (strncmp((char*)buf, "AIFC", 4)) {
      buf[4] = 0;
      fprintf(stderr, "unsupported type %s\nonly AIFC handled\n", buf);
      return -1;
    }

    *size -= 4;
    return FORM;
  }

  if (!strncmp((char*)buf, "COMM", 4))
    return COMM;

  if (!strncmp((char*)buf, "FVER", 4))
    return FVER;

  if (!strncmp((char*)buf, "SSND", 4))
    return SSND;

  buf[4] = 0;
  fprintf(stderr, "warning: unhandled chunk %s (size=%ld)\n", buf, *size);

  return -2;
}

/* transform an int into a little endian buffer */
unsigned char *itole(unsigned long l, unsigned char *t)
{
  *t = l & 255;
  *(t+1) = (l >>  8) & 255;
  *(t+2) = (l >> 16) & 255;
  *(t+3) = (l >> 24) & 255;
  return t;
}

/* transform a short into a little endian buffer */
unsigned char *stole(unsigned short l, unsigned char *t)
{
  *t = l & 255;
  *(t+1) = (l >>  8) & 255;
  return t;
}

/* different flags to know what chunk here, what chunk has been proceeded */
#define FORM_DONE	1
#define COMM_DONE	2
#define FVER_DONE	4
#define SSND_DONE	8

/* we don't want stack space for those */
static unsigned char buf[24];
static unsigned char in[34];
static unsigned char out[256];  /* only 128 if mono, 256 if stereo */
static unsigned char out2[128]; /* for stereo, another buffer (we must
				 * interleave the two buffers in the WAV file)
				 */

/* != file types supported */
#define IMA4	1
#define SOWT	2
#define SDX2	3

int main(int n, char **v)
{
  long size;
  int flags = 0;
  char *input_file;
  char *output_file;
  FILE *fin, *fout;
  int nb_chans;		/* number of channels, must be 1 or 2 */
  int nb_frames;	/* number of sound chunks */
  int nb_bits;		/* nb bits / sample, must be 16 */
  long double freq;	/* sample rate */
  long ssnd_pos;	/* pos of beginning of the stream in the input file */
  long total_size=1;	/* some files are bigger than what says FORM size
			 * so we must only parse the nb of bytes given by
			 * FORM size (init to 1 for the while to run)
			 */
  int sound_type=0;	/* the type of the data in the sound file */
  long riff_size;	/* the riff size - depends of the type of data sound */
  short *last;		/* for SDX2 files */

  if (n != 3) {
    fprintf(stderr, "usage: %s <input AIFC file> <output WAV file>\n", v[0]);
    return 1;
  }

  input_file = v[1];
  output_file = v[2];

  fin = fopen(input_file, "rb");
  if (!fin) {
    perror(input_file);
    return 1;
  }

  fout = fopen(output_file, "wb");
  if (!fout) {
    perror(output_file);
    fclose(fin);
    return 1;
  }

  /* we don't care about the order (only FORM must be first) */
  /* we require only one SSND, one COMM, at most one FVER, that's all */
  /* total_size must be != 0 before to come here */
  while(total_size)
  switch (get_type(fin, &size)) {
    case FORM :
      if (flags) goto bad_file;

      flags = FORM_DONE;
      total_size = size;

      break;

    case COMM :
      if (!(flags & FORM_DONE)) goto bad_file;

      if (flags & COMM_DONE) goto bad_file;

      total_size -= 8;
      if (total_size < 0) goto in_error_size;

      flags |= COMM_DONE;

      if (fread(buf, 1, 22, fin) != 22)
	goto bad_file;
      total_size -= 22;
      if (total_size < 0) goto in_error_size;

      nb_chans = (buf[0]<<8) + buf[1];
      nb_frames = (unsigned)(buf[2]<<24) + (buf[3]<<16) + (buf[4]<<8) + buf[5];
      nb_bits = (buf[6]<<8) + buf[7];

      /* converting this silly extended format into freq
       * IAFC people are weird, why extended ?
       * this code is far from efficient
       * based on :
       * http://www.mactech.com/articles/mactech/Vol.06/06.01/SANENormalized/
       * this may fail on your machine, it works on ia32 and sparc
       */
      {
	int sign = (buf[8] >> 7) & 1;
	int exp = ((buf[8] & 127) << 8) + buf[9] - 16383;
	int j = (buf[10]>>7)&1;
	unsigned long frac = ((buf[10]&127) << 24) +
			      (buf[11] << 16) +
			      (buf[12] << 8) +
			      (buf[13]);
	unsigned long frac2 = (unsigned)(buf[14] << 24) +
			      (buf[15] << 16) +
			      (buf[16] << 8) +
			      (buf[17]);
	int i;

	freq = frac2;
	for (i=0; i<32; i++) freq /= 2;
	freq += frac;
	for (i=0; i<31; i++) freq /= 2;
	if (j) freq += 1.;
	while (exp) {
	  freq *= 2;
	  exp--;
	}

	if (sign) freq = -freq;
      }

      if (nb_chans != 1 && nb_chans != 2) {
	fprintf(stderr, "nb of channels = %d\nonly 1 or 2 channels handled\n",
		nb_chans);
	goto bad_file;
      }

      if (nb_bits != 16) {
	fprintf(stderr, "bits per sample = %d\nonly 16 bits handled\n",
		nb_bits);
	goto bad_file;
      }

      /* 1e7 is enough ? */
      if (freq <= 0 || freq >=1e7) {
	fprintf(stderr, "freq = %Lf\nonly 0 < freq < 1e7 handled\n", freq);
	goto bad_file;
      }

      /* let's test if it's a ima4, sowt or SDX2 or something else */
      memcpy(buf, &buf[18], 4);
      buf[4] = 0;
      if (strcmp((char*)buf, "ima4") &&
          strcmp((char*)buf, "sowt") &&
          strcmp((char*)buf, "SDX2")) {
	fprintf(stderr,
          "compression scheme = %s\nonly ima4, sowt and SDX2 handled\n", buf);
	goto bad_file;
      }
      sound_type = IMA4;
      if (!strcmp((char*)buf, "sowt"))
	sound_type = SOWT;
      else if (!strcmp((char*)buf, "SDX2"))
        sound_type = SDX2;

      /* should we optimize here ? no */
      /* the doc says there may be a non-counted byte */
      if (size & 1) size++;
      size -= 22;
      total_size -= size;
      if (total_size < 0) goto in_error_size;

      while(size > 0) {
	int c = fgetc(fin);
	if (c == EOF) {
	  fprintf(stderr, "error with COMM chunk, bad size\n");
	  goto bad_file;
	}
	size--;
      }

      break;

    case FVER : {
      unsigned long version;

      if (!(flags & FORM_DONE)) goto bad_file;

      if (flags & FVER_DONE) goto bad_file;

      flags |= FVER_DONE;

      total_size -= 8;
      if (total_size < 0) goto in_error_size;

      if (fread(buf, 1, 4, fin) != 4)
	goto bad_file;
      total_size -= 4;
      if (total_size < 0) goto in_error_size;

      version = (unsigned)(buf[0]<<24) + (buf[1]<<16) + (buf[2]<<8) + buf[3];

      if (version != 2726318400U) {
	fprintf(stderr, "version = %lu\nonly 2726318400 handled\n", version);
	goto bad_file;
      }

      /* size should be 4, what to do ? if we don't read, there might
       * be a problem, but someone might not set it to 4 and still
       * telling us there is nothing more... well, let's read it
       */
      /* the doc says there may be a non-counted byte */
      if (size & 1) size++;
      size -= 4;
      total_size -= size;
      if (total_size < 0) goto in_error_size;

      while(size > 0) {
	int c = fgetc(fin);
	if (c == EOF) {
	  fprintf(stderr, "error with FVER chunk, bad size\n");
	  goto bad_file;
	}
	size--;
      }

      break;
    }

    case SSND : {
      unsigned long offset, blocksize;
      unsigned long nbbytes=0;

      if (!(flags & FORM_DONE)) goto bad_file;

      if (flags & SSND_DONE) {
	fprintf(stderr, "more than one SSND\n");
	goto bad_file;
      }

      total_size -= 8;
      if (total_size < 0) goto in_error_size;

      flags |= SSND_DONE;

      if (fread(buf, 1, 8, fin) != 8)
	goto bad_file;
      total_size -= 8;
      if (total_size < 0) goto in_error_size;

      offset = (unsigned)(buf[0]<<24) + (buf[1]<<16) + (buf[2]<<8) + buf[3];
      blocksize = (unsigned)(buf[4]<<24) + (buf[5]<<16) + (buf[6]<<8) + buf[7];

#if 0
      if (offset || blocksize) {
	fprintf(stderr, "offset = %lu, blocksize = %lu\nthey both must be 0\n",
		offset, blocksize);
        goto bad_file;
      }
#endif

      ssnd_pos = ftell(fin) + offset;
      if (ssnd_pos == -1) {
	perror("ftell");
	goto bad_file;
      }

      /* the doc says there may be a non-counted byte */
      if (size & 1) size++;

      /* we should optimize here, no ? */
      size -= 8;
      total_size -= size;
      if (total_size < 0) goto in_error_size;

      while(size > 0) {
	int c = fgetc(fin);
#if 0
	if (c == EOF) {
	  fprintf(stderr, "error with SSND chunk, bad size\n");
	  goto bad_file;
	}
#endif
	if (c == EOF) {
	  if (!(flags & COMM_DONE)) goto bad_file;

	  /* a little hack to update nb_frames for sowt only */
	  fprintf(stderr, "warning - bad size in SSND, the output file will"
			  " probably be wrong\n");

	  nbbytes -= offset;
          if (sound_type == SDX2) {
            fprintf(stderr, "error - for SDX2 files, I don't know what to"
                            " do\nContact sed@free.fr to see what"
                            " we can do.\n");
            goto both_error;
          } else if (sound_type == SOWT)
	    nbbytes /= 2;
	  else
	    nbbytes /= 34;
	  if (nb_chans == 2) nbbytes /= 2;
	  nb_frames = nbbytes;

	  /* we go to all_parsed, because it's EOF */
	  goto all_parsed;
	}

        nbbytes++;
	size--;
      }

      break;
    }

    /* -1 is error */
    case -1:
	goto bad_file;

    /* -2 is unhandled chunk type, so pass it */
    case -2: {
      if (!(flags & FORM_DONE)) goto bad_file;

      total_size -= 8;
      if (total_size < 0) goto in_error_size;

      /* the doc says there may be a non-counted byte */
      if (size & 1) size++;
      total_size -= size;
      if (total_size < 0) goto in_error_size;

      while (size) {
	int c = fgetc(fin);
	if (c == EOF)
	  goto bad_file;
	size --;
      }
      break;
    }

    /* -3 is EOF */
    case -3 :
      /* this may be an error, let's just produce a warning */
      fprintf(stderr, "warning: your file is shorter than expected\n");
      goto all_parsed;

    default : 
      fprintf(stderr, "you should not read this...\n");
      goto bad_file;
  }

all_parsed:
  /* we must have parsed COMM and SSND */
  if ((flags & (COMM_DONE | SSND_DONE)) != (COMM_DONE | SSND_DONE)) {

    if (!(flags & COMM_DONE))
      fprintf(stderr, "no COMM chunk found\n");

    if (!(flags & SSND_DONE))
      fprintf(stderr, "no SSND chunk found\n");

    goto bad_file;
  }

  /* is freq an integer ? */
  if (freq != (long double)((int)freq))
    fprintf(stderr, "warning, freq = %Lf will be truncated to %d\n",
	freq, (int)freq);

  /* all is alright */

  /* write header of wav */
  /* start with RIFF */
  if (fwrite("RIFF", 1, 4, fout) != 4) goto out_error;

  if (sound_type == IMA4)
    riff_size = nb_frames * 128;
  else
    riff_size = nb_frames * 2;
  if (nb_chans == 2)
    riff_size *= 2;

  /* then size of the stuff */
  if (fwrite(itole(riff_size+36, buf), 1, 4, fout) != 4) goto out_error;

  /* then WAVEfmt */
  if (fwrite("WAVEfmt ", 1, 8, fout) != 8) goto out_error;

  /* length of fmt chunk (16) */
  if (fwrite(itole(16, buf), 4, 1, fout) != 1) goto out_error;

  /* 1 => PCM */
  if (fwrite(stole(1, buf), 2, 1, fout) != 1) goto out_error;

  /* nb of channels */
  if (fwrite(stole(nb_chans, buf), 2, 1, fout) != 1) goto out_error;

  /* freq */
  if (fwrite(itole((int)freq, buf), 4, 1, fout) != 1) goto out_error;

  /* don't know what */
  if (fwrite(itole((int)freq * nb_chans * (nb_bits/8), buf), 4, 1, fout) != 1)
    goto out_error;

  /* don't know what */
  if (fwrite(stole(nb_chans * (nb_bits/8), buf), 2, 1, fout) != 1)
    goto out_error;

  /* nb bits */
  if (fwrite(stole(nb_bits, buf), 2, 1, fout) != 1) goto out_error;

  /* data chunk */
  if (fwrite("data", 4, 1, fout) != 1) goto out_error;

  /* data size */
  if (fwrite(itole(riff_size, buf), 1, 4, fout) != 4) goto out_error;

  /* let's kick all the frames out */
  if (fseek(fin, ssnd_pos, SEEK_SET) == -1) goto out_error;

  if (sound_type != SOWT) goto not_sowt;

  /* sowt */
  while (nb_frames) {
    nb_frames--;

    if (fread(in, 1, 2, fin) != 2) {
      perror(input_file);
      goto bad_file;
    }
    if (fwrite(in, 1, 2, fout) != 2)
      goto out_error;

    if (nb_chans == 1)
      continue;

    if (fread(in, 1, 2, fin) != 2) {
      perror(input_file);
      goto bad_file;
    }
    if (fwrite(in, 1, 2, fout) != 2)
      goto out_error;
  }

  goto file_done;

not_sowt:
  /* SDX2? */
  if (sound_type != SDX2) goto not_sdx2;

  last = malloc(nb_chans * sizeof(short));
  if (!last) {
    perror("memory");
    goto both_error;
  }
  memset(last, 0, sizeof(short) * nb_chans);

  while (nb_frames > 0) {
    signed char src;
    unsigned short dst;
    int i;

    nb_frames--;

    for (i=0; i<nb_chans; i++) {
      if (fread(&src, 1, 1, fin) != 1) {
        perror(input_file);
        free(last);
        goto bad_file;
      }

      if (src & 1)
        dst = last[i] + ((src * abs(src)) << 1);
      else
        dst = (src * abs(src)) << 1;

      last[i] = dst;

      /* out is 256 bytes, so we're fine */
      out[0] = dst&255;
      out[1] = (dst>>8)&255;

      if (fwrite(out, 1, 2, fout) != 2) {
        free(last);
        goto out_error;
      }
    }
  }

  free(last);

  goto file_done;

not_sdx2:
  /* ima4 */
  while (nb_frames) {
    if (fread(in, 1, 34, fin) != 34) {
      perror(input_file);
      goto bad_file;
    }

    decode_chunk(in, out);

    /* if stereo mode the next chunk is right channel */
    if (nb_chans == 2) {
      int i;

      if (fread(in, 1, 34, fin) != 34) {
        perror(input_file);
        goto bad_file;
      }

      decode_chunk(in, out2);

      /* we must now interleave the two channels */

      /* first part, move the first channel to its correct position */
      /* we should optimize this code */
      for (i=63; i>0; i--) {
	out[i*4] = out[i*2];
	out[i*4+1] = out[i*2+1];
      }

      /* second, add right channel to output buffer */
      /* we should optimize this code */
      for (i=0; i<64; i++) {
	out[i*4+2] = out2[i*2];
	out[i*4+3] = out2[i*2+1];
      }
    }

    if (fwrite(out, 1, nb_chans == 2 ? 256 : 128, fout) !=
		(nb_chans == 2 ? 256 : 128))
      goto out_error;

    nb_frames--;
  }

file_done:
  fclose(fin);
  fclose(fout);
  return 0;

in_error_size:
  fprintf(stderr, "the size information of your file is bad\n");

bad_file:
  fprintf(stderr, "%s: bad sound file, can't load\n", input_file);
  fprintf(stderr, "contact sed@free.fr to add support for your file\n");

both_error:
  fclose(fin);
  fclose(fout);
  remove(output_file);
  return 1;

out_error:
  perror(output_file);
  goto both_error;
}
