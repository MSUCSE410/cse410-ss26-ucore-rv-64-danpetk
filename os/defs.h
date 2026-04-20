#ifndef DEFS_H
#define DEFS_H

#include "const.h"
#include "file.h"
#include "kalloc.h"
#include "log.h"
#include "printf.h"
#include "proc.h"
#include "riscv.h"
#include "sbi.h"
#include "string.h"
#include "types.h"
#include "vm.h"

#define BIG_STRIDE 65536

#define DIR 0x040000 // directory
#define FILE 0x100000 // ordinary regular fil

// number of elements in fixed-size array
#define NELEM(x) (sizeof(x) / sizeof((x)[0]))
#define MIN(a, b) (a < b ? a : b)
#define MAX(a, b) (a > b ? a : b)

#define NULL ((void *)0)

#endif // DEF_H
