/* Copyright (C) 2022 Free Software Foundation, Inc.

   This file is part of GDB.

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

#ifndef JIT_PROTOCOL_UTIL_H
#define JIT_PROTOCOL_UTIL_H

#include "jit-protocol.h"

static int jit_empty (void)
{
  return __jit_debug_descriptor.relevant_entry == NULL;
}

static void jit_push_back (struct jit_code_entry *entry)
{
  entry->prev_entry = __jit_debug_descriptor.relevant_entry;
  __jit_debug_descriptor.relevant_entry = entry;

  if (entry->prev_entry != NULL)
    entry->prev_entry->next_entry = entry;
  else
    __jit_debug_descriptor.first_entry = entry;

  /* Notify GDB.  */
  __jit_debug_descriptor.action_flag = JIT_REGISTER;
  __jit_debug_register_code ();
}

static struct jit_code_entry *jit_pop_back (void)
{
  struct jit_code_entry *const entry = __jit_debug_descriptor.relevant_entry;
  struct jit_code_entry *const prev_entry = entry->prev_entry;

  if (prev_entry != NULL)
    {
      prev_entry->next_entry = NULL;
      entry->prev_entry = NULL;
    }
  else
    __jit_debug_descriptor.first_entry = NULL;

  /* Notify GDB.  */
  __jit_debug_descriptor.action_flag = JIT_UNREGISTER;
  __jit_debug_register_code ();

  __jit_debug_descriptor.relevant_entry = prev_entry;

  return entry;
}

#endif /* JIT_PROTOCOL_UTIL_H */
