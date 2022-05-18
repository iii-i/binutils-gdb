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
   Cormen T. H., Leiserson C. E., Rivest R. L., and Stein C..  2009.
   Introduction to Algorithms, Third Edition (3rd ed.).  The MIT Press.
   Section 13: Red-Black Trees.
   Section 14.3: Interval trees.  */

#include "gdbsupport/gdb_assert.h"
#include <algorithm>
#include <iterator>
#include <utility>

/* Forward declarations.  */
template <typename Interval> struct interval_traits;
template <typename Interval, typename Traits> class interval_tree;
template <typename Interval, typename Traits> class interval_tree_key;
template <typename Interval, typename Traits> class interval_tree_node;
template <typename Interval, typename Traits> class interval_tree_iterator;
template <typename Interval, typename Traits>
class interval_tree_find_iterator;
template <typename Interval, typename Traits> class interval_tree;

/* Accessors for INTERVAL's low and high endpoints.  */

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

/* Interval tree key that uses both the low and the high interval ends.
   Strictly speaking, only the low end is enough, however, using the high one
   as a tie-breaker makes the iteration order more predictable.
   For internal use only.  */

template <typename Interval, typename Traits> class interval_tree_key
{
private:
  using endpoint_type = typename Traits::endpoint_type;

  interval_tree_key (endpoint_type low, endpoint_type high)
      : m_low (low), m_high (high)
  {
  }

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
  operator<(const interval_tree_key &rhs) const
  {
    return m_low != rhs.m_low ? m_low < rhs.m_low : m_high < rhs.m_high;
  }

  bool
  operator<= (const interval_tree_key &rhs) const
  {
    return m_low != rhs.m_low ? m_low < rhs.m_low : m_high <= rhs.m_high;
  }

  endpoint_type m_low;
  endpoint_type m_high;

  friend class interval_tree<Interval, Traits>;
  friend class interval_tree_node<Interval, Traits>;
};

/* Interval tree node.  For internal use only.  */

template <typename Interval, typename Traits> class interval_tree_node
{
private:
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

  using endpoint_type = typename Traits::endpoint_type;
  using key_type = interval_tree_key<Interval, Traits>;

  template <typename... Args>
  explicit interval_tree_node (Args &&...args)
      : m_int (std::forward<Args> (args)...), m_max (Traits::high (m_int))
  {
  }

  ~interval_tree_node ()
  {
    delete m_left;
    delete m_right;
  }

  /* Prevent copies and moves - nodes must stay where they are created,
     otherwise unwanted iterator invalidation may happen.  */

  interval_tree_node (const interval_tree_node &) = delete;
  interval_tree_node &operator= (const interval_tree_node &) = delete;
  interval_tree_node (interval_tree_node &&) = delete;
  interval_tree_node &operator= (interval_tree_node &&) = delete;

  key_type
  key () const
  {
    return key_type (Traits::low (m_int), Traits::high (m_int));
  }

  interval_tree_node *
  child (direction which) const
  {
    return which == direction::left ? m_left : m_right;
  }

  void
  set_child (direction which, interval_tree_node *child)
  {
    if (which == direction::left)
      m_left = child;
    else
      m_right = child;
    if (child != nullptr)
      child->m_p = this;
  }

  interval_tree_node *
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

  color m_color = color::red;
  Interval m_int;
  interval_tree_node *m_left = nullptr;
  interval_tree_node *m_right = nullptr;
  interval_tree_node *m_p;
  endpoint_type m_max;

  friend class interval_tree_iterator<Interval, Traits>;
  friend class interval_tree_find_iterator<Interval, Traits>;
  friend class interval_tree<Interval, Traits>;
};

/* Interval tree iterator.  Returns intervals with smaller low endpoints first,
   using high endpoints as tie-breakers.  Intervals with identical endpoints
   are returned in an undefined order.  */

template <typename Interval, typename Traits> class interval_tree_iterator
{
public:
  using node = interval_tree_node<Interval, Traits>;
  using tree = interval_tree<Interval, Traits>;

  /* Create an iterator referring to an OBJECT from an INTERVAL within a
     TREE.  */

  static interval_tree_iterator
  from_interval (tree &tree, Interval *object)
  {
    return interval_tree_iterator (
	&tree, reinterpret_cast<node *> (reinterpret_cast<char *> (object)
					 - offsetof (node, m_int)));
  }

  bool
  operator== (const interval_tree_iterator &rhs) const
  {
    return m_x == rhs.m_x;
  }

  bool
  operator!= (const interval_tree_iterator &rhs) const
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
  explicit interval_tree_iterator (const tree *tree, node *x)
      : m_tree (tree), m_x (x)
  {
  }

  const tree *m_tree;
  node *m_x;

  friend class interval_tree<Interval, Traits>;
};

/* Interval tree search iterator.  Returns intervals that overlap [LOW, HIGH].
   Intervals with smaller low endpoints are returned first, high endpoints are
   used as tie-breakers.  Intervals with identical endpoints are returned in an
   undefined order.  */

template <typename Interval, typename Traits>
class interval_tree_find_iterator
    : public interval_tree_iterator<Interval, Traits>
{
public:
  using node = interval_tree_node<Interval, Traits>;
  using endpoint_type = typename node::endpoint_type;
  using tree = interval_tree<Interval, Traits>;

  interval_tree_find_iterator &
  operator++ ()
  {
    interval_search ();
    return *this;
  }

  interval_tree_find_iterator
  operator++ (int)
  {
    interval_tree_find_iterator result = *this;
    interval_search ();
    return result;
  }

private:
  interval_tree_find_iterator (const tree *tree, node *start,
			       endpoint_type low, endpoint_type high)
      : interval_tree_iterator<Interval, Traits> (tree, start), m_low (low),
	m_high (high), m_stage (stage::left)
  {
    if (this->m_x != nullptr)
      interval_search ();
  }

  enum class stage
  {
    left,
    overlap,
    right,
    up,
  };

  /* Find the next overlapping interval.  */

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
	    m_stage = !this->m_tree->is_root (this->m_x)
			      && this->m_x->is_child (node::direction::left)
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

  friend class interval_tree<Interval, Traits>;
};

/* A container for intervals.  Supports efficient addition, lookup of
   overlapping intervals and removal.  */

template <typename Interval, typename Traits = interval_traits<Interval> >
class interval_tree
{
public:
  using node = interval_tree_node<Interval, Traits>;
  using direction = typename node::direction;
  using color = typename node::color;
  using endpoint_type = typename node::endpoint_type;
  using iterator = interval_tree_iterator<Interval, Traits>;

  ~interval_tree () { delete m_root; }

  iterator
  begin () const
  {
    if (m_root == nullptr)
      return end ();
    return iterator (this, tree_minimum (m_root));
  }

  iterator
  end () const
  {
    return iterator (this, nullptr);
  }

  /* Construct a new interval and insert it into this tree.
     Return an iterator pointing to this interval.  */

  template <typename... Args>
  iterator
  emplace (Args &&...args)
  {
    node *z = new node (std::forward<Args> (args)...);
    rb_insert (z);
    return iterator (this, z);
  }

  /* Find all intervals in this tree that overlap with [LOW, HIGH].  */

  interval_tree_find_iterator<Interval, Traits>
  find (endpoint_type low, endpoint_type high) const
  {
    return interval_tree_find_iterator<Interval, Traits> (this, m_root, low,
							  high);
  }

  /* Remove a single interval from this tree.
     All iterators except POS stay valid.  */

  void
  erase (iterator pos)
  {
    rb_delete (pos.m_x);
    delete pos.m_x;
  }

  /* Remove all intervals from this tree.  */

  void
  clear ()
  {
    delete m_root;
    m_root = nullptr;
  }

  /* Print T and check its invariants.  */

  template <typename Stream>
  friend Stream &
  operator<< (Stream &stream, const interval_tree &t)
  {
    if (t.m_root == nullptr)
      stream << "(nil)\n";
    else
      {
	t.rb_print (stream, t.m_root);
	int black_height = -1;
	t.rb_check (t.m_root, 0, black_height);
      }
    return stream;
  }

private:
  static direction
  opposite (direction where)
  {
    return where == direction::left ? direction::right : direction::left;
  }

  bool
  is_root (const node *x) const
  {
    return x->m_p == nullptr;
  }

  bool
  is_black (const node *x) const
  {
    return x == nullptr || x->m_color == color::black;
  }

  bool
  is_red (const node *x) const
  {
    return !is_black (x);
  }

  /* Link X into the root position.  */

  void
  set_root (node *x)
  {
    m_root = x;
    if (x != nullptr)
      x->m_p = nullptr;
  }

  /* Link V in place of U.  */

  void
  rb_transplant (node *u, node *v)
  {
    if (is_root (u))
      set_root (v);
    else
      u->m_p->set_child (u->which_child (), v);
  }

  /* Perform a left or a right rotation of X.

          |                     |
        x=A                   x=C
         / \     left ==>      / \
        B y=C    <== right  y=A   E
           / \               / \
          D   E             B   D
  */

  void
  rotate (node *x, direction where)
  {
    node *y = x->child (opposite (where));
    x->set_child (opposite (where), y->child (where));
    rb_transplant (x, y);
    y->set_child (where, x);
    update_max_1 (x);
    update_max_1 (y);
  }

  /* Restore the red-black tree invariants after inserting node Z.  */

  void
  rb_insert_fixup (node *z)
  {
    while (is_red (z->m_p))
      {
	direction which = z->m_p->which_child ();
	node *y = z->m_p->sibling (opposite (which));
	/* In the drawings below we assume that z's parent is a left child.  */
	if (is_red (y))
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
		   Rotate left in order to turn this into case 3.

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

  /* Insert node Z into this tree.  */

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
	update_max_1 (y);
      }

    /* Restore the red-black tree invariants.  */
    rb_insert_fixup (z);
  }

  /* Find an interval with the smallest key in a subtree rooted at X.  */

  static node *
  tree_minimum (node *x)
  {
    while (x->m_left != nullptr)
      x = x->m_left;
    return x;
  }

  /* Recompute m_max of node X.  */

  void
  update_max_1 (node *x)
  {
    x->m_max = Traits::high (x->m_int);
    if (x->m_left != nullptr && x->m_left->m_max > x->m_max)
      x->m_max = x->m_left->m_max;
    if (x->m_right != nullptr && x->m_right->m_max > x->m_max)
      x->m_max = x->m_right->m_max;
  }

  /* Recompute m_max of node X and its ancestors.  */

  void
  update_max (node *x)
  {
    for (; x != nullptr; x = x->m_p)
      update_max_1 (x);
  }

  /* Restore the red-black tree invariants after deleting a node.
     Note that X is not the deleted node, but rather the node at which
     inconsistencies start.  */

  void
  rb_delete_fixup (node *x)
  {
    while (!is_root (x) && is_black (x))
      {
	direction which = x->which_child ();
	/* In the drawings below we assume that x is a left child.  */
	node *w = x->sibling (opposite (which));
	if (is_red (w))
	  {
	    /* Case 1: x's sibling (w) is red.
	       Adjust colors and rotate left in order to turn this into case 2,
	       3 or 4.

			 |                                     |
		     A(black)                              C(black)
		     /       \                             /       \
	      x=B(black)  w=C(red)         ==>         A(red)   E(black)
			    /     \                    /     \
		       D(black)  E(black)       x=B(black)  w=D(black)
	    */
	    w->m_color = color::black;
	    x->m_p->m_color = color::red;
	    rotate (x->m_p, which);
	    w = x->sibling (opposite (which));
	  }
	if (is_black (w->m_left) && is_black (w->m_right))
	  {
	    /* Case 2: x's sibling (w) is black, and so are w's children.
	       It is sufficient to adjust colors.  */
	    w->m_color = color::red;
	    x = x->m_p;
	  }
	else
	  {
	    if (is_black (w->child (opposite (which))))
	      {
		/* Case 3: x's sibling (w) is black, w's left child is red,
		   and w's right child is black.  Adjust colors and rotate
		   right in order to turn this into case 4.

			     |
			     A
			  /     \
		   x=B(black)  w=D(black)
				 /       \
			     E(red)   F(black)
		*/
		w->child (which)->m_color = color::black;
		w->m_color = color::red;
		rotate (w, opposite (which));
		w = x->sibling (opposite (which));
	      }
	    /* Case 4: x's sibling (w) is black, and w's right child is red.
	       Adjust colors and rotate left.

			 |                                  |
		       A(?)                             w=E(?)
		      /     \                          /      \
	       x=B(black)  w=E(black)      =>    A(black)  D(black)
			     /       \              /    \
			    G      D(red)      x=B(black) G
	    */
	    w->m_color = x->m_p->m_color;
	    x->m_p->m_color = color::black;
	    w->child (opposite (which))->m_color = color::black;
	    rotate (x->m_p, which);
	    /* No more inconsistencies can arise, exit the loop.  */
	    x = m_root;
	  }
      }
    x->m_color = color::black;
  }

  /* Remove node Z from this tree.  */

  void
  rb_delete (node *z)
  {
    node *y = z;
    color y_original_color = y->m_color;
    node *x;
    if (z->m_left == nullptr)
      {
	/* There is no left subtree, link z's right subtree in place of z.  */
	x = z->m_right;
        z->m_right = nullptr;
	rb_transplant (z, x);
	update_max (z->m_p);
      }
    else if (z->m_right == nullptr)
      {
	/* There is no right subtree, link z's left subtree in place of z.  */
	x = z->m_left;
        z->m_left = nullptr;
	rb_transplant (z, x);
	update_max (z->m_p);
      }
    else
      {
	/* y is z's successor: the leftmost node in z's right subtree.  It has
	   no left subtree.  First, link its right subtree (x) in its
	   place.  */
	y = tree_minimum (z->m_right);
	y_original_color = y->m_color;
	x = y->m_right;
	node *m;
	if (y->m_p == z)
	  {
	    m = y;
	  }
	else
	  {
	    m = y->m_p;
	    rb_transplant (y, x);
	    y->set_child (direction::right, z->m_right);
	  }
	/* Now that y is unlinked from its original position, link it in z's
	   place.  */
	rb_transplant (z, y);
	y->set_child (direction::left, z->m_left);
	z->m_left = nullptr;
	z->m_right = nullptr;
	y->m_color = z->m_color;
	/* Finally, recompute m_max, which we need to do from y's parent's
	   position.  If y's parent was z, then use y itself, because y was
	   linked in z's position.  Otherwise, use y's original parent.  */
	update_max (m);
      }

    if (x != nullptr && y_original_color == color::black)
      {
	/* Restore the red-black tree invariants.  The inconsistencies start at
	   the deepest node that was touched.  */
	rb_delete_fixup (x);
      }
  }

  /* Print node X and its children.  */

  template <typename Stream>
  void
  rb_print (Stream &stream, const node *x, int indent = 0,
	    const char *prefix = "") const
  {
    std::fill_n (std::ostream_iterator<char> (stream), indent, ' ');
    stream << prefix << (x->m_color == color::black ? "B" : "R") << " ["
	   << Traits::low (x->m_int) << ", " << Traits::high (x->m_int)
	   << "] | " << x->m_max << "\n";
    if (x->m_left != nullptr)
      rb_print (stream, x->m_left, indent + 1, "L");
    if (x->m_right != nullptr)
      rb_print (stream, x->m_right, indent + 1, "R");
  }

  /* Checks invariants of this node and of its children.  */

  void
  rb_check (const node *x, int cur_black_height, int &black_height) const
  {
    if (is_root (x))
      {
	gdb_assert (x->m_p == nullptr);
	gdb_assert (is_black (x));
      }
    if (is_red (x))
      gdb_assert (is_black (x->m_left) && is_black (x->m_right));
    gdb_assert (Traits::low (x->m_int) <= Traits::high (x->m_int));
    endpoint_type max = Traits::high (x->m_int);
    if (x->m_left == nullptr || x->m_right == nullptr)
      {
	if (black_height < 0)
	  black_height = cur_black_height;
	else
	  gdb_assert (black_height == cur_black_height);
      }
    if (x->m_left != nullptr)
      {
	gdb_assert (x->m_left->m_p == x);
	gdb_assert (x->m_left->key () <= x->key ());
	if (max < x->m_left->m_max)
	  max = x->m_left->m_max;
	rb_check (x->m_left, cur_black_height + (is_black (x->m_left) ? 1 : 0),
		  black_height);
      }
    if (x->m_right != nullptr)
      {
	gdb_assert (x->m_right->m_p == x);
	gdb_assert (x->key () <= x->m_right->key ());
	if (max < x->m_right->m_max)
	  max = x->m_right->m_max;
	rb_check (x->m_right,
		  cur_black_height + (is_black (x->m_right) ? 1 : 0),
		  black_height);
      }
    gdb_assert (x->m_max == max);
  }

  node *m_root = nullptr;

  friend class interval_tree_find_iterator<Interval, Traits>;
};

#endif /* GDBSUPPORT_INTERVAL_TREE_H */
