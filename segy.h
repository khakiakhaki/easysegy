/* This file is automatically generated. DO NOT EDIT! */
#include <stdio.h>
#ifndef _segy_h
#define _segy_h

#define SEGY_BH_FORMAT 24
#define SEGY_BH_NS 20
#define SEGY_BH_DT 16

enum {
  SEGY_EBCBYTES = 3200, /* Bytes in the card image EBCDIC block */
  SEGY_BHNBYTES = 400,  /* Bytes in the binary coded block	*/
  SEGY_THNBYTES = 240,  /* Bytes in the tape trace header	*/
  SEGY_THNKEYS = 91,    /* Number of mandated header fields	*/
  SEGY_BHNKEYS = 27,    /* Number of mandated binary fields	*/
};

/** format,ns,dt,nsegy,ntrace,textraw,bhraw,bhead,tracebuf*/
typedef struct {
  FILE* fp;
  int format;
  int ns;
  float dt;
  size_t nsegy;    // bytes of one trace with header
  size_t ntrace;   // number of traces
  char* textraw;   // text header raw
  char* bhraw;     // binray raw (same as segy) avoid to handle the bhraw
  int* bhead;      // binary header
  char* tracebuf;  // a buffer
} SEGY_FILE;

typedef SEGY_FILE* segyfile;

/*<eroor info  */
void errorinfo(const char* format, ...);
/*< warning info */
void warninginfo(const char* format, ...) ;

/*< initialize segyfile in read mode  >*/
segyfile segyfile_init_read(FILE* fp);

/*< initialize segyfile in read mode  >*/
segyfile segyfile_init_write(FILE* fp, int ns, float dt, int format,size_t ntrace);

/*< free the segyfile */
void segyfile_free(segyfile segyf);

/*< Convert char array arrr[narr]: EBC to ASCII >*/
void ebc2asc(int narr, char* arr);

/*< Convert char array arrr[narr]: ASCII to EBC >*/
void asc2ebc(int narr, char* arr);

/*< extracts SEGY format from binary header >*/
int segyformat(const char* bhead);

/*< set SEGY format in binary header >*/
void set_segyformat(char* bhead, int format);

/*< extracts ns (number of samples) from binary header >*/
int segyns(const char* bhead);

/*< set ns (number of samples) in binary header >*/
void set_segyns(char* bhead, int ns);

/*< extracts dt (sampling) from binary header >*/
float segydt(const char* bhead);

/*< set dt (sampling) in binary header >*/
void set_segydt(char* bhead, float dt);

/*< Extract a SEGY key value >*/
int segykey(const char* key);

/*< Extract a SEGY binary header index by key name */
int segybhkey(const char* key);

/*< Find a SEGY key from its number >*/
const char* segybhkeyword(int k);

/*< Find a SEGY key from its number >*/
const char* segykeyword(int k);

/*< Extract a floating-point trace[nt] from buffer buf.
-- format: 1: IBM, 2: int4, 3: int2, 5: IEEE, 7: int1
>*/
void segy2trace(const char* buf, float* trace, int ns, int format);

/*< Convert a floating-point trace[ns] to buffer buf.
-- format: 1: IBM, 2: int4, 3: int2, 5: IEEE
>*/
void trace2segy(char* buf, const float* trace, int ns, int format);

/*< Convert an integer trace[nk] to buffer buf >*/
void head2segy(char* theadchar, const int* thead, int nk);

/*< convert raw segy traceheader to native int array */
void segy2head(char* theadchar, int* thead, int nk);

/*< Create a binary header for SEGY >*/
void bhead2segy(char* bheadchar, const int* bhead, int nk);

/** convert raw trace segy to native int array */
void segy2bhead(char* bheadchar, int* bhead, int nk);

/*< writhe 3200 header*/
int segywrite_texthead(segyfile segyf, int isskip, int useebc);

/*< read the segy text header to file 3200 bytes */
int segyread_texthead(segyfile segyf, int isskip, int useebc);

/*< write the bheader segy */
int segywrite_binaryhead(segyfile segyf);

/*< read the bheader segy */
int segyread_binaryhead(segyfile segyf);

/*< set the segy bytes of trace with header */
size_t segycal_nsegy(segyfile segyf);

/*< get trace number */
size_t segycal_ntrace(segyfile segyf);

/*< read one trace from segy */
int segyread_onetrace(segyfile segyf, int* thead, float* trace);

/*< write one trace from segy */
int segywrite_onetrace(segyfile segyf, const int* thead, const float* trace);

/*< convert char to value */
void char2value(const char* chars, void* value, size_t off, const char* type);

/*< convert value to char */
void value2char(char* chars, void* value, size_t off, const char* type);

#endif
