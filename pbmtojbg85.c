/*
 *  pbmtojbg85 - Portable Bitmap to JBIG converter (T.85 version)
 *
 *  Markus Kuhn - http://www.cl.cam.ac.uk/~mgk25/jbigkit/
 *
 *  $Id: pbmtojbg85.c 1293 2008-08-25 22:26:39Z mgk25 $
 */

#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <string.h>
#include "jbig85.h"

#define FRAME_WIDTH 384
#define FRAME_HEGHT 288


/*
 * malloc() with exception handler
 */
void *checkedmalloc(size_t n)
{
  void *p;
  
  if ((p = malloc(n)) == NULL) {
    fprintf(stderr, "Sorry, not enough memory available!\n");
    exit(1);
  }
  
  return p;
}


/*
 * Callback procedure which is used by JBIG encoder to deliver the
 * encoded data. It simply sends the bytes to the output file.
 */
static void data_out(unsigned char *start, size_t len, unsigned char **out)
{
int i;
unsigned char *tmp;
tmp = *out;
for(i=0; i<len; i++) {
	*tmp++ = *start++;
}
*out = tmp;

  //fwrite(start, len, 1, (FILE *) file);
  return;
}

unsigned char* encode_frame(unsigned char *inp,
				 unsigned char *output,
				 unsigned long width,
				 unsigned long height)
{
unsigned char *p, *lines, *next_line;
size_t bpl;
struct jbg85_enc_state s;
unsigned char *prev_line = NULL, *prevprev_line = NULL;
int options = JBG_TPBON;
int mx = -1;
unsigned long x, y;
unsigned long l0 = 0, yi = 0, yr = 0;


 /* allocate buffer for a single image line */
  bpl = (width >> 3) + !!(width & 7);     /* bytes per line */
  //lines = (unsigned char *) checkedmalloc(bpl * 3);
  lines = inp;
  /* initialize parameter struct for JBIG encoder*/
  jbg85_enc_init(&s, width, height, data_out, &output);

  /* Specify a few other options (each is ignored if negative) */
  
  jbg85_enc_options(&s, options, l0, mx);
  for (y = 0; y < height; y++) {

    /* Use a 3-line ring buffer, because the encoder requires that the two
     * previously supplied lines are still in memory when the next line is
     * processed. */
    next_line = lines + y * bpl;

    /* JBIG compress another line and write out result via callback */
    jbg85_enc_lineout(&s, next_line, prev_line, prevprev_line);
    prevprev_line = prev_line;
    prev_line = next_line;

    /* adjust final image height via NEWLEN */
    if (yi && y == yr)
      jbg85_enc_newlen(&s, height);
  }
  return *(s.file);
}

/*
int main (int argc, char **argv)
{
  FILE *fin = stdin, *fout = stdout;
  const char *fnin = NULL, *fnout = NULL;
  int i;
  int all_args = 0, files = 0;
  
  unsigned long width, height;
  //char type;
  unsigned char *inp_cadr, *out_buf, *cur;

  //char *comment = NULL;

	fnin  = argv[1];
	fnout = argv[2];
  
    fin = fopen(fnin, "rb");
    if (!fin) {
      fprintf(stderr, "Can't open input file '%s", fnin);
      perror("'");
      exit(1);
    }

  width = FRAME_WIDTH;
  height = FRAME_HEGHT;

  inp_cadr = (unsigned char *) checkedmalloc(FRAME_WIDTH * FRAME_HEGHT / 8);
  out_buf = (unsigned char *) checkedmalloc(FRAME_WIDTH * FRAME_HEGHT / 8);
  cur = out_buf;

    fout = fopen(fnout, "wb");
    if (!fout) {
      fprintf(stderr, "Can't open input file '%s", fnout);
      perror("'");
      exit(1);
    }
	for(i=0; i<194; i++) {
		fread(inp_cadr, FRAME_WIDTH * FRAME_HEGHT / 8, 1, fin);
		cur = encode_frame(inp_cadr, out_buf, width, height);
		fwrite(out_buf, cur - out_buf, 1, fout);
	}
  free(inp_cadr);
  free(out_buf);
  fclose(fin);
  fclose(fout);
  return 0;
}
*/