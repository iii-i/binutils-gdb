/* This test program is part of GDB, the GNU debugger.

   Copyright (C) 2022 Free Software Foundation, Inc.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 3 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program.  If not, see <http://www.gnu.org/licenses/>.  */

/* Benchmark for registering and unregistering JITed code.
   Must be compiled with -DSHLIB=<total number of shared objects>.  */

#include "../gdb.base/jit-elf-util.h"
#include "../gdb.base/jit-protocol-util.h"
#include <stdint.h>
#include <sys/time.h>

static struct jit_code_entry entries[SHLIB];
static uint64_t register_times[SHLIB];
static uint64_t unregister_times[SHLIB];

static uint64_t
time_delta (struct timeval *start_time)
{
  struct timeval end_time;

  gettimeofday (&end_time, NULL);
  return (uint64_t)(end_time.tv_sec - start_time->tv_sec) * 1000000
	 + (end_time.tv_usec - start_time->tv_usec);
}

void __attribute__ ((noinline)) done_breakpoint (void) {}

int
main (void)
{
  struct timeval start_time;
  int i;

  /* Load and register shared libraries.  */
  for (i = 0; i < SHLIB; i++)
    {
      int (*jited_func) (void);
      size_t obj_size;
      char name[32];
      void *addr;

      sprintf (name, "jit-lib%d.so", i);
      addr = load_elf (name, &obj_size, NULL);
      sprintf (name, "jited_func_%d", i);
      jited_func = addr + (uintptr_t)load_symbol (addr, name);

      entries[i].symfile_addr = addr;
      entries[i].symfile_size = obj_size;
      gettimeofday (&start_time, NULL);
      jit_push_back (&entries[i]);
      register_times[i] = time_delta (&start_time);

      if (jited_func () != i)
	{
	  fprintf (stderr, "jited_func_%d () != %d\n", i, i);
	  return 1;
	}
    }

  /* Now unregister them all in reverse order.  */
  for (i = SHLIB - 1; i >= 0; i--)
    {
      gettimeofday (&start_time, NULL);
      jit_pop_back ();
      unregister_times[i] = time_delta (&start_time);
    }

  done_breakpoint ();

  return 0;
}
