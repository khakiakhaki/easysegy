/* Operations with SEGY standard files */
/*
  Copyright (C) 2004 University of Texas at Austin
  Copyright (C) 2025 China University of Mining and Technology-Beijing
  
  This program is free software; you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation; either version 2 of the License, or
  (at your option) any later version.
  
  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.
  
  You should have received a copy of the GNU General Public License
  along with this program; if not, write to the Free Software
  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
*/

#include <math.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef HAVE_SYS_STAT_H
#include <sys/stat.h>
#include <sys/types.h>
#endif  //HAVE_SYS_STAT_H

#include "segy.h"
#ifndef _segy_h

enum {
  SEGY_EBCBYTES = 3200, /* Bytes in the card image EBCDIC block */
  SEGY_BNYBYTES = 400,  /* Bytes in the binary coded block	*/
  SEGY_HDRBYTES = 240,  /* Bytes in the tape trace header	*/
  SEGY_NKEYS = 91,      /* Number of mandated header fields	*/
  SEGY_BHKEYS = 27,     /* Number of mandated binary fields	*/
  // SEGY_MAXKEYS=356,      /* Maximum number of keys               */
};

/** segyfile for read and write segy 
*/
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

#endif

#define IEEEMAX 0x7FFFFFFF
#define IEMAXIB 0x611FFFFF
#define IEMINIB 0x21200000

#if defined(__BYTE_ORDER__) && __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__
#define HOST_LITTLE_ENDIAN 1
#else
#define HOST_LITTLE_ENDIAN 0
#endif

// Check for compiler intrinsics for byte swapping
#if defined(__GNUC__) || defined(__clang__)
#define bswap16(x) __builtin_bswap16(x)
#define bswap32(x) __builtin_bswap32(x)
#define bswap64(x) __builtin_bswap64(x)
#elif defined(_MSC_VER)
#include <stdlib.h>
#define bswap16(x) _byteswap_ushort(x)
#define bswap32(x) _byteswap_ulong(x)
#define bswap64(x) _byteswap_uint64(x)
#else
// Fallback for other compilers
static inline uint16_t bswap16(uint16_t x) {
  return (x >> 8) | (x << 8);
}
static inline uint32_t bswap32(uint32_t x) {
  return (x >> 24) | ((x << 8) & 0x00FF0000) | ((x >> 8) & 0x0000FF00) |
         (x << 24);
}
static inline uint64_t bswap64(uint64_t x) {
  x = (x & 0x00000000FFFFFFFF) << 32 | (x & 0xFFFFFFFF00000000) >> 32;
  x = (x & 0x0000FFFF0000FFFF) << 16 | (x & 0xFFFF0000FFFF0000) >> 16;
  x = (x & 0x00FF00FF00FF00FF) << 8 | (x & 0xFF00FF00FF00FF00) >> 8;
  return x;
}
#endif

typedef unsigned char byte;

static byte EBCtoASC[256] = {
    0x00, 0x01, 0x02, 0x03, 0xCF, 0x09, 0xD3, 0x7F, 0xD4, 0xD5, 0xC3, 0x0B,
    0x0C, 0x0D, 0x0E, 0x0F, 0x10, 0x11, 0x12, 0x13, 0xC7, 0xB4, 0x08, 0xC9,
    0x18, 0x19, 0xCC, 0xCD, 0x83, 0x1D, 0xD2, 0x1F, 0x81, 0x82, 0x1C, 0x84,
    0x86, 0x0A, 0x17, 0x1B, 0x89, 0x91, 0x92, 0x95, 0xA2, 0x05, 0x06, 0x07,
    0xE0, 0xEE, 0x16, 0xE5, 0xD0, 0x1E, 0xEA, 0x04, 0x8A, 0xF6, 0xC6, 0xC2,
    0x14, 0x15, 0xC1, 0x1A, 0x20, 0xA6, 0xE1, 0x80, 0xEB, 0x90, 0x9F, 0xE2,
    0xAB, 0x8B, 0x9B, 0x2E, 0x3C, 0x28, 0x2B, 0x7C, 0x26, 0xA9, 0xAA, 0x9C,
    0xDB, 0xA5, 0x99, 0xE3, 0xA8, 0x9E, 0x21, 0x24, 0x2A, 0x29, 0x3B, 0x5E,
    0x2D, 0x2F, 0xDF, 0xDC, 0x9A, 0xDD, 0xDE, 0x98, 0x9D, 0xAC, 0xBA, 0x2C,
    0x25, 0x5F, 0x3E, 0x3F, 0xD7, 0x88, 0x94, 0xB0, 0xB1, 0xB2, 0xFC, 0xD6,
    0xFB, 0x60, 0x3A, 0x23, 0x40, 0x27, 0x3D, 0x22, 0xF8, 0x61, 0x62, 0x63,
    0x64, 0x65, 0x66, 0x67, 0x68, 0x69, 0x96, 0xA4, 0xF3, 0xAF, 0xAE, 0xC5,
    0x8C, 0x6A, 0x6B, 0x6C, 0x6D, 0x6E, 0x6F, 0x70, 0x71, 0x72, 0x97, 0x87,
    0xCE, 0x93, 0xF1, 0xFE, 0xC8, 0x7E, 0x73, 0x74, 0x75, 0x76, 0x77, 0x78,
    0x79, 0x7A, 0xEF, 0xC0, 0xDA, 0x5B, 0xF2, 0xF9, 0xB5, 0xB6, 0xFD, 0xB7,
    0xB8, 0xB9, 0xE6, 0xBB, 0xBC, 0xBD, 0x8D, 0xD9, 0xBF, 0x5D, 0xD8, 0xC4,
    0x7B, 0x41, 0x42, 0x43, 0x44, 0x45, 0x46, 0x47, 0x48, 0x49, 0xCB, 0xCA,
    0xBE, 0xE8, 0xEC, 0xED, 0x7D, 0x4A, 0x4B, 0x4C, 0x4D, 0x4E, 0x4F, 0x50,
    0x51, 0x52, 0xA1, 0xAD, 0xF5, 0xF4, 0xA3, 0x8F, 0x5C, 0xE7, 0x53, 0x54,
    0x55, 0x56, 0x57, 0x58, 0x59, 0x5A, 0xA0, 0x85, 0x8E, 0xE9, 0xE4, 0xD1,
    0x30, 0x31, 0x32, 0x33, 0x34, 0x35, 0x36, 0x37, 0x38, 0x39, 0xB3, 0xF7,
    0xF0, 0xFA, 0xA7, 0xFF};

static byte ASCtoEBC[256] = {
    0x00, 0x01, 0x02, 0x03, 0x37, 0x2D, 0x2E, 0x2F, 0x16, 0x05, 0x15, 0x0B,
    0x0C, 0x0D, 0x0E, 0x0F, 0x10, 0x11, 0x12, 0x13, 0x3C, 0x15, 0x32, 0x26,
    0x18, 0x19, 0x3F, 0x27, 0x1C, 0x1D, 0x1E, 0x1F, 0x40, 0x5A, 0x7F, 0x7B,
    0x5B, 0x6C, 0x50, 0x7D, 0x4D, 0x5D, 0x5C, 0x4E, 0x6B, 0x60, 0x4B, 0x61,
    0xF0, 0xF1, 0xF2, 0xF3, 0xF4, 0xF5, 0xF6, 0xF7, 0xF8, 0xF9, 0x7A, 0x5E,
    0x4C, 0x7E, 0x6E, 0x6F, 0x7C, 0xC1, 0xC2, 0xC3, 0xC4, 0xC5, 0xC6, 0xC7,
    0xC8, 0xC9, 0xD1, 0xD2, 0xD3, 0xD4, 0xD5, 0xD6, 0xD7, 0xD8, 0xD9, 0xE2,
    0xE3, 0xE4, 0xE5, 0xE6, 0xE7, 0xE8, 0xE9, 0xAD, 0xE0, 0xBD, 0x5F, 0x6D,
    0x79, 0x81, 0x82, 0x83, 0x84, 0x85, 0x86, 0x87, 0x88, 0x89, 0x91, 0x92,
    0x93, 0x94, 0x95, 0x96, 0x97, 0x98, 0x99, 0xA2, 0xA3, 0xA4, 0xA5, 0xA6,
    0xA7, 0xA8, 0xA9, 0xC0, 0x4F, 0xD0, 0xA1, 0x07, 0x3F, 0x3F, 0x3F, 0x3F,
    0x3F, 0x3F, 0x3F, 0x3F, 0x3F, 0x3F, 0x3F, 0x3F, 0x3F, 0x3F, 0x3F, 0x3F,
    0x3F, 0x3F, 0x3F, 0x3F, 0x3F, 0x3F, 0x3F, 0x3F, 0x3F, 0x3F, 0x3F, 0x3F,
    0x3F, 0x3F, 0x3F, 0x3F, 0x3F, 0x3F, 0x3F, 0x3F, 0x3F, 0x3F, 0x3F, 0x3F,
    0x3F, 0x3F, 0x3F, 0x3F, 0x3F, 0x3F, 0x3F, 0x3F, 0x3F, 0x3F, 0x3F, 0x3F,
    0x3F, 0x3F, 0x3F, 0x3F, 0x3F, 0x3F, 0x3F, 0x3F, 0x3F, 0x3F, 0x3F, 0x3F,
    0x3F, 0x3F, 0x3F, 0x3F, 0x3F, 0x3F, 0x3F, 0x3F, 0x3F, 0x3F, 0x3F, 0x3F,
    0x3F, 0x3F, 0x3F, 0x3F, 0x3F, 0x3F, 0x3F, 0x3F, 0x3F, 0x3F, 0x3F, 0x3F,
    0x3F, 0x3F, 0x3F, 0x3F, 0x3F, 0x3F, 0x3F, 0x3F, 0x3F, 0x3F, 0x3F, 0x3F,
    0x3F, 0x3F, 0x3F, 0x3F, 0x3F, 0x3F, 0x3F, 0x3F, 0x3F, 0x3F, 0x3F, 0x3F,
    0x3F, 0x3F, 0x3F, 0x3F, 0x3F, 0x3F, 0x3F, 0x3F, 0x3F, 0x3F, 0x3F, 0x3F,
    0x3F, 0x3F, 0x3F, 0xFF};

typedef struct Segy {
  const char* name;
  unsigned int size;
} segy;

static const segy bheadkey[] = {
    {"jobid", 4},  /* job identification number 4 */
    {"lino", 4},   /* line number (only one line per reel) 8 */
    {"reno", 4},   /* reel number 12 */
    {"ntrpr", 2},  /* number of data traces per record 14 */
    {"nart", 2},   /* number of auxiliary traces per record 16 */
    {"hdt", 2},    /* sample interval in micro secs for this reel 18 */
    {"dto", 2},    /* same for original field recording 20 */
    {"hns", 2},    /* number of samples per trace for this reel 22 */
    {"nso", 2},    /* same for original field recording 24 */
    {"format", 2}, /* data sample format code:
         1 = floating point, 4 byte (32 bits)
         2 = fixed point, 4 byte (32 bits)
         3 = fixed point, 2 byte (16 bits)
         4 = fixed point w/gain code, 4 byte (32 bits)
         5 = IEEE floating point, 4 byte (32 bits)
         8 = two's complement integer, 1 byte (8 bits)
         26 */
    {"fold", 2},   /* CDP fold expected per CDP ensemble 28 */
    {"tsort", 2},  /* trace sorting code: 
         1 = as recorded (no sorting)
         2 = CDP ensemble
         3 = single fold continuous profile
         4 = horizontally stacked 30 */
    {"vscode", 2}, /* vertical sum code:
         1 = no sum
         2 = two sum ...
         N = N sum (N = 32,767) 32 */
    {"hsfs", 2},   /* sweep frequency at start 34 */
    {"hsfe", 2},   /* sweep frequency at end 36 */
    {"hslen", 2},  /* sweep length (ms) 38 */
    {"hstyp", 2},  /* sweep type code:
        1 = linear
        2 = parabolic
        3 = exponential
        4 = other 40 */
    {"schn", 2},   /* trace number of sweep channel 42 */
    {"hstas", 2},  /* sweep trace taper length at start if tapered (the
         taper starts at zero time and is effective for
         this length) 44 */
    {"hstae", 2},  /* sweep trace taper length at end (the ending taper
         starts at sweep length minus the taper length at
         end) 46 */
    {"htatyp", 2}, /* sweep trace taper type code:
         1 = linear
         2 = cos-squared
         3 = other 48 */
    {"hcorr", 2},  /* correlated data traces code:
         1 = no
         2 = yes 50 */
    {"bgrcv", 2},  /* binary gain recovered code:
         1 = yes
         2 = no 52 */
    {"rcvm", 2},   /* amplitude recovery method code:
        1 = none
        2 = spherical divergence
        3 = AGC
        4 = other 54 */
    {"mfeet", 2},  /* measurement system code:
        1 = meters
        2 = feet 56 */
    {"polyt", 2},  /* impulse signal polarity code:
        1 = increase in pressure or upward
        geophone case movement gives
        negative number on tape
        2 = increase in pressure or upward
        geophone case movement gives
        positive number on tape 58 */
    {"vpol", 2},   /* vibratory polarity code:
        code  seismic signal lags pilot by
        1 -> 337.5 to  22.5 degrees
        2 ->  22.5 to  67.5 degrees
        3 ->  67.5 to 112.5 degrees
        4 -> 112.5 to 157.5 degrees
        5 -> 157.5 to 202.5 degrees
        6 -> 202.5 to 247.5 degrees
        7 -> 247.5 to 292.5 degrees
        8 -> 293.5 to 337.5 degrees 60 */
};

static const segy standard_segy_key[] = {
    {"tracl", 4},  /* trace sequence number within line 0 */
    {"tracr", 4},  /* trace sequence number within reel 4 */
    {"fldr", 4},   /* field record number 8 */
    {"tracf", 4},  /* trace number within field record 12 */
    {"ep", 4},     /* energy source point number 16 */
    {"cdp", 4},    /* CDP ensemble number 20 */
    {"cdpt", 4},   /* trace number within CDP ensemble 24 */
    {"trid", 2},   /* trace identification code:
                        1 = seismic data
                        2 = dead
                        3 = dummy
                        4 = time break
                        5 = uphole
                        6 = sweep
                        7 = timing
                        8 = water break
                        9---, N = optional use (N = 32,767) 28 */
    {"nvs", 2},    /* number of vertically summed traces (see
        vscode in bhed structure) 30 */
    {"nhs", 2},    /* number of horizontally summed traces (see
        vscode in bhed structure) 32 */
    {"duse", 2},   /* data use:
        1 = production
        2 = test 34 */
    {"offset", 4}, /* distance from source point to receiver
        group (negative if opposite to direction
        in which the line was shot) 36 */
    {"gelev", 4},  /* receiver group elevation from sea level
        (above sea level is positive) 40 */
    {"selev", 4},  /* source elevation from sea level
         (above sea level is positive) 44 */
    {"sdepth", 4}, /* source depth (positive) 48 */
    {"gdel", 4},   /* datum elevation at receiver group 52 */
    {"sdel", 4},   /* datum elevation at source 56 */
    {"swdep", 4},  /* water depth at source 60 */
    {"gwdep", 4},  /* water depth at receiver group 64 */
    {"scalel", 2}, /* scale factor for previous 7 entries
         with value plus or minus 10 to the
         power 0, 1, 2, 3, or 4 (if positive,
         multiply, if negative divide) 68 */
    {"scalco", 2}, /* scale factor for next 4 entries
         with value plus or minus 10 to the
         power 0, 1, 2, 3, or 4 (if positive,
         multiply, if negative divide) 70 */
    {"sx", 4},     /* X source coordinate 72 */
    {"sy", 4},     /* Y source coordinate 76 */
    {"gx", 4},     /* X group coordinate 80 */
    {"gy", 4},     /* Y source coordinate 84 */
    {"counit", 2}, /* coordinate units code:
         for previoius four entries
         1 = length (meters or feet)
         2 = seconds of arc (in this case, the
         X values are unsigned intitude and the Y values
         are latitude, a positive value designates
         the number of seconds east of Greenwich
         or north of the equator 88 */
    {"wevel", 2},  /* weathering velocity 90 */
    {"swevel", 2}, /* subweathering velocity 92 */
    {"sut", 2},    /* uphole time at source 94 */
    {"gut", 2},    /* uphole time at receiver group 96 */
    {"sstat", 2},  /* source static correction 98 */
    {"gstat", 2},  /* group static correction 100 */
    {"tstat", 2},  /* total static applied 102 */
    {"laga", 2},   /* lag time A, time in ms between end of 240-
          byte trace identification header and time
          break, positive if time break occurs after
          end of header, time break is defined as
          the initiation pulse which maybe recorded
          on an auxiliary trace or as otherwise
          specified by the recording system 104 */
    {"lagb", 2},   /* lag time B, time in ms between the time
          break and the initiation time of the energy source,
          may be positive or negative 106 */
    {"delrt", 2},  /* delay recording time, time in ms between
          initiation time of energy source and time
          when recording of data samples begins
          (for deep water work if recording does not
          start at zero time) 108 */
    {"muts", 2},   /* mute time--start 110 */
    {"mute", 2},   /* mute time--end 112 */
    {"ns", 2},     /* number of samples in this trace 114 */
    {"dt", 2},     /* sample interval, in micro-seconds 116 */
    {"gain", 2},   /* gain type of field instruments code:
          1 = fixed
          2 = binary
          3 = floating point
          4 ---- N = optional use 118 */
    {"igc", 2},    /* instrument gain constant 120 */
    {"igi", 2},    /* instrument early or initial gain 122 */
    {"corr", 2},   /* correlated:
          1 = no
          2 = yes 124 */
    {"sfs", 2},    /* sweep frequency at start 126 */
    {"sfe", 2},    /* sweep frequency at end 128 */
    {"slen", 2},   /* sweep length in ms 130 */
    {"styp", 2},   /* sweep type code:
          1 = linear
          2 = cos-squared
          3 = other 132 */
    {"stas", 2},   /* sweep trace length at start in ms 134 */
    {"stae", 2},   /* sweep trace length at end in ms 136 */
    {"tatyp", 2},  /* taper type: 1=linear, 2=cos^2, 3=other 138 */
    {"afilf", 2},  /* alias filter frequency if used 140 */
    {"afils", 2},  /* alias filter slope 142 */
    {"nofilf", 2}, /* notch filter frequency if used 144 */
    {"nofils", 2}, /* notch filter slope 146 */
    {"lcf", 2},    /* low cut frequency if used 148 */
    {"hcf", 2},    /* high cut frequncy if used 150 */
    {"lcs", 2},    /* low cut slope 152 */
    {"hcs", 2},    /* high cut slope 154 */
    {"year", 2},   /* year data recorded 156 */
    {"day", 2},    /* day of year 158 */
    {"hour", 2},   /* hour of day (24 hour clock) 160 */
    {"minute", 2}, /* minute of hour 162 */
    {"sec", 2},    /* second of minute 164 */
    {"timbas", 2}, /* time basis code:
          1 = local
          2 = GMT
          3 = other 166 */
    {"trwf", 2},   /* trace weighting factor, defined as 1/2^N
          volts for the least sigificant bit 168 */
    {"grnors", 2}, /* geophone group number of roll switch
          position one 170 */
    {"grnofr", 2}, /* geophone group number of trace one within
          original field record 172 */
    {"grnlof", 2}, /* geophone group number of last trace within
          original field record 174 */
    {"gaps", 2},   /* gap size (total number of groups dropped) 176 */
    {"otrav", 2},  /* overtravel taper code:
          1 = down (or behind)
          2 = up (or ahead) 71/178 */
    {"cdpx", 4},   /* X coordinate of CDP 180 */
    {"cdpy", 4},   /* Y coordinate of CDP 184 */
    {"iline", 4},  /* in-line number 188 */
    {"xline", 4},  /* cross-line number 192 */
    {"shnum", 4},  /* shotpoint number 196 */
    {"shsca", 2},  /* shotpoint scalar 200 */
    {"trunit", 2}, /* trace value measurement. 202 */
    {"tdcm4", 4},  /* transduction const 204 */
    {"tdcm2", 2},  /* transduction const 208 */
    {"tdunit", 2}, /* transduction units 210 */
    {"triden", 2}, /* device/trace identifier 212 */
    {"stype", 2},  /* time scalar 214 */
    {"sto", 2},    /* source type/Orientation 216 */
    {"sedxl", 4},  /* source energy direction. 218 */
    {"sedil", 2},  /* unknown 222 */
    {"smm", 4},    /* source measurement 224 */
    {"sm", 2},     /* source measurement unit 228 */
    {"smu", 2},    /* source measurement unit 230 */
    {"unass1", 4}, /* unassigned 232 */
    {"unass2", 4}  /* unassigned 236 */
};

/* Big-endian to Little-endian conversion and back */
static inline uint16_t get16(const char* buf);
static inline uint32_t get32(const char* buf);
static inline uint64_t get64(const char* buf);
static inline float get32f(const char* buf);
static inline double get64f(const char* buf);

static inline void put16(char* buf, uint16_t val);
static inline void put32(char* buf, uint32_t val);
static inline void put64(char* buf, uint64_t val);
// static inline void put32f(char* buf,float val);
// static inline void put64f(char* buf,double val);

/* IBM to IEEE float conversion and back */
static float ibm2ieee(const char* num);
static void ieee2ibm(char* num, float y);

/* alloc segyfiel */
static void segyinit_alloc(segyfile segyf);

/* init a segey for read
* @return SEGY_FILE 
with binary header and text header already read
could direct read data 
*/
segyfile segyfile_init_read(FILE* fp) {
  segyfile segyf = (segyfile)malloc(sizeof(SEGY_FILE));
  segyinit_alloc(segyf);
  segyf->fp = fp;

  segyread_texthead(segyf, 0, 0);
  segyread_binaryhead(segyf);
  segyf->format = segyformat(segyf->bhraw);
  segyf->ns = segyns(segyf->bhraw);
  segyf->dt = segydt(segyf->bhraw);
  segyf->nsegy = segycal_nsegy(segyf);
  segyf->ntrace = segycal_ntrace(segyf);
  segyf->tracebuf = (char*)malloc(segyf->nsegy);
  if (!segyf->tracebuf)
    errorinfo("malloc failed for tracebuf");
  memset(segyf->tracebuf, 0, segyf->nsegy);
  return segyf;
}

/* segyfile write init , no write set, should write manual for more flexible write */
segyfile segyfile_init_write(FILE* fp, int ns, float dt, int format,
                             size_t ntrace) {
  segyfile segyf = (segyfile)malloc(sizeof(SEGY_FILE));
  segyinit_alloc(segyf);
  segyf->fp = fp;

  segyf->format = format;
  segyf->ns = ns;
  segyf->dt = dt;
  segyf->bhead[segybhkey("hns")] = ns;
  segyf->bhead[segybhkey("hdt")] = (int)(dt > 1 ? dt * 1000. : dt * 1000000.);
  segyf->bhead[segybhkey("format")] = format;
  segyf->ntrace = ntrace;
  segyf->nsegy = segycal_nsegy(segyf);
  segyf->tracebuf = (char*)malloc(segyf->nsegy);
  if (!segyf->tracebuf)
    errorinfo("malloc failed for tracebuf");
  memset(segyf->tracebuf, 0, segyf->nsegy);
  return segyf;
}

static void segyinit_alloc(segyfile segyf) {
  if (!segyf)
    errorinfo("malloc failed for SEGY_FILE");
  segyf->textraw = (char*)malloc(SEGY_EBCBYTES);
  if (!segyf->textraw)
    errorinfo("malloc failed for segy textraw");
  memset(segyf->textraw, 0, SEGY_EBCBYTES);
  segyf->bhraw = (char*)malloc(SEGY_BHNBYTES);
  if (!segyf->bhraw)
    errorinfo("malloc failed for segy bhraw");
  memset(segyf->bhraw, 0, SEGY_BHNBYTES);
  segyf->bhead = (int*)malloc(sizeof(int) * SEGY_BHNKEYS);
  if (!segyf->bhead)
    errorinfo("malloc failed for segy bhead");
  memset(segyf->bhead, 0, sizeof(int) * SEGY_BHNKEYS);
}

/*< free the segyfile */
void segyfile_free(segyfile segyf) {
  if (segyf) {
    free(segyf->tracebuf);
    free(segyf->textraw);
    free(segyf->bhraw);
    free(segyf->bhead);
    free(segyf);
  }
}

// Get a 2-byte integer from a big-endian buffer
static inline uint16_t get16(const char* buf) {
  uint16_t x;
  memcpy(&x, buf, 2);
#ifdef HOST_LITTLE_ENDIAN
  x = bswap16(x);
#endif
  return x;
}

// Get a 4-byte integer from a big-endian buffer
static inline uint32_t get32(const char* buf) {
  uint32_t x;
  memcpy(&x, buf, 4);
#ifdef HOST_LITTLE_ENDIAN
  x = bswap32(x);
#endif
  return x;
}

// Get a 8-byte integer from a big-endian buffer
static inline uint64_t get64(const char* buf) {
  uint64_t x;
  memcpy(&x, buf, 8);
#ifdef HOST_LITTLE_ENDIAN
  x = bswap64(x);
#endif
  return x;
}

// Get a 4-byte float from a big-endian buffer
static inline float get32f(const char* buf) {
  union {
    uint32_t u;
    float f;
  } x;
  memcpy(&x.u, buf, 4);
#ifdef HOST_LITTLE_ENDIAN
  x.u = bswap32(x.u);
#endif
  return x.f;
}

// Get a 8-byte float from a big-endian buffer
static inline double get64f(const char* buf) {
  union {
    uint64_t u;
    double d;
  } x;
  memcpy(&x.u, buf, 8);
#ifdef HOST_LITTLE_ENDIAN
  x.u = bswap64(x.u);
#endif
  return x.d;
}

// Put a 2-byte integer into a big-endian buffer
static inline void put16(char* buf, uint16_t val) {
#ifdef HOST_LITTLE_ENDIAN
  val = bswap16(val);
#endif
  memcpy(buf, &val, 2);
}

// Put a 4-byte integer/float into a big-endian buffer
static inline void put32(char* buf, uint32_t val) {
#ifdef HOST_LITTLE_ENDIAN
  val = bswap32(val);
#endif
  memcpy(buf, &val, 4);
}

// put a 8-byte integerinto a big-endian buffer
static inline void put64(char* buf, uint64_t val) {
#ifdef HOST_LITTLE_ENDIAN
  val = bswap64(val);
#endif
  memcpy(buf, &val, 8);
}

// Get a 8-byte integer from a big-endian buffer
static inline void put32f(char* buf, float val) {
  union {
    uint32_t u;
    float f;
  } x;
  x.f = val;
#ifdef HOST_LITTLE_ENDIAN
  x.u = bswap32(x.u);
#endif
  memcpy(buf, &x.u, 4);
}

static inline void put64f(char* buf, double val) {
  union {
    uint64_t u;
    double d;
  } x;
  x.d = val;
#ifdef HOST_LITTLE_ENDIAN
  x.u = bswap64(x.u);
#endif
  memcpy(buf, &x.u, 8);
}

void ebc2asc(int narr, char* arr)
/*< Convert char array arrr[narr]: EBC to ASCII >*/
{
  int i;
  unsigned char j;

  for (i = 0; i < narr; i++) {
    j = (unsigned char)arr[i];
    arr[i] = (char)EBCtoASC[j];
  }
}

void asc2ebc(int narr, char* arr)
/*< Convert char array arrr[narr]: ASCII to EBC >*/
{
  int i;
  unsigned char j;

  for (i = 0; i < narr; i++) {
    j = (unsigned char)arr[i];
    arr[i] = (char)ASCtoEBC[j];
  }
}

int segyformat(const char* bhead) {
  return (int)get16(bhead + SEGY_BH_FORMAT);
}

void set_segyformat(char* bhead, int format) {
  put16(bhead + SEGY_BH_FORMAT, (uint16_t)format);
}

int segyns(const char* bhead) {
  return (int)get16(bhead + SEGY_BH_NS);
}

void set_segyns(char* bhead, int ns) {
  put16(bhead + SEGY_BH_NS, (uint16_t)ns);
}

float segydt(const char* bhead) {
  float dt_micro = (float)(get16(bhead + SEGY_BH_DT));
  return dt_micro / 1000000.0f;
}

void set_segydt(char* bhead, float dt) {
  int16_t dt_micro = (int)(dt * 1000000.0f + 0.5f);
  put16(bhead + SEGY_BH_DT, (int16_t)dt_micro);
}

static void ieee2ibm(char* num, float y)
/* floating-point conversion to IBM format */
{
  unsigned int x, s, f;
  int e;
  const unsigned int fMAXIBM = 0x7FFFFFFF;

  memcpy(&x, &y, 4);

  /* check for special case of zero */
  if ((x & 0x7fffffff) == 0) {
    put32(num, x);
    return;
  }

  /* fetch the sign, exponent (removing excess 127), and fraction */
  s = x & 0x80000000;
  e = ((x & 0x7f800000) >> 23) - 127;
  f = x & 0x007fffff;

  /* convert 23 bit fraction to 24 bit fraction */
  f <<= 1;

  /* restore the '1' preceeded the IEEE binary point */
  f |= 0x01000000;

  /* convert scale factor from base-2 to base-16 */
  if (e >= 0) {
    f <<= (e & 3);
    e >>= 2;
  } else {
    f >>= ((-e) & 3);
    e = -((-e) >> 2);
  }

  /* reduce fraction to 24 bits */
  if (f & 0x0f000000) {
    f >>= 4;
    e += 1;
  }

  /* convert exponent to excess 64 and store the number */
  if ((e += 64) > 127) {
    s |= fMAXIBM;
  } else if (e >= 0) {
    s |= (e << 24) | f;
  }

  put32(num, s);
}

static float ibm2ieee(const char* num)
/* floating point conversion from IBM format */
{
  unsigned int x, s, f;
  const unsigned int fMAXIEEE = 0x7F7FFFFF;
  int e;
  float y;

  x = get32(num);

  /* check for special case of zero */
  if ((x & 0x7fffffff) == 0)
    return 0.0;

  /* fetch the sign, exponent (removing excess 64), and fraction */
  s = x & 0x80000000;
  e = ((x & 0x7f000000) >> 24) - 64;
  f = x & 0x00ffffff;

  /* convert scale factor from base-16 to base-2 */
  if (e >= 0) {
    e <<= 2;
  } else {
    e = -((-e) << 2);
  }

  /* convert exponent for 24 bit fraction to 23 bit fraction */
  e -= 1;

  /* normalize the fraction */
  if (0 != f) {
    while ((f & 0x00800000) == 0) {
      f <<= 1;
      e -= 1;
    }
  }

  /* drop the '1' preceeding the binary point */
  f &= 0x007fffff;

  /* convert exponent to excess 127 and store the number */
  if ((e += 127) >= 255) {
    s |= fMAXIEEE;
  } else if (e > 0) {
    s |= (e << 23) | f;
  }

  memcpy(&y, &s, 4);
  return y;
}

/*< Extract a SEGY key index by key name */
int segykey(const char* key) {

  int i;
  for (i = 0; i < SEGY_THNKEYS; i++) {
    if (0 == strcmp(key, standard_segy_key[i].name))
      return i;
  }
  errorinfo("no such key %s", key);
  return 0;
}

/*< Extract a SEGY binary header index by key name */
int segybhkey(const char* key) {
  for (int i = 0; i < SEGY_BHNKEYS; i++) {
    if (0 == strcmp(key, bheadkey[i].name))
      return i;
  }
  errorinfo("no such binary header key %s", key);
  return 0;
}

/*< Find a SEGY key from its number >*/
const char* segybhkeyword(int k) {
  return bheadkey[k].name;
}

/*< Find a SEGY key from its number >*/
const char* segykeyword(int k) {
  return standard_segy_key[k].name;
}

/*< Extract a floating-point trace[nt] from traced raw.
-- format: 1: IBM, 2: int4, 3: int2, 5: IEEE
>*/
void segy2trace(const char* buf, float* trace, int ns, int format) {
  int i, nb;

  nb = (3 == format) ? 2 : 4;

  for (i = 0; i < ns; i++, buf += nb) {
    switch (format) {
      case 1:
        trace[i] = ibm2ieee(buf);
        break; /* IBM float */
      case 2:
        trace[i] = (float)get32(buf);
        break; /* int4 */
      case 3:
        trace[i] = (float)get16(buf);
        break; /* int2 */
      case 5:
        trace[i] = get32f(buf);
        break; /* IEEE float */
      default:
        errorinfo("not support format %d", format);
        break;
    }
  }
}

/*< Convert a floating-point trace[ns] to buffer buf.
-- format: 1: IBM, 2: int4, 3: int2, 5: IEEE
*/
void trace2segy(char* tracebuf, const float* trace, int ns, int format) {
  int i, nb;

  nb = (3 == format) ? 2 : 4;

  for (i = 0; i < ns; i++, tracebuf += nb) {
    switch (format) {
      case 1:
        ieee2ibm(tracebuf, trace[i]);
        break; /* IBM float */
      case 2:
        put32(tracebuf, (int)trace[i]);
        break; /* int4 */
      case 3:
        put16(tracebuf, (int)trace[i]);
        break; /* int2 */
      case 5:
        put32f(tracebuf, (float)trace[i]);
        break; /* IEEE float */
      default:
        errorinfo("Unknown format %d", format);
        break;
    }
  }
}

/*< Convert an integer trace[nk] to buffer buf */
void head2segy(char* tracebuf, const int* thead, int nk) {
  char* buf = tracebuf;
  if (nk > SEGY_THNKEYS)
    nk = SEGY_THNKEYS;
  for (int i = 0; i < nk; i++) {
    if (thead[i] == 0) {
      buf += standard_segy_key[i].size;
      continue;
    }
    if (2 == standard_segy_key[i].size) {
      put16(buf, thead[i]);
      buf += 2;
    } else {
      put32(buf, thead[i]);
      buf += 4;
    }
  }
}

/** convert raw segy traceheader to native int array
* @param theadchar: raw segy buffer, must be at least SEGY_THNBYTES bytes
* @param thead: integer array to store trace header, must be at least SEGY
*/
void segy2head(char* tracebuf, int* thead, int nk) {
  char* p = tracebuf;
  if (nk > SEGY_THNKEYS)
    nk = SEGY_THNKEYS;
  for (int i = 0; i < nk; i++) {
    if (i < SEGY_THNKEYS && 2 == standard_segy_key[i].size) {
      thead[i] = (short)get16(p);
      p += 2;
    } else {
      thead[i] = get32(p);
      p += 4;
    }
  }
}

/* write the first nk keys to binary header */
void bhead2segy(char* bheadchar, const int* bhead, int nk) {
  char* buf = bheadchar;
  if (nk > SEGY_BHNKEYS)
    nk = SEGY_BHNKEYS;
  for (int i = 0; i < nk; i++) {
    if (i < SEGY_BHNKEYS && 2 == bheadkey[i].size) {
      put16(buf, (int)bhead[i]);
      buf += 2;
    } else {
      put32(buf, bhead[i]);
      buf += 4;
    }
  }
}

/** convert raw segy to native int array
* @param bheadchar: raw segy buffer, must be at least SEGY_THNBYTES bytes
* @param bhead: integer array to store trace header, must be at least SEGY
*/
void segy2bhead(char* bheadchar, int* bhead, int nk) {
  char* buf = bheadchar;
  if (nk > SEGY_BHNKEYS)
    nk = SEGY_BHNKEYS;
  for (int i = 0; i < SEGY_BHNKEYS; i++) {
    if (i < SEGY_BHNKEYS && 2 == bheadkey[i].size) {
      bhead[i] = (short)get16(buf);
      buf += 2;
    } else {
      bhead[i] = get32(buf);
      buf += 4;
    }
  }
}

/*< print error info and exit program */
void errorinfo(const char* format, ...) {
  va_list args;

  (void)fflush(stdout);
  va_start(args, format);

  /* print out name of program causing error */
  fprintf(stderr, "%s: ", "ERROR");

  /* print out remainder of message */
  (void)vfprintf(stderr, format, args);
  va_end(args);

  /* if format ends with ':', print system information */
  fprintf(stderr, "\n");
  (void)fflush(stderr);
  exit(EXIT_FAILURE);
}

/*< print warning info and exit program */
void warninginfo(const char* format, ...) {
  va_list args;

  (void)fflush(stdout);
  va_start(args, format);

  /* print out name of program causing error */
  fprintf(stderr, "%s: ", "WARNING");

  /* print out remainder of message */
  (void)vfprintf(stderr, format, args);
  va_end(args);

  /* if format ends with ':', print system information */
  fprintf(stderr, "\n");
  (void)fflush(stderr);
}

/*
wirte the segy text header to file , all 3200 bytes
@pram file: file to write header to
@param content: content to write, if NULL, writes empty header
@param isskip: if true, skips writing header,write nothing
@param isebccode: if true, write with EBCDIC code 
*/

int segywrite_texthead(segyfile segyf, int isskip, int useebc) {
  if (isskip) {
    fseek(segyf->fp, SEGY_EBCBYTES, SEEK_SET);
    return 3200;
  }
  char ahead[SEGY_EBCBYTES];
  memcpy(ahead, segyf->textraw, SEGY_EBCBYTES);

  if (useebc) {
    asc2ebc(SEGY_EBCBYTES, ahead);
  }

  return fwrite(ahead, 1, SEGY_EBCBYTES, segyf->fp);
}

/*
wirte the segy text header to file , all 3200 bytes
@pram file: file to write header to
@param content: content to write, if NULL, writes empty header
@param isskip: if true, skips writing header,write nothing
@param isebccode: if true, decode from EBCDIC to ASCII 
*/
int segyread_texthead(segyfile segyf, int isskip, int useebc) {
  if (isskip) {
    fseek(segyf->fp, SEGY_EBCBYTES, SEEK_SET);
    return 3200;
  }

  if (SEGY_EBCBYTES != fread(segyf->textraw, 1, SEGY_EBCBYTES, segyf->fp))
    errorinfo("Error reading ebcdic header");

  if (useebc) {
    ebc2asc(SEGY_EBCBYTES, segyf->textraw);
  }

  return SEGY_EBCBYTES;
}

/*< write the binary header segy 
@param fp: file to write header to
@param bhead : binary header to write,  int *
*/
int segywrite_binaryhead(segyfile segyf) {
  bhead2segy(segyf->bhraw, segyf->bhead, SEGY_BHNKEYS);
  if (segydt(segyf->bhraw) == 0. || segyf->bhead[segybhkey("hdt")] == 0)
    warninginfo("binary header dt not set");
  if (segyns(segyf->bhraw) == 0 || segyf->bhead[segybhkey("hns")] == 0)
    warninginfo("binary header ns not set");
  if (segyformat(segyf->bhraw) == 0 || segyf->bhead[segybhkey("format")] == 0)
    warninginfo("binary header format not set");
  return fwrite(segyf->bhraw, 1, SEGY_BHNBYTES, segyf->fp);
}

int segyread_binaryhead(segyfile segyf) {
  if (SEGY_BHNBYTES != fread(segyf->bhraw, 1, SEGY_BHNBYTES, segyf->fp))
    errorinfo("Error reading binary header");
  segy2bhead(segyf->bhraw, segyf->bhead, SEGY_BHNKEYS);
  return SEGY_BHNBYTES;
}

/*< the bytes of one trace with header
240 + ns * sizeof(databyte)*/
size_t segycal_nsegy(segyfile segyf) {
  return (size_t)(SEGY_THNBYTES + segyf->ns * ((segyf->format == 3) ? 2 : 4));
}

/*< calculate trace number */
size_t segycal_ntrace(segyfile segyf) {
  size_t original_pos = ftello(segyf->fp);
  fseeko(segyf->fp, 0, SEEK_END);
  size_t pos = ftello(segyf->fp); /* pos is the filesize in bytes */
  fseeko(segyf->fp, original_pos, SEEK_SET);
  return (pos - SEGY_EBCBYTES - SEGY_BHNBYTES) / segyf->nsegy;
}

/** read one trace from segy 
* @param SEGY_FILE: segyfile struct
* @param thead: integer array to store trace header, must be at least SEGY_THNKEYS
* @param trace: float array to store trace data, must be at least ns elements
*/
int segyread_onetrace(segyfile segyf, int* thead, float* trace) {
  if (1 != fread(segyf->tracebuf, segyf->nsegy, 1, segyf->fp))
    return 0; /* End of file or error */
  segy2head(segyf->tracebuf, thead, SEGY_THNKEYS);
  segy2trace(segyf->tracebuf + SEGY_THNBYTES, trace, segyf->ns, segyf->format);
  return 1;
}

/** write one trace from segy 
* @param SEGY_FILE: segyfile struct
* @param thead: integer array to store trace header, must be at least SEGY_THNKEYS
* @param trace: float array to store trace data, must be at least ns elements
*/
int segywrite_onetrace(segyfile segyf, const int* thead, const float* trace) {
  head2segy(segyf->tracebuf, thead, SEGY_THNKEYS);
  trace2segy(segyf->tracebuf + SEGY_THNBYTES, trace, segyf->ns, segyf->format);
  if (1 != fwrite(segyf->tracebuf, segyf->nsegy, 1, segyf->fp))
    errorinfo("Error writing trace");
  return 1;
}

/** convert char to value
* @param chars: character array to convert from
* @param value: pointer to store the converted value
* @param off: offset in chars to start reading from (start from 0)
* @param type: type of value to convert to, /(i)nt/(l)ong/(f)loat/(d)ouble/(s)hort

example: write cdpx,sx from segy header 
int gx,sx;
value2char(segy->tracebuf, &cdpx, 180,"i"); // cdpx is at offset 180 in trace head (240bytes) 
value2char(segy->tracebuf, &sx, 72,"i"); // gx is at offset 180 in tracebuf (240bytes)
*/
void char2value(const char* chars, void* value, size_t off, const char* type) {
  switch (type[0]) {
    case 'i': /* int */
      *(int*)value = get32(chars + off);
      break;
    case 'l': /* long int */
      *(long*)value = get64f(chars + off);
      break;
    case 'f': /* float */
      *(float*)value = get32f(chars + off);
      break;
    case 'd': /* double */
      *(double*)value = get64f(chars + off);
      break;
    case 's': /* short */
      *(short*)value = (short)get16(chars + off);
      break;
    case 'c': /* char/byte/int8 */
      *(char*)value = *(chars + off);
      break;
    default:
      errorinfo("Unknown type %c", type);
  }
}

/** convert value to char
* @param chars: character array to convert from
* @param value: pointer to store the converted value
* @param off: offset in chars to start reading from (start from 0)
* @param type: type of value to convert to, /(i)nt/(l)ong/(f)loat/(d)ouble/(s)hort

example :
// write cdpx,sx from segy header 
int gx,sx;
value2char(segy->tracebuf, &cdpx, 180,"i"); // cdpx is at offset 180 in trace head (240bytes) 
value2char(segy->tracebuf, &sx, 72,"i"); // gx is at offset 180 in tracebuf (240bytes)
*/
void value2char(char* chars, void* value, size_t off, const char* type) {
  switch (type[0]) {
    case 'i': /* int */
      put32(chars + off, *(uint32_t*)value);
      break;
    case 'l': /* long int */
      put64(chars + off, *(uint64_t*)value);
      break;
    case 'f': /* float */
      put32f(chars + off, *(float*)value);
      break;
    case 'd': /* double */
      put64f(chars + off, *(double*)value);
      break;
    case 's': /* short */
      put16(chars + off, *(uint16_t*)value);
      break;
    case 'c': /* char/byte/int8 */
      *(chars + off) = *(char*)value;
      break;
    default:
      errorinfo("Unknown type %c", type[0]);
  }
}
