/* Tests interval tree for GDB, the GNU debugger.
   Copyright (C) 2022 Free Software Foundation, Inc.

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

#include "defs.h"

#include "gdbsupport/interval_tree.h"
#include "gdbsupport/selftest.h"
#include <set>

struct test_interval
{
  test_interval (int low, int high) : low (low), high (high) {}

  typedef int endpoint_type;
  const int low;
  const int high;
};

struct cmp_test_interval
{
  bool operator() (const test_interval &lhs, const test_interval &rhs) const
  {
    return lhs.low < rhs.low;
  }
};

static void
test_interval_tree ()
{
  interval_tree<test_interval> t;

  gdb_assert (t.find (0, 1) == t.end ());

  auto interval0 = t.emplace (0, 1);
  auto it = t.find (0, 1);
  gdb_assert (it != t.end () && it->low == 0 && it->high == 1);
  ++it;
  gdb_assert (it == t.end ());

  t.erase (interval0);
  gdb_assert (t.find (0, 1) == t.end ());
}

class FuzzerInput
{
public:
  FuzzerInput (const unsigned char *data, size_t size) : m_data (data), m_size (size) {}

  bool end () const { return m_size == 0; }

  template <typename T>
  T
  get ()
  {
    T result = 0;
    for (size_t i = 0; i < sizeof (T); i++)
      result |= read_byte () << (i * 8);
    return result;
  }

private:
  unsigned char read_byte ()
  {
    if (end ())
      return 0;
    m_data++;
    m_size--;
    return m_data[-1];
  }

  const unsigned char *m_data;
  size_t m_size;
};

extern "C" int LLVMFuzzerTestOneInput(const unsigned char *, size_t);
extern "C" int
LLVMFuzzerTestOneInput(const unsigned char *data, size_t size)
{
  FuzzerInput input (data, size);
  interval_tree<test_interval> actual;
  std::set<test_interval, cmp_test_interval> expected;

  while (!input.end ())
    {
      switch (input.get<char> () % 1)
        {
        case 0:
          {
            int low = input.get<int> (), high = input.get<int> ();
            if (low <= high)
              {
                actual.emplace (low, high);
                expected.emplace (low, high);
              }
            break;
          }
        case 1:
          break;
        }
    }
  return 0;
}

void _initialize_interval_tree_selftests ();
void
_initialize_interval_tree_selftests ()
{
  selftests::register_test ("interval_tree", test_interval_tree);
}
