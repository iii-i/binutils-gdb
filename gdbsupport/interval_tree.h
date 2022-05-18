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

#include <algorithm>
#include <iterator>

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
    explicit node (Args &&...args) : m_int (std::forward<Args> (args)...)
    {
    }

    ~node ()
    {
      delete m_left;
      delete m_right;
    }

    node (const node &) = delete;
    node &operator= (const node &) = delete;

    endpoint_type
    key () const
    {
      return Traits::low (m_int);
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

    direction
    which_sibling () const
    {
      return is_sibling (direction::left) ? direction::left : direction::right;
    }

    bool
    is_sibling (direction which) const
    {
      return this == sibling (which);
    }

    bool
    is_black () const
    {
      return m_color == color::black;
    }

    bool
    is_red () const
    {
      return m_color == color::red;
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

    void update_max ()
    {
      m_max = Traits::high (m_int);
      if (m_left != nullptr && m_left->m_max > m_max)
        m_max = m_left->m_max;
      if (m_right != nullptr && m_right->m_max > m_max)
        m_max = m_right->m_max;
    }

    color m_color;
    Interval m_int;
    node *m_left;
    node *m_right;
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
	: iterator (start), m_low (low), m_high (high),
	  m_stage (stage::overlap)
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
      overlap,
      left,
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
	    case stage::overlap:
	      if (Traits::low (this->m_x->m_int) <= m_high
		  && m_low <= Traits::high (this->m_x->m_int))
		{
		  m_stage = stage::left;
		  return;
		}
	      /* fallthrough */
	    case stage::left:
	      if (this->m_x->m_left != nullptr
		  && this->m_x->m_left->m_max >= m_low)
		{
		  this->m_x = this->m_x->m_left;
		  m_stage = stage::overlap;
		  continue;
		}
	      /* fallthrough */
	    case stage::right:
	      if (this->m_x->m_right != nullptr
		  && this->m_x->m_right->m_max >= m_low
		  && this->m_x->m_right->key () <= m_high)
		{
		  this->m_x = this->m_x->m_right;
		  m_stage = stage::overlap;
		  continue;
		}
	      /* fallthrough */
	    case stage::up:
	      m_stage = !this->m_x->is_root ()
				&& this->m_x->is_sibling (direction::left)
			    ? stage::right
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
  Stream &
  operator<< (Stream &stream) const
  {
    if (m_root == nullptr)
      stream << "(nil)\n";
    else
      m_root->template print (stream);
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
      u->m_p->set_child (u->which_sibling (), v);
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

  void
  rb_insert_fixup (node *z)
  {
    while (z->m_p != nullptr && z->m_p->is_red ())
      {
	direction which = z->m_p->which_sibling ();
	node *y = z->m_p->sibling (opposite (which));
	if (y->is_red ())
	  {
	    z->m_p->m_color = color::black;
	    y->m_color = color::black;
	    z->m_p->m_p->m_color = color::red;
	    z = z->m_p->m_p;
	  }
	else if (z->is_sibling (opposite (which)))
	  {
	    z = z->m_p;
	    rotate (z, which);
	  }
	else
	  {
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
    node *y = nullptr;
    node *x = m_root;
    direction which;
    while (x != nullptr)
      {
	y = x;
	which = z->key () < x->key () ? direction::left : direction::right;
	x = x->child (which);
      }
    if (y == nullptr)
      set_root (z);
    else
      y->set_child (which, z);
    z->set_child (direction::left, nullptr);
    z->set_child (direction::right, nullptr);
    z->m_color = color::red;
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
    while (!x->is_root () && x->is_black ())
      {
	direction which = x->which_sibling ();
	node *w = x->sibling (opposite (which));
	if (w->is_red ())
	  {
	    w->m_color = color::black;
	    x->m_p->m_color = color::red;
	    rotate (x->m_p, which);
	    w = x->sibling (opposite (which));
	  }
	if (w->m_left->is_black () && w->m_right->is_black ())
	  {
	    w->m_color = color::red;
	    x = x->m_p;
	  }
	else if (w->child (opposite (which))->is_black ())
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
