#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "segy.h"
#include <math.h>

#define VERBOSE 1  // printinfo

int main() {
  int nt = 200, ntrace = 100;
  float dt = 0.002, dx = 10.;
  float* data = (float*)malloc(sizeof(float) * nt);

  char *filename = "output.segy";
  FILE* fout = fopen(filename, "wb");

  char bhead[SEGY_BNYBYTES];
  // length of a trace 240+nt*4
  size_t nsegy = SEGY_HDRBYTES + (nt * 4);

  char* trace = (char*)malloc(nsegy);
  int* thead= (int*)malloc(sizeof(int) * SEGY_NKEYS);
  // init with no value
  memset(bhead, 0, SEGY_BNYBYTES);
  memset(trace, 0, nsegy);
  memset(thead, 0, SEGY_NKEYS * sizeof(int));
  // set binaryheader 400bytes
  set_segyformat(bhead, 1);
  set_segyns(bhead, nt);
  set_segydt(bhead, dt);
  // always to ibm
  set_segyformat(bhead, 1);
  int tracecount = 0;

  // write text header and binary header
  writesegyheader(fout, "testest", 0, 0);
  if (SEGY_BNYBYTES != fwrite(bhead, 1, SEGY_BNYBYTES, fout))
    errorinfo("Error writing binary header");

  // this for coor dinate scalign
  int scalco = 10;
  float calscalco = scalco > 0 ? scalco : 1.0 / (-1 * scalco);

  for (int itrace = 0; itrace < ntrace; itrace++) {
    for (int i = 0; i < nt; i++) {
        /* 基础线性项：随采样点序号递增 | Base linear term: Increases with sample index */
    float linear = (float)i / nt; 
    
    /* 正弦波项：频率与道号关联 | Sinusoidal term: Frequency linked to trace number */
    float sine = sinf(2.0f * M_PI * linear * (itrace + 1)); 
    
    /* 组合信号：振幅随道号变化 | Combined signal: Amplitude scales with trace number */
    data[i] = 0.5f * (itrace + 1) * (linear + sine); 
    }

    // // set trace header 240 bytes
    thead[segykey("tracl")] = itrace + 1;                 //设置道号
    thead[segykey("tracr")] = tracecount;            //设置文件中道序号
    thead[segykey("fldr")] = itrace + 1;                  //设置炮点号
    thead[segykey("ep")] = itrace + 1;                    //设置炮点号
    thead[segykey("scalco")] = scalco;               //设置坐标因子
    thead[segykey("sx")] = itrace * dx * calscalco;  // sx location
    thead[segykey("dt")] = dt < 1 ? 1000000 * dt : 1000 * dt;  // sx location
    thead[segykey("gx")] = itrace * dx * calscalco;    //reciever locaiton
    thead[segykey("cdpx")] = itrace * dx * calscalco;  //reciever locaiton
    thead[segykey("offset")] = 0 * dx * calscalco;     //offset

    // // header and data to segy
    head2segy(trace, thead, SEGY_NKEYS);
    trace2segy(trace + SEGY_HDRBYTES, data, nt, 1);
    if (nsegy != fwrite(trace, 1, nsegy, fout))
      errorinfo("Error writing trace %d", tracecount + 1);
  }

  free(trace);
  free(thead);
  return 0;
}
