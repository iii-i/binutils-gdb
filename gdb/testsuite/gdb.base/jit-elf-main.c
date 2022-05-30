/* This test program is part of GDB, the GNU debugger.

   Copyright 2011-2022 Free Software Foundation, Inc.

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

/* Simulate loading of JIT code.  */

#include <elf.h>
#include <link.h>
#include <errno.h>
#include <fcntl.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>

#include "jit-elf-util.h"
#include "jit-protocol-util.h"

static void
usage (void)
{
  fprintf (stderr, "Usage: jit-elf-main libraries...\n");
  exit (1);
}

/* Defined by the .exp file if testing attach.  */
#ifndef ATTACH
#define ATTACH 0
#endif

#ifndef MAIN
#define MAIN main
#endif

/* Used to spin waiting for GDB.  */
volatile int wait_for_gdb = ATTACH;
#define WAIT_FOR_GDB do {} while (wait_for_gdb)

/* The current process's PID.  GDB retrieves this.  */
int mypid;

int
MAIN (int argc, char *argv[])
{
  int i;
  alarm (300);
  /* Used as backing storage for GDB to populate argv.  */
  char *fake_argv[10];

  mypid = getpid ();
  /* gdb break here 0  */

  if (argc < 2)
    {
      usage ();
      exit (1);
    }

  for (i = 1; i < argc; ++i)
    {
      size_t obj_size;
      void *load_addr = n_jit_so_address (i);
      printf ("Loading %s as JIT at %p\n", argv[i], load_addr);
      void *addr = load_elf (argv[i], &obj_size, load_addr);

      char name[32];
      sprintf (name, "jit_function_%04d", i);
      int (*jit_function) (void) = (int (*) (void)) load_symbol (addr, name);

      /* Link entry at the end of the list.  */
      struct jit_code_entry *const entry = calloc (1, sizeof (*entry));
      entry->symfile_addr = (const char *)addr;
      entry->symfile_size = obj_size;
      jit_push_back (entry);

      if (jit_function () != 42)
	{
	  fprintf (stderr, "unexpected return value\n");
	  exit (1);
	}
    }

  WAIT_FOR_GDB; i = 0;  /* gdb break here 1 */

  /* Now unregister them all in reverse order.  */
  while (!jit_empty ())
    free (jit_pop_back ());

  WAIT_FOR_GDB; return 0;  /* gdb break here 2  */
}
