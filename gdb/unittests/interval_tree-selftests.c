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
#include <assert.h>
#include <iostream>
#include <set>
#include <sstream>

#ifdef FUZZING_BUILD_MODE_UNSAFE_FOR_PRODUCTION
#undef gdb_assert
#define gdb_assert assert
#endif

#include "gdbsupport/interval_tree.h"

struct test_interval
{
  test_interval (int low, int high) : low (low), high (high) {}

  typedef int endpoint_type;
  const int low;
  const int high;
};

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

static interval_tree<test_interval>::iterator
check_emplace (interval_tree<test_interval> &t, int low, int high)
{
  auto it = t.emplace (low, high);
  std::ostringstream () << t;
  return it;
}

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

static void
check_erase (interval_tree<test_interval> &t,
	     interval_tree<test_interval>::iterator it)
{
  t.erase (it);
  std::ostringstream () << t;
}

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
}

class FuzzerInput
{
public:
  FuzzerInput (const unsigned char *data, size_t size)
      : m_data (data), m_size (size)
  {
  }

  bool
  end () const
  {
    return m_size == 0;
  }

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
  unsigned char
  read_byte ()
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

extern "C" int LLVMFuzzerTestOneInput (const unsigned char *, size_t);
extern "C" int
LLVMFuzzerTestOneInput (const unsigned char *data, size_t size)
{
  FuzzerInput input (data, size);
  interval_tree<test_interval> t;
  std::vector<std::pair<decltype (t)::iterator, size_t> > t_iterators;
  std::multiset<test_interval, cmp_test_interval> exp;
  std::vector<decltype (exp)::iterator> exp_iterators;
  size_t add_counter = 0;

  static const char *debug_str = getenv ("DEBUG");
  static int debug = debug_str == nullptr ? 0 : atoi (debug_str);

  while (!input.end ())
    {
      switch (input.get<char> () % 3)
	{
	case 0:
	  {
	    /* Add.  */
	    int low = input.get<int> (), high = input.get<int> ();
	    if (low > high)
	      std::swap (low, high);
	    if (debug)
	      std::cout << "auto it" << add_counter << " = check_emplace (t, "
			<< low << ", " << high << ");" << std::endl;
	    t_iterators.push_back (
		std::make_pair (t.emplace (low, high), add_counter));
	    if (debug)
	      std::cout << "/*\n" << t << "*/" << std::endl;
	    else
	      std::ostringstream () << t;
	    exp_iterators.push_back (exp.emplace (low, high));
	    add_counter += 1;
	    break;
	  }
	case 1:
	  {
	    /* Find.  */
	    int low = input.get<int> (), high = input.get<int> ();
	    if (low > high)
	      std::swap (low, high);
	    if (debug)
	      std::cout << "check_iterator (t.find (" << low << ", " << high
			<< "), t.end ()" << std::flush;
	    auto it = t.find (low, high);
	    for (const test_interval &exp_interval : exp)
	      {
		if (high < exp_interval.low || low > exp_interval.high)
		  continue;
		if (debug)
		  std::cout << ", " << exp_interval.low << ", "
			    << exp_interval.high << std::flush;
		gdb_assert (it->low == exp_interval.low
			    && it->high == exp_interval.high);
		++it;
	      }
	    if (debug)
	      std::cout << ");" << std::endl;
	    gdb_assert (it == t.end ());
	    break;
	  }
	case 2:
	  {
	    /* Remove.  */
	    if (!t_iterators.empty ())
	      {
		int index = input.get<int> () % t_iterators.size ();
		if (debug)
		  std::cout << "check_erase (t, it"
			    << t_iterators[index].second << ");" << std::endl;
		t.erase (t_iterators[index].first);
		t_iterators.erase (t_iterators.begin () + index);
		exp.erase (exp_iterators[index]);
		exp_iterators.erase (exp_iterators.begin () + index);
                if (debug)
                  std::cout << "/*\n" << t << "*/" << std::endl;
                else
                  std::ostringstream () << t;
	      }
	    break;
	  }
	}
    }
  return 0;
}

void _initialize_interval_tree_selftests ();
void
_initialize_interval_tree_selftests ()
{
#ifndef FUZZING_BUILD_MODE_UNSAFE_FOR_PRODUCTION
  selftests::register_test ("interval_tree", test_interval_tree);
#endif
}
