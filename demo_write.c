#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "segy.h"

#define VERBOSE 1  // printinfo

int main() {
  int nt = 200, ntrace = 100;
  float dt = 0.002, dx = 10.;
  float* data = (float*)malloc(sizeof(float) * nt);
  int* thead = (int*)malloc(sizeof(int) * SEGY_THNKEYS);

  char* filename = "output.segy";
  FILE* fout = fopen(filename, "wb");
  segyfile segyout = segyfile_init_write(fout, nt, dt, 1, ntrace);

  int needtexthead = 1;  // if want write text header, set to 1, else set to 0

  if (needtexthead) {
    // first kind of write text header 3200 bytes
    // set text header, padding 3200 bytes
    snprintf(segyout->textraw, SEGY_EBCBYTES, "%3199s",
             "SEGY file created by easy_segy demo write\n");
    // write text header, 3200 bytes
    segywrite_texthead(segyout, 0, 0);
  } else {
    // this would write nothing to text header
    segywrite_texthead(segyout, 1, 0);
  }

  // if want set other binary with header key
  segyout->bhead[segybhkey("jobid")] = 10;  // set dt
  // if want set other binary header key with byte offset , star from 0
  int value = 100;
  value2char(segyout->bhraw, &value, 302, "s");  // set 302
  // write binary header to file
  segywrite_binaryhead(segyout);

  int tracecount = 0;

  // this for coordinate scaling 10,100,1000 means x10,x100,x1000
  // -10,-100,-1000 means x0.1 x0.001 x0.0001

  int scalco = 10;
  float calscalco = 1.0;  // coordinate scaling factor
  if (scalco == 0) {
    calscalco = 1.;
  } else if (scalco < 0) {
    calscalco = -1. / scalco;  // convert to positive
  }

  for (int itrace = 0; itrace < ntrace; itrace++) {
    // data generation
    for (int i = 0; i < nt; i++) {
      /* 基础线性项：随采样点序号递增 | Base linear term: Increases with sample index */
      /* 正弦波项：频率与道号关联 | Sinusoidal term: Frequency linked to trace number */
      /* 组合信号：振幅随道号变化 | Combined signal: Amplitude scales with trace number */
      float linear = (float)i / nt;
      float sine = sinf(2.0f * M_PI * linear * (itrace + 1));
      data[i] = 0.5f * (itrace + 1) * (linear + sine);
    }

    // for every trace, set its header
    thead[segykey("tracl")] = itrace + 1;  //设置道号
    thead[segykey("tracr")] = tracecount;  //设置文件中道序号
    thead[segykey("fldr")] = itrace + 1;   //设置炮点号
    thead[segykey("ep")] = itrace + 1;     //设置炮点号
    thead[segykey("scalco")] = scalco == 1 ? 1 : -1 * scalco;  //设置坐标因子
    thead[segykey("sx")] = itrace * dx * calscalco;            // sx location
    thead[segykey("dt")] = dt < 1 ? 1000000 * dt : 1000 * dt;  // sx location
    thead[segykey("gx")] = itrace * dx * calscalco;    //reciever locaiton
    thead[segykey("cdpx")] = itrace * dx * calscalco;  //reciever locaiton
    thead[segykey("offset")] = 0 * dx * calscalco;     //offset

    // // header and data to segy
    segywrite_onetrace(segyout, thead, data);
  }

  warninginfo("write %d traces to %s", ntrace, filename);
  free(data);
  free(thead);
  segyfile_free(segyout);
  fclose(fout);
  return 0;
}
