/* Interval tree for GDB, the GNU debugger.
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

#ifndef GDBSUPPORT_INTERVAL_TREE_H
#define GDBSUPPORT_INTERVAL_TREE_H

/* Based on:
   Cormen T. H., Leiserson C. E., Rivest R. L., and Stein C.. 2009.
   Introduction to Algorithms, Third Edition (3rd ed.). The MIT Press.
   Section 13: Red-Black Trees.
   Section 14.3: Interval trees.  */

#include "gdbsupport/gdb_assert.h"
#include <algorithm>
#include <iterator>
#include <utility>

template <typename Interval> struct interval_traits
{
  typedef typename Interval::endpoint_type endpoint_type;

  static endpoint_type
  low (const Interval &t)
  {
    return t.low;
  }

  static endpoint_type
  high (const Interval &t)
  {
    return t.high;
  }
};

template <typename Interval, typename Traits = interval_traits<Interval> >
class interval_tree
{
  using endpoint_type = typename Traits::endpoint_type;

  enum class direction
  {
    left,
    right
  };

  enum class color
  {
    black,
    red
  };

  struct node
  {
    template <typename... Args>
    explicit node (Args &&...args)
	: m_int (std::forward<Args> (args)...), m_max (Traits::high (m_int))
    {
    }

    ~node ()
    {
      delete m_left;
      delete m_right;
    }

    /* Prevent copies and moves - nodes must stay where they are created,
       otherwise unwanted iterator invalidation may happen.  */

    node (const node &) = delete;
    node &operator= (const node &) = delete;
    node (node &&) = delete;
    node &operator= (node &&) = delete;

    /* Use both the low and the high interval ends as a key.  Strictly
       speaking, only the low end is enough, however, using the high one as a
       tie-breaker makes iteration order more predictable.  */

    struct Key
    {
      endpoint_type m_low;
      endpoint_type m_high;

      bool
      operator<= (endpoint_type rhs) const
      {
	return m_low <= rhs;
      }

      bool
      operator>= (endpoint_type rhs) const
      {
	return m_low >= rhs;
      }

      bool
      operator<(const Key &rhs) const
      {
	return m_low != rhs.m_low ? m_low < rhs.m_low : m_high < rhs.m_high;
      }

      bool
      operator<= (const Key &rhs) const
      {
	return m_low != rhs.m_low ? m_low < rhs.m_low : m_high <= rhs.m_high;
      }

      bool
      operator>= (const Key &rhs) const
      {
	return m_low != rhs.m_low ? m_low > rhs.m_low : m_high >= rhs.m_high;
      }
    };

    Key
    key () const
    {
      return Key{ Traits::low (m_int), Traits::high (m_int) };
    }

    bool
    is_root () const
    {
      return m_p == nullptr;
    }

    node *
    child (direction which) const
    {
      return which == direction::left ? m_left : m_right;
    }

    void
    set_child (direction which, node *child)
    {
      if (which == direction::left)
	m_left = child;
      else
	m_right = child;
      if (child != nullptr)
	child->m_p = this;
    }

    node *
    sibling (direction which) const
    {
      return m_p->child (which);
    }

    bool
    is_child (direction which) const
    {
      return this == sibling (which);
    }

    direction
    which_child () const
    {
      return is_child (direction::left) ? direction::left : direction::right;
    }

    static bool
    is_black (const node *node)
    {
      return node == nullptr || node->m_color == color::black;
    }

    static bool
    is_red (const node *node)
    {
      return !is_black (node);
    }

    template <typename Stream>
    void
    print (Stream &stream, int indent = 0, const char *prefix = "") const
    {
      std::fill_n (std::ostream_iterator<char> (stream), indent, ' ');
      stream << prefix << (m_color == color::black ? "B" : "R") << " ["
	     << Traits::low (m_int) << ", " << Traits::high (m_int) << "] | "
	     << m_max << "\n";
      if (m_left != nullptr)
	m_left->print (stream, indent + 1, "L");
      if (m_right != nullptr)
	m_right->print (stream, indent + 1, "R");
    }

    void
    check () const
    {
      if (is_root ())
	gdb_assert (is_black (this));
      if (is_red (this))
	gdb_assert (is_black (m_left) && is_black (m_right));
      gdb_assert (Traits::low (m_int) <= Traits::high (m_int));
      endpoint_type max = Traits::high (m_int);
      if (m_left != nullptr)
	{
	  gdb_assert (m_left->key () <= key ());
	  if (max < m_left->m_max)
	    max = m_left->m_max;
	  m_left->check ();
	}
      if (m_right != nullptr)
	{
	  gdb_assert (m_right->key () >= key ());
	  if (max < m_right->m_max)
	    max = m_right->m_max;
	  m_right->check ();
	}
      gdb_assert (m_max == max);
    }

    void update_max ()
    {
      m_max = Traits::high (m_int);
      if (m_left != nullptr && m_left->m_max > m_max)
        m_max = m_left->m_max;
      if (m_right != nullptr && m_right->m_max > m_max)
        m_max = m_right->m_max;
    }

    color m_color = color::red;
    Interval m_int;
    node *m_left = nullptr;
    node *m_right = nullptr;
    node *m_p;
    endpoint_type m_max;
  };

public:
  ~interval_tree () { delete m_root; }

  class iterator
  {
  public:
    explicit iterator (node *x) : m_x (x) {}

    bool
    operator== (const iterator &rhs) const
    {
      return m_x == rhs.m_x;
    }

    bool
    operator!= (const iterator &rhs) const
    {
      return !(*this == rhs);
    }

    Interval &
    operator* () const
    {
      return m_x->m_int;
    }

    Interval *
    operator->() const
    {
      return &**this;
    }

  protected:
    node *m_x;

    friend class interval_tree;
  };

  iterator
  end () const
  {
    return iterator (nullptr);
  }

  template <typename... Args>
  iterator
  emplace (Args &&...args)
  {
    node *z = new node (std::forward<Args> (args)...);
    rb_insert (z);
    return iterator (z);
  }

  class find_iterator : public iterator
  {
  public:
    find_iterator (node *start, endpoint_type low, endpoint_type high)
	: iterator (start), m_low (low), m_high (high), m_stage (stage::left)
    {
      if (this->m_x != nullptr)
	interval_search ();
    }

    find_iterator &
    operator++ ()
    {
      interval_search ();
      return *this;
    }

    find_iterator
    operator++ (int)
    {
      find_iterator temp = *this;
      interval_search ();
      return temp;
    }

  private:
    enum class stage
    {
      left,
      overlap,
      right,
      up,
    };

    void
    interval_search ()
    {
      for (;;)
	{
	  switch (m_stage)
	    {
	    case stage::left:
	      if (this->m_x->m_left != nullptr
		  && this->m_x->m_left->m_max >= m_low)
		{
		  /* The left subtree contains an overlap - descend.  */
		  this->m_x = this->m_x->m_left;
		  continue;
		}
	      /* fallthrough */
	    case stage::overlap:
	      if (Traits::low (this->m_x->m_int) <= m_high
		  && m_low <= Traits::high (this->m_x->m_int))
		{
		  /* Overlapping interval found - pause the search.  */
		  m_stage = stage::right;
		  return;
		}
	      /* fallthrough */
	    case stage::right:
	      if (this->m_x->m_right != nullptr
		  && this->m_x->m_right->m_max >= m_low)
		{
		  /* The right subtree contains an overlap - descend.  */
		  this->m_x = this->m_x->m_right;
		  m_stage = stage::left;
		  continue;
		}
	      /* fallthrough */
	    case stage::up:
	      m_stage = !this->m_x->is_root ()
				&& this->m_x->is_child (direction::left)
			    ? stage::overlap
			    : stage::up;
	      this->m_x = this->m_x->m_p;
	      if (this->m_x == nullptr)
		return;
	      /* fallthrough */
	    }
	}
    }

    endpoint_type m_low;
    endpoint_type m_high;
    stage m_stage;
  };

  find_iterator
  find (endpoint_type low, endpoint_type high) const
  {
    return find_iterator (m_root, low, high);
  }

  void
  erase (iterator pos)
  {
    rb_delete (pos.m_x);
    delete pos.m_x;
  }

  void
  erase (Interval *object)
  {
    erase (iterator (reinterpret_cast<node *> (
	reinterpret_cast<char *> (object) - offsetof (node, m_int))));
  }

  void
  clear ()
  {
    delete m_root;
    m_root = nullptr;
  }

  template <typename Stream>
  friend Stream &
  operator<< (Stream &stream, const interval_tree &t)
  {
    if (t.m_root == nullptr)
      stream << "(nil)\n";
    else
      {
	t.m_root->template print (stream);
	t.m_root->check ();
      }
    return stream;
  }

private:
  static direction
  opposite (direction where)
  {
    return where == direction::left ? direction::right : direction::left;
  }

  void
  set_root (node *x)
  {
    m_root = x;
    if (x != nullptr)
      x->m_p = nullptr;
  }

  void
  rb_transplant (node *u, node *v)
  {
    if (u->is_root ())
      set_root (v);
    else
      u->m_p->set_child (u->which_child (), v);
  }

  void
  rotate (node *x, direction where)
  {
    node *y = x->child (opposite (where));
    x->set_child (opposite (where), y->child (where));
    rb_transplant (x, y);
    y->set_child (where, x);
    x->update_max ();
    y->update_max ();
  }

  /* Restore the red-black tree invariants after inserting node Z.  */

  void
  rb_insert_fixup (node *z)
  {
    while (node::is_red (z->m_p))
      {
	direction which = z->m_p->which_child ();
	node *y = z->m_p->sibling (opposite (which));
	/* In the drawings below we assume that z's parent is a left child and
	   z's uncle (y) is a right child.  */
	if (node::is_red (y))
	  {
	    /* Case 1: z's uncle (y) is red.
	       It is sufficient to adjust colors.
	       Whether z itself is a left or a right child does not matter,
	       in the drawing below we assume it is a left child.

			      |                           |
			  C(black)                   z=C(red)
			 /        \                    /     \
		     B(red)   y=D(red)   ==>      B(black)  D(black)
		    /      \    /     \           /       \ /       \
	      z=A(red)                        A(red)
		/     \                       /     \
	    */
	    z->m_p->m_color = color::black;
	    y->m_color = color::black;
	    z->m_p->m_p->m_color = color::red;
	    z = z->m_p->m_p;
	  }
	else
	  {
	    if (z->is_child (opposite (which)))
	      {
		/* Case 2: z's uncle (y) is black and z is a right child.
		   Rotate left and turn this into case 3.

			    |
			C(black)
			/       \
		    A(red)       y
		    /     \
		   m  z=B(red)
			/     \
		       n       k
		*/
		z = z->m_p;
		rotate (z, which);
	      }
	    /* Case 3: z's uncle (y) is black and z is a left child.
	       Rotate right and adjust colors.

			    |                       |
			C(black)                B(black)
			/       \               /       \
		    B(red)       y          A(red)     C(red)
		    /     \         ==>     /     \    /     \
	      z=A(red)    k                m       n  k       y
		/     \
	       m       n
	    */
	    z->m_p->m_color = color::black;
	    z->m_p->m_p->m_color = color::red;
	    rotate (z->m_p->m_p, opposite (which));
	  }
      }
    m_root->m_color = color::black;
  }

  void
  rb_insert (node *z)
  {
    /* Find an insertion point according to the key.
       Update m_max along the way.  */
    node *y = nullptr;
    node *x = m_root;
    direction which;
    endpoint_type high = Traits::high (z->m_int);
    while (x != nullptr)
      {
	y = x;
	which = z->key () < x->key () ? direction::left : direction::right;
	x = x->child (which);
	if (y->m_max < high)
	  y->m_max = high;
      }

    /* Perform insertion.  */
    if (y == nullptr)
      set_root (z);
    else
      {
	y->set_child (which, z);
	y->update_max ();
      }

    /* Restore the red-black tree invariants.  */
    rb_insert_fixup (z);
  }

  static node *
  tree_minimum (node *x)
  {
    while (x->m_left != nullptr)
      x = x->m_left;
    return x;
  }

  void
  rb_delete_fixup (node *x)
  {
    while (!x->is_root () && node::is_black (x))
      {
	direction which = x->which_child ();
	node *w = x->sibling (opposite (which));
	if (node::is_red (w))
	  {
	    w->m_color = color::black;
	    x->m_p->m_color = color::red;
	    rotate (x->m_p, which);
	    w = x->sibling (opposite (which));
	  }
	if (node::is_black (w->m_left) && node::is_black (w->m_right))
	  {
	    w->m_color = color::red;
	    x = x->m_p;
	  }
	else if (node::is_black (w->child (opposite (which))))
	  {
	    w->child (opposite (which))->m_color = color::black;
	    w->m_color = color::red;
	    rotate (w, opposite (which));
	    w = x->sibling (opposite (which));
	  }
	else
	  {
	    w->m_color = x->m_p->m_color;
	    x->m_p->m_color = color::black;
	    w->child (opposite (which))->m_color = color::black;
	    rotate (x->m_p, which);
	    x = m_root;
	  }
      }
    x->m_color = color::black;
  }

  void
  rb_delete (node *z)
  {
    node *y = z;
    color y_original_color = y->m_color;
    node *x;
    if (z->m_left == nullptr)
      {
	x = z->m_right;
	rb_transplant (z, x);
      }
    else if (z->m_right == nullptr)
      {
	x = z->m_left;
	rb_transplant (z, x);
      }
    else
      {
	y = tree_minimum (z->m_right);
	y_original_color = y->m_color;
	x = y->m_right;
	if (y->m_p == z)
	  {
	    x->m_p = y;
	  }
	else
	  {
	    rb_transplant (y, x);
	    y->set_child (direction::right, z->m_right);
	  }
	rb_transplant (z, y);
	y->set_child (direction::left, z->m_left);
	y->m_color = z->m_color;
      }
    if (x != nullptr && y_original_color == color::black)
      rb_delete_fixup (x);
  }

  node *m_root = nullptr;
};

#endif /* GDBSUPPORT_INTERVAL_TREE_H */
