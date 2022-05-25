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

#include "gdbsupport/selftest.h"
#include <iostream>
#include <set>
#include <sstream>

#include "gdbsupport/interval_tree.h"

/* A test type for storing in an interval tree.  Interval tree must be able to
   handle types without a default constructor, nonmoveable types, and
   noncopyable types.  */

struct test_interval
{
  test_interval (int low, int high) : low (low), high (high) {}

  test_interval () = delete;
  test_interval (const test_interval &) = delete;
  test_interval &operator= (const test_interval &) = delete;
  test_interval (test_interval &&) = delete;
  test_interval &operator= (test_interval &&) = delete;

  typedef int endpoint_type;
  const int low;
  const int high;
};

/* An expected ordering within an interval tree.  */

struct cmp_test_interval
{
  bool
  operator() (const test_interval &lhs, const test_interval &rhs) const
  {
    if (lhs.low != rhs.low)
      return lhs.low < rhs.low;
    return lhs.high < rhs.high;
  }
};

/* Insert an interval into a tree and verify its integrity.  */

static interval_tree<test_interval>::iterator
check_emplace (interval_tree<test_interval> &t, int low, int high)
{
  auto it = t.emplace (low, high);
  std::ostringstream () << t;
  return it;
}

/* Check that iterator range has specific content.  */

template <typename Iterator, typename EndIterator>
static void
check_iterator (Iterator it, EndIterator end)
{
  gdb_assert (it == end);
}

template <typename Iterator, typename EndIterator, typename... Args>
static void
check_iterator (Iterator it, EndIterator end, int low, int high,
		Args &&...args)
{
  gdb_assert (it != end);
  gdb_assert (it->low == low && it->high == high);
  ++it;
  check_iterator (it, end, std::forward<Args> (args)...);
}

/* Remove an interval from a tree and verify its integrity.  */

static void
check_erase (interval_tree<test_interval> &t,
	     interval_tree<test_interval>::iterator it)
{
  t.erase (it);
  std::ostringstream () << t;
}

/* Small tests for various corner cases.  */

static void
test_interval_tree_1 ()
{
  interval_tree<test_interval> t;
  check_iterator (t.find (0, 1), t.end ());
  auto it0 = check_emplace (t, 0, 1);
  check_iterator (t.find (0, 1), t.end (), 0, 1);
  check_erase (t, it0);
  check_iterator (t.find (0, 1), t.end ());
}

static void
test_interval_tree_2 ()
{
  interval_tree<test_interval> t;
  check_emplace (t, -16119041, -1);
  check_emplace (t, -1, 184549375);
  check_emplace (t, 0, 0);
  check_iterator (t.find (0, 0), t.end (), -1, 184549375, 0, 0);
}

static void
test_interval_tree_3 ()
{
  interval_tree<test_interval> t;
  check_emplace (t, 0, 65536);
  check_emplace (t, -1978987776, 10);
  check_iterator (t.find (0, 239), t.end (), -1978987776, 10, 0, 65536);
}

static void
test_interval_tree_4 ()
{
  interval_tree<test_interval> t;
  check_emplace (t, 0, 59);
  check_emplace (t, 0, 0);
  check_iterator (t.find (0, 0), t.end (), 0, 0, 0, 59);
}

static void
test_interval_tree_5 ()
{
  interval_tree<test_interval> t;
  check_emplace (t, -16777216, -16711936);
  check_emplace (t, 0, 0);
}

static void
test_interval_tree_6 ()
{
  interval_tree<test_interval> t;
  check_emplace (t, -167772160, -33554186);
  check_emplace (t, -16908034, -16712192);
  check_emplace (t, -1, -1);
  check_emplace (t, 0, 0);
}

static void
test_interval_tree_7 ()
{
  interval_tree<test_interval> t;
  check_emplace (t, 621897471, 983770623);
  check_emplace (t, 0, 0);
  check_emplace (t, 0, 0);
  check_emplace (t, 0, 8061696);
  check_iterator (t.find (0, 0), t.end (), 0, 0, 0, 0, 0, 8061696);
}

static void
test_interval_tree_8 ()
{
  interval_tree<test_interval> t;
  auto it0 = check_emplace (t, 1795875964, 1796149007);
  check_emplace (t, 3855, 252371968);
  check_erase (t, it0);
}

static void
test_interval_tree_9 ()
{
  interval_tree<test_interval> t;
  check_emplace (t, 0, 0);
  auto it1 = check_emplace (t, -603979523, 853292838);
  check_erase (t, it1);
}

static void
test_interval_tree_10 ()
{
  interval_tree<test_interval> t;
  auto it0 = check_emplace (t, 0, 6);
  check_emplace (t, -65527, 65280);
  check_emplace (t, 5636352, 26411009);
  check_erase (t, it0);
}

static void
test_interval_tree_11 ()
{
  interval_tree<test_interval> t;
  auto it0 = check_emplace (t, 62652437, 454794924);
  check_emplace (t, -188, 1145351340);
  check_emplace (t, 352332868, 1140916191);
  check_erase (t, it0);
}

static void
test_interval_tree_12 ()
{
  interval_tree<test_interval> t;
  auto it0 = check_emplace (t, -366592, 1389189);
  auto it1 = check_emplace (t, 16128, 29702);
  check_emplace (t, 2713716, 1946157056);
  check_emplace (t, 393215, 1962868736);
  check_erase (t, it0);
  check_emplace (t, 2560, 4128768);
  check_emplace (t, 0, 4128768);
  check_emplace (t, 0, 125042688);
  check_erase (t, it1);
}

/* Performance test: add and lookup 1M intervals.  */

static void
test_interval_tree_perf ()
{
  interval_tree<test_interval> t;
  const int count = 1000000;
  for (int i = 0; i < count; i++)
    t.emplace (i * 5, i * 5 + 5);
  std::ostringstream () << t;
  for (int i = 1; i < count; i++)
    check_iterator (t.find (i * 5 - 2, i * 5 + 2), t.end (), i * 5 - 5, i * 5,
		    i * 5, i * 5 + 5);
}

/* Test registration.  */

static void
test_interval_tree ()
{
  test_interval_tree_1 ();
  test_interval_tree_2 ();
  test_interval_tree_3 ();
  test_interval_tree_4 ();
  test_interval_tree_5 ();
  test_interval_tree_6 ();
  test_interval_tree_7 ();
  test_interval_tree_8 ();
  test_interval_tree_9 ();
  test_interval_tree_10 ();
  test_interval_tree_11 ();
  test_interval_tree_12 ();
  test_interval_tree_perf ();
}

void _initialize_interval_tree_selftests ();
void
_initialize_interval_tree_selftests ()
{
  selftests::register_test ("interval_tree", test_interval_tree);
}
