/* Some code common to C and ObjC front ends.
   Copyright (C) 2001-2019 Free Software Foundation, Inc.

This file is part of GCC.

GCC is free software; you can redistribute it and/or modify it under
the terms of the GNU General Public License as published by the Free
Software Foundation; either version 3, or (at your option) any later
version.

GCC is distributed in the hope that it will be useful, but WITHOUT ANY
WARRANTY; without even the implied warranty of MERCHANTABILITY or
FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
for more details.

You should have received a copy of the GNU General Public License
along with GCC; see the file COPYING3.  If not see
<http://www.gnu.org/licenses/>.  */

#include "config.h"
#include "system.h"
#include "coretypes.h"
#include "c-tree.h"
#include "intl.h"
#include "c-family/c-pretty-print.h"
#include "tree-pretty-print.h"
#include "gimple-pretty-print.h"
#include "langhooks.h"
#include "c-objc-common.h"
#include "gcc-rich-location.h"

static bool c_tree_printer (pretty_printer *, text_info *, const char *,
			    int, bool, bool, bool, bool *, const char **);

bool
c_missing_noreturn_ok_p (tree decl)
{
  /* A missing noreturn is not ok for freestanding implementations and
     ok for the `main' function in hosted implementations.  */
  return flag_hosted && MAIN_NAME_P (DECL_ASSEMBLER_NAME (decl));
}

/* Called from check_global_declaration.  */

bool
c_warn_unused_global_decl (const_tree decl)
{
  if (TREE_CODE (decl) == FUNCTION_DECL && DECL_DECLARED_INLINE_P (decl))
    return false;
  if (DECL_IN_SYSTEM_HEADER (decl))
    return false;

  return true;
}

/* Initialization common to C and Objective-C front ends.  */
bool
c_objc_common_init (void)
{
  c_init_decl_processing ();

  return c_common_init ();
}

/* Return true if it's worth saying that TYPE1 is also known as TYPE2.  */

static bool
useful_aka_type_p (tree type1, tree type2)
{
  if (type1 == type2)
    return false;

  if (type1 == error_mark_node || type2 == error_mark_node)
    return false;

  if (TREE_CODE (type1) != TREE_CODE (type2))
    return true;

  if (typedef_variant_p (type1))
    {
      /* Saying that "foo" is also known as "struct foo" or
	 "struct <anonymous>" is unlikely to be useful, since users of
	 structure-like types would already know that they're structures.
	 The same applies to unions and enums; in general, printing the
	 tag is only useful if it has a different name.  */
      tree_code code = TREE_CODE (type2);
      tree id2 = TYPE_IDENTIFIER (type2);
      if ((code == RECORD_TYPE || code == UNION_TYPE || code == ENUMERAL_TYPE)
	  && (!id2 || TYPE_IDENTIFIER (type1) == id2))
	return false;

      return true;
    }
  else
    {
      switch (TREE_CODE (type1))
	{
	case POINTER_TYPE:
	case REFERENCE_TYPE:
	  return useful_aka_type_p (TREE_TYPE (type1), TREE_TYPE (type2));

	case ARRAY_TYPE:
	  return (useful_aka_type_p (TYPE_DOMAIN (type1), TYPE_DOMAIN (type2))
		  || useful_aka_type_p (TREE_TYPE (type1), TREE_TYPE (type2)));

	case FUNCTION_TYPE:
	  {
	    tree args1 = TYPE_ARG_TYPES (type1);
	    tree args2 = TYPE_ARG_TYPES (type2);
	    while (args1 != args2)
	      {
		/* Although this shouldn't happen, it seems to wrong to assert
		   for it in a diagnostic routine.  */
		if (!args1 || args1 == void_type_node)
		  return true;
		if (!args2 || args2 == void_type_node)
		  return true;
		if (useful_aka_type_p (TREE_VALUE (args1), TREE_VALUE (args2)))
		  return true;
		args1 = TREE_CHAIN (args1);
		args2 = TREE_CHAIN (args2);
	      }
	    return useful_aka_type_p (TREE_TYPE (type1), TREE_TYPE (type2));
	  }

	default:
	  return true;
	}
    }
}

/* Print T to CPP.  */

static void
print_type (c_pretty_printer *cpp, tree t, bool *quoted)
{
  gcc_assert (TYPE_P (t));
  struct obstack *ob = pp_buffer (cpp)->obstack;
  char *p = (char *) obstack_base (ob);
  /* Remember the end of the initial dump.  */
  int len = obstack_object_size (ob);

  tree name = TYPE_NAME (t);
  if (name && TREE_CODE (name) == TYPE_DECL && DECL_NAME (name))
    pp_identifier (cpp, lang_hooks.decl_printable_name (name, 2));
  else
    cpp->type_id (t);

  /* If we're printing a type that involves typedefs, also print the
     stripped version.  But sometimes the stripped version looks
     exactly the same, so we don't want it after all.  To avoid
     printing it in that case, we play ugly obstack games.  */
  if (TYPE_CANONICAL (t) && useful_aka_type_p (t, TYPE_CANONICAL (t)))
    {
      c_pretty_printer cpp2;
      /* Print the stripped version into a temporary printer.  */
      cpp2.type_id (TYPE_CANONICAL (t));
      struct obstack *ob2 = cpp2.buffer->obstack;
      /* Get the stripped version from the temporary printer.  */
      const char *aka = (char *) obstack_base (ob2);
      int aka_len = obstack_object_size (ob2);
      int type1_len = obstack_object_size (ob) - len;

      /* If they are identical, bail out.  */
      if (aka_len == type1_len && memcmp (p + len, aka, aka_len) == 0)
	return;

      /* They're not, print the stripped version now.  */
      if (*quoted)
	pp_end_quote (cpp, pp_show_color (cpp));
      pp_c_whitespace (cpp);
      pp_left_brace (cpp);
      pp_c_ws_string (cpp, _("aka"));
      pp_c_whitespace (cpp);
      if (*quoted)
	pp_begin_quote (cpp, pp_show_color (cpp));
      cpp->type_id (TYPE_CANONICAL (t));
      if (*quoted)
	pp_end_quote (cpp, pp_show_color (cpp));
      pp_right_brace (cpp);
      /* No further closing quotes are needed.  */
      *quoted = false;
    }
}

/* Called during diagnostic message formatting process to print a
   source-level entity onto BUFFER.  The meaning of the format specifiers
   is as follows:
   %D: a general decl,
   %E: an identifier or expression,
   %F: a function declaration,
   %G: a Gimple statement,
   %K: a CALL_EXPR,
   %T: a type.
   %V: a list of type qualifiers from a tree.
   %v: an explicit list of type qualifiers
   %#v: an explicit list of type qualifiers of a function type.

   Please notice when called, the `%' part was already skipped by the
   diagnostic machinery.  */
static bool
c_tree_printer (pretty_printer *pp, text_info *text, const char *spec,
		int precision, bool wide, bool set_locus, bool hash,
		bool *quoted, const char **)
{
  tree t = NULL_TREE;
  // FIXME: the next cast should be a dynamic_cast, when it is permitted.
  c_pretty_printer *cpp = (c_pretty_printer *) pp;
  pp->padding = pp_none;

  if (precision != 0 || wide)
    return false;

  if (*spec == 'G')
    {
      percent_G_format (text);
      return true;
    }

  if (*spec == 'K')
    {
      t = va_arg (*text->args_ptr, tree);
      percent_K_format (text, EXPR_LOCATION (t), TREE_BLOCK (t));
      return true;
    }

  if (*spec != 'v')
    {
      t = va_arg (*text->args_ptr, tree);
      if (set_locus)
	text->set_location (0, DECL_SOURCE_LOCATION (t),
			    SHOW_RANGE_WITH_CARET);
    }

  switch (*spec)
    {
    case 'D':
      if (VAR_P (t) && DECL_HAS_DEBUG_EXPR_P (t))
	{
	  t = DECL_DEBUG_EXPR (t);
	  if (!DECL_P (t))
	    {
	      cpp->expression (t);
	      return true;
	    }
	}
      /* FALLTHRU */

    case 'F':
      if (DECL_NAME (t))
	{
	  pp_identifier (cpp, lang_hooks.decl_printable_name (t, 2));
	  return true;
	}
      break;

    case 'T':
      print_type (cpp, t, quoted);
      return true;

    case 'E':
      if (TREE_CODE (t) == IDENTIFIER_NODE)
	pp_identifier (cpp, IDENTIFIER_POINTER (t));
      else
	cpp->expression (t);
      return true;

    case 'V':
      pp_c_type_qualifier_list (cpp, t);
      return true;

    case 'v':
      pp_c_cv_qualifiers (cpp, va_arg (*text->args_ptr, int), hash);
      return true;

    default:
      return false;
    }

  pp_string (cpp, _("({anonymous})"));
  return true;
}

/* C-specific implementation of range_label::get_text () vfunc for
   range_label_for_type_mismatch.  */

label_text
range_label_for_type_mismatch::get_text (unsigned /*range_idx*/) const
{
  if (m_labelled_type == NULL_TREE)
    return label_text (NULL, false);

  c_pretty_printer cpp;
  bool quoted = false;
  print_type (&cpp, m_labelled_type, &quoted);
  return label_text (xstrdup (pp_formatted_text (&cpp)), true);
}


/* In C and ObjC, all decls have "C" linkage.  */
bool
has_c_linkage (const_tree decl ATTRIBUTE_UNUSED)
{
  return true;
}

void
c_initialize_diagnostics (diagnostic_context *context)
{
  pretty_printer *base = context->printer;
  c_pretty_printer *pp = XNEW (c_pretty_printer);
  context->printer = new (pp) c_pretty_printer ();

  /* It is safe to free this object because it was previously XNEW()'d.  */
  base->~pretty_printer ();
  XDELETE (base);

  c_common_diagnostics_set_defaults (context);
  diagnostic_format_decoder (context) = &c_tree_printer;
}

int
c_types_compatible_p (tree x, tree y)
{
  return comptypes (TYPE_MAIN_VARIANT (x), TYPE_MAIN_VARIANT (y));
}

/* Determine if the type is a vla type for the backend.  */

bool
c_vla_unspec_p (tree x, tree fn ATTRIBUTE_UNUSED)
{
  return c_vla_type_p (x);
}

/* Special routine to get the alias set of T for C.  */

alias_set_type
c_get_alias_set (tree t)
{
  /* Allow aliasing between enumeral types and the underlying
     integer type.  This is required since those are compatible types.  */
  if (TREE_CODE (t) == ENUMERAL_TYPE)
    {
      tree t1 = c_common_type_for_size (tree_to_uhwi (TYPE_SIZE (t)),
					/* short-cut commoning to signed
					   type.  */
					false);
      return get_alias_set (t1);
    }

  return c_common_get_alias_set (t);
}
