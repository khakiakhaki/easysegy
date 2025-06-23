#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "segy.h"

#define VERBOSE 1  // printinfo

int main() {

  char* filename = "output.segy";
  FILE* fo = fopen("testread.bin", "wb");
  FILE* fin = fopen(filename, "rb");
  // after this line , the binaryheader and text header is already read,
  // segy file is on the data start , should direct read data
  segyfile segyin = segyfile_init_read(fin);

  int ebccode = 1;  // if want write text header, set to 1, else set to 0

  if (ebccode) {
    // first kind of read text header 3200 bytes
    // if text header is coded in EBCDIC, set useebc to 1
    // convert to ascii
    ebc2asc(SEGY_EBCBYTES, segyin->textraw);
    // now the segyin ->textraw is ascii text header rather ebcidc code
  }

  //  
  float* data = (float*)malloc(sizeof(float) * segyin->ns);
  int* thead = (int*)malloc(sizeof(int) * SEGY_THNKEYS);
  memset(data, 0, sizeof(float) * segyin->ns);
  memset(thead,0,SEGY_THNKEYS * sizeof(float));

  // get the jobid from binary header by header key
  int jobid = segyin->bhead[segybhkey("jobid")];
  printf("binary header jobid: %d\n", jobid);
  // if want get other binary header key with byte offset , star from 0
  int value = 100;
  char2value(segyin->bhraw, &value, 302, "s");  // set 302
  printf("binary header value at 302: %d\n", value);

  for (size_t itrace = 0; itrace < segyin->ntrace; itrace++) {
    // read one trace data
    segyread_onetrace(segyin, thead, data);

    // read trace header and cal the real value
    if (itrace % 10 == 0) {
      float scale = thead[segykey("scalco")];
      if (scale == 0) {
        scale = 1.0;
      } else if (scale < 0) {
        scale = -1.0 / scale;
      }
      int   shotn = thead[segykey("fldr")];
      float cdpx = thead[segykey("cdpx")] * scale;
      float sx = thead[segykey("sx")] * scale;
      float gx = thead[segykey("gx")] * scale;
      float offset = thead[segykey("offset")] * scale;
      printf("fldr : %d, cdpx: %f, sx: %f, gx: %f, offset: %f, \n", shotn, cdpx, sx, gx, offset);
    }
    fwrite(data, segyin->ns * 4, 1, fo);
  }

  warninginfo("read %d traces to %s", segyin->ntrace, filename);
  free(data);
  free(thead);
  segyfile_free(segyin);
  fclose(fin);
  fclose(fo);
  return 0;
}
