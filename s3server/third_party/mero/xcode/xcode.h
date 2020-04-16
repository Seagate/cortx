/* -*- C -*- */
/*
 * COPYRIGHT 2015 XYRATEX TECHNOLOGY LIMITED
 *
 * THIS DRAWING/DOCUMENT, ITS SPECIFICATIONS, AND THE DATA CONTAINED
 * HEREIN, ARE THE EXCLUSIVE PROPERTY OF XYRATEX TECHNOLOGY
 * LIMITED, ISSUED IN STRICT CONFIDENCE AND SHALL NOT, WITHOUT
 * THE PRIOR WRITTEN PERMISSION OF XYRATEX TECHNOLOGY LIMITED,
 * BE REPRODUCED, COPIED, OR DISCLOSED TO A THIRD PARTY, OR
 * USED FOR ANY PURPOSE WHATSOEVER, OR STORED IN A RETRIEVAL SYSTEM
 * EXCEPT AS ALLOWED BY THE TERMS OF XYRATEX LICENSES AND AGREEMENTS.
 *
 * YOU SHOULD HAVE RECEIVED A COPY OF XYRATEX'S LICENSE ALONG WITH
 * THIS RELEASE. IF NOT PLEASE CONTACT A XYRATEX REPRESENTATIVE
 * http://www.xyratex.com/contact
 *
 * Original author: Nikita Danilov <nikita_danilov@xyratex.com>
 * Original creation date: 25-Dec-2011
 */

#pragma once

#ifndef __MERO_XCODE_XCODE_H__
#define __MERO_XCODE_XCODE_H__

#include "lib/vec.h"                /* m0_bufvec_cursor */
#include "lib/types.h"              /* m0_bcount_t */
#include "lib/assert.h"             /* M0_BASSERT */
#include "xcode/xcode_attr.h"       /* M0_XC_ATTR */
#include "xcode/enum.h"

/**
   @defgroup xcode

   xcode module implements a modest set of introspection facilities. A user
   defines a structure (m0_xcode_type) which describes the in memory layout of a
   C data-type. xcode provides interfaces to iterate over hierarchy of such
   descriptors and to associate user defined state with types and fields.

   A motivating example of xcode usage is universal encoding and decoding
   interface (m0_xcode_decode(), m0_xcode_encode(), m0_xcode_length()) which
   converts between an in-memory object and its serialized representation.

   Other usages of xcoding interfaces are:

       - pretty-printing,

       - pointer swizzling and adjustment when loading BE segments into memory,

       - consistency checking: traversing data-structures in memory validating
         check-sums and invariants.

   Not every C data-structure can be represented by xcode. The set of
   representable data-structures is defined inductively according to the
   "aggregation type" of data-structure:

       - ATOM aggregation type: scalar data-types void, uint8_t, uint32_t and
         uint64_t are representable,

       - RECORD aggregation type: a struct type, whose members are all
         representable is representable,

       - UNION aggregation type: a "discriminated union" structure of the form

         @code
         struct {
                 scalar_t discriminator;
	         union {
	                 ...
	         } u;
         };
         @endcode

         where scalar_t is one of the scalar data-types mentioned above and all
         union fields are representable is representable,

       - SEQUENCE aggregation type: a "counted array" structure of the form

         @code
         struct {
                 scalar_t  nr;
		 el_t     *el;
         };
         @endcode

         where scalar_t is one of the scalar data-types mentioned above and el_t
         is representable,

       - ARRAY is a fixed size array

         @code
         struct {
		 el_t el[N];
         };
         @endcode

	 where N is a compile-time constant and el_t is representable.

       - OPAQUE aggregation type: pointer type is representable when it is used
         as the type of a field in a representable type and a special function
         (m0_xcode_field::xf_opaque()) is assigned to the field, which returns
         the representation of the type of an object the pointer points to.

         The usage of function allows representation of pointer structures where
         the actual type of the object pointed to depends on the contents of its
         parent structure.

   A representable type is described by an instance of struct m0_xcode_type,
   which describes type attributes and also contains an array
   (m0_xcode_type::xct_child[]) of "fields". A field is represented by struct
   m0_xcode_field and describes a sub-object. The field points to a
   m0_xcode_type instance, describing the type of sub-object. This way a
   hierarchy (forest of trees) of types is organized. Its leaves are atomic
   types.

   Sub-objects are located contiguously in memory, except for SEQUENCE elements
   and OPAQUE fields.

   xcode description of a data-type can be provided either by

       - manually creating an instance of m0_xcode_type structure, describing
         properties of the data-type or

       - by tagging C structure declarations with macros defined in
         xcode/xcode.h and then executing xcode/m0gccxml2xcode on the header file
         or

       - by creating a description of the desired serialized format of a
         data-type and using ff2c translator (xcode/ff2c/ff2c.c) to produce C
         files (.c and .h) containing the matching data-type definitions and
         xcode descriptors.

   The first method is suitable for memory-only structures. The last 2 methods
   are for structures designed to be transmitted over network of stored on
   persistent storage (fops, db records, &c.). m0gccxml2xcode is the preferable
   method. ff2c should only be used when m0gccxml2xcode cannot accomplish the
   task.
 */
/** @{ */

/* import */
struct m0_bufvec_cursor;
struct m0_be_seg;
struct m0_be_tx;

/* export */
struct m0_xcode;
struct m0_xcode_type;
struct m0_xcode_type_ops;
struct m0_xcode_ctx;
struct m0_xcode_obj;
struct m0_xcode_field;
struct m0_xcode_cursor;
struct m0_xcode_field_ops;

/**
   Type of aggregation for a data-type.

   A value of this enum, stored in m0_code_type::xct_aggr determines how fields
   of the type are interpreted.
 */
enum m0_xcode_aggr {
	/**
	   RECORD corresponds to C struct. Fields of RECORD type are located one
	   after another in memory.
	 */
	M0_XA_RECORD,
	/**
	   UNION corresponds to discriminated union. Its first field is referred
	   to as a "discriminator" and has an atomic type. Other fields of union
	   are tagged (m0_xcode_field::xf_tag) and the value of the
	   discriminator field determines which of the following fields is
	   actually used.

	   @note that, similarly to M0_XA_SEQUENCE, the discriminator field can
	   have M0_XAT_VOID type. In this case its tag is used instead of its
	   value (use cases are not clear).
	 */
	M0_XA_UNION,
	/**
	   SEQUENCE corresponds to counted array. Sequence always has two
	   fields: a scalar "counter" field and a field denoting the element of
	   the array.

	   @note if counter has type M0_XAT_VOID, its tag used as a
	   counter. This is used to represent fixed size arrays without an
	   explicit counter field.
	 */
	M0_XA_SEQUENCE,
	/**
	   Array is a fixed size array. It always has a single field. The number
	   of elements in the array is recorded as the tag is this field.
	*/
	M0_XA_ARRAY,
	/**
	   TYPEDEF is an alias for another type. It always has a single field.
	 */
	M0_XA_TYPEDEF,
	/**
	   OPAQUE represents a pointer.

	   A field of OPAQUE type must have m0_xcode_field::xf_opaque() function
	   pointer set to a function which determines the actual type of the
	   object pointed to.
	 */
	M0_XA_OPAQUE,
	/**
	   ATOM represents "atomic" data-types having no internal
	   structure. m0_xcode_type-s with m0_xcode_type::xct_aggr set to
	   M0_XA_ATOM have m0_xcode_type::xct_nr == 0 and no fields.

	   Atomic types are enumerated in m0_xode_atom_type.
	 */
	M0_XA_ATOM,
	M0_XA_NR
};

/**
   Human-readable names of m0_xcode_aggr values.
 */
extern const char *m0_xcode_aggr_name[M0_XA_NR];

/**
    Atomic types.

    To each value of this enumeration, except for M0_XAT_NR, a separate
    m0_xcode_type (M0_XT_VOID, M0_XT_U8, &c.).
 */
enum m0_xode_atom_type {
	M0_XAT_VOID,
	M0_XAT_U8,
	M0_XAT_U32,
	M0_XAT_U64,

	M0_XAT_NR
};

/** Human-readable names of elements of m0_xcode_atom_type */
extern const char *m0_xcode_atom_type_name[M0_XAT_NR];

enum {
	M0_XCODE_DECOR_READ,
	M0_XCODE_DECOR_MAX = 10
};

/** Field of data-type. */
struct m0_xcode_field {
	/** Field name. */
	const char                 *xf_name;
	/** Field type. */
	const struct m0_xcode_type *xf_type;
	/** Tag, associated with this field.

	    Tag is used in the following ways:

	        - if first field of a SEQUENCE type has type VOID, its tag is
                  used as a count of element in the sequence;

                - tag of the only field of ARRAY type is the number of elements
                  in the array;

		- tag of non-first field of a UNION type is used to determine
                  when the field is actually present in the object: the field is
                  present iff its tag equals the discriminator of the union.

		  The discriminator is the value of the first field of the
		  union.
	 */
	uint64_t                         xf_tag;
	/**
	   Fields with m0_xcode_type::xf_type == &M0_XT_OPAQUE are "opaque"
	   fields. An opaque field corresponds to a
	   pointer. m0_xcode_type::xf_opaque() is called by the xcode to follow
	   the pointer. This function returns (in its "out" parameter) a type of
	   the object pointed to. "par" parameter refers to the parent object to
	   which the field belongs.
	 */
	int                        (*xf_opaque)(const struct m0_xcode_obj *par,
					     const struct m0_xcode_type **out);
	/**
	   Byte offset of this field from the beginning of the object.
	 */
	uint32_t                         xf_offset;
	/**
	   "Decorations" are used by xcode users to associate additional
	   information with introspection elements.

	   @see m0_xcode_decor_register()
	   @see m0_xcode_type::xct_decor[]
	 */
	void                        *xf_decor[M0_XCODE_DECOR_MAX];
	int                        (*xf_read)(const struct m0_xcode_cursor *it,
					      struct m0_xcode_obj *obj,
					      const char *str);

};

enum m0_xcode_type_flags {
	/** Type belongs to BE xcode domain, @see M0_XCA_DOMAIN */
	M0_XCODE_TYPE_FLAG_DOM_BE     = 1 << 0,
	/** Type belongs to RPC xcode domain, @see M0_XCA_DOMAIN */
	M0_XCODE_TYPE_FLAG_DOM_RPC    = 1 << 1,
	/** Type belongs to CONF xcode domain, @see M0_XCA_DOMAIN */
	M0_XCODE_TYPE_FLAG_DOM_CONF   = 1 << 2,
};
M0_BASSERT(sizeof(enum m0_xcode_type_flags) <= sizeof(uint32_t));

/**
   This struct represents a data-type.
 */
struct m0_xcode_type {
	/** What sub-objects instances of this type have and how they are
	    organized? */
	enum m0_xcode_aggr              xct_aggr;
	/** Type name. */
	const char                     *xct_name;
	/** Custom operations. */
	const struct m0_xcode_type_ops *xct_ops;
	/**
	    Which atomic type this is?

	    This field is valid only when xt->xct_aggr == M0_XA_ATOM.
	 */
	enum m0_xode_atom_type          xct_atype;
	/**
	   Flags, associated with this type (if any).
	   For possible values @see m0_xcode_type_flags enum.
	 */
	uint32_t                        xct_flags;
	/**
	   "Decorations" are used by xcode users to associate additional
	   information with introspection elements.

	   @see m0_xcode_decor_register()
	   @see m0_xcode_field::xf_decor[]
	 */
	void                           *xct_decor[M0_XCODE_DECOR_MAX];
	/** Size in bytes of in-memory instances of this type. */
	size_t                          xct_sizeof;
	/** Number of fields. */
	size_t                          xct_nr;
	/** Array of fields. */
	struct m0_xcode_field           xct_child[0];
};

/** "Typed" xcode object. */
struct m0_xcode_obj {
	/** Object's type. */
	const struct m0_xcode_type *xo_type;
	/** Pointer to object in memory. */
	void                       *xo_ptr;
};

/**
   Custom xcoding functions.

   User provides these functions (which are all optional) to use non-standard
   xcoding.

   @see m0_xcode_decode()
 */
struct m0_xcode_type_ops {
	int (*xto_length)(struct m0_xcode_ctx *ctx, const void *obj);
	int (*xto_encode)(struct m0_xcode_ctx *ctx, const void *obj);
	int (*xto_decode)(struct m0_xcode_ctx *ctx, void *obj);
	/**
	 * Call-back invoked by m0_xcode_read() to read custom object
	 * representation.
	 *
	 * Returns number of bytes consumed from the string, or negative error
	 * code. obj->xo_ptr of suitable size is allocated by m0_xcode_read(),
	 * obj->xo_type is assigned. The call-back should fill obj->xo_ptr
	 * fields.
	 *
	 * @see string_literal().
	 */
	int (*xto_read)(const struct m0_xcode_cursor *it,
			struct m0_xcode_obj *obj, const char *str);
};

enum { M0_XCODE_DEPTH_MAX = 10 };

/**
   @name iteration

   xcode provides an iteration interface to walk through the hierarchy of types
   and fields.

   This interface consists of two functions: m0_xcode_next(), m0_xcode_skip()
   and m0_xcode_cursor data-type.

   m0_xcode_next() takes a starting type (m0_xcode_type) and walks the tree of
   its fields, their types, their fields &c., all the way down to the atomic
   types.

   m0_xcode_next() can be used to walk the tree in any "standard" order:
   preorder, inorder and postorder traversals are supported. To this end,
   m0_xcode_next() visits each tree node multiple times, setting the flag
   m0_xcode_cursor::xcu_stack[]::s_flag according to the order.
 */
/** @{ */

/**
    Traversal order.
 */
enum m0_xcode_cursor_flag {
	/** This value is never returned by m0_xcode_next(). It is set by the
	    user to indicate the beginning of iteration. */
	M0_XCODE_CURSOR_NONE,
	/** Tree element is visited for the first time. */
	M0_XCODE_CURSOR_PRE,
	/** The sub-tree, rooted at an element's field has been processed
	    fully. */
	M0_XCODE_CURSOR_IN,
	/** All fields have been processed fully, this is the last time the
	    element is visited. */
	M0_XCODE_CURSOR_POST,
	M0_XCODE_CURSOR_NR
};

/** Human-readable names of values in m0_xcode_cursor_flag */
extern const char *m0_xcode_cursor_flag_name[M0_XCODE_CURSOR_NR];

/**
    Cursor that captures the state of iteration.

    The cursor contains a stack of "frames". A frame describes the iteration at
    a particular level.
 */
struct m0_xcode_cursor {
	/** Depth of the iteration. */
	int xcu_depth;
	struct m0_xcode_cursor_frame {
		/** An object that the iteration is currently in. */
		struct m0_xcode_obj       s_obj;
		/** A field within the object that the iteration is currently
		    at. */
		int                       s_fieldno;
		/** A sequence element within the field that the iteration is
		    currently at.

		    This is valid iff ->s_obj->xo_type->xcf_aggr ==
		    M0_XA_SEQUENCE.
		 */
		uint64_t                  s_elno;
		/** Flag, indicating visiting order. */
		enum m0_xcode_cursor_flag s_flag;
		/** Datum reserved for cursor users. */
		uint64_t                  s_datum;
	} xcu_stack[M0_XCODE_DEPTH_MAX];
};

M0_INTERNAL void m0_xcode_cursor_init(struct m0_xcode_cursor *it,
				      const struct m0_xcode_obj *obj);

/**
   Iterates over tree of xcode types.

   To start the iteration, call this with the cursor where
   m0_xcode_cursor_frame::s_obj field of the 0th stack frame is set to the
   desired object and the rest of the cursor is zeroed (see
   m0_xcode_ctx_init()).

   m0_xcode_next() returns a positive value when iteration can be continued, 0
   when the iteration is complete and negative error code on error. The intended
   usage pattern is

   @code
   while ((result = m0_xcode_next(it)) > 0) {
           ... process next tree node ...
   }
   @endcode

   On each return, m0_xcode_next() sets the cursor to point to the next element
   reached in iteration. The information about the element is stored in the
   topmost element of the cursor's stack and can be extracted with
   m0_xcode_cursor_top().

   An element with N children is reached 1 + N + 1 times: once in preorder, once
   in inorder after each child is processed and once in postorder. Here N equals

       - number of fields in a RECORD object;

       - 1 or 2 in a UNION object: one for discriminator and one for an actually
         present field, if any;

       - 1 + (number of elements in array) in a SEQUENCE object. Additional 1 is
         for count field;

       - number of elements in array in an ARRAY object;

       - 0 for an ATOMIC object.

   For example, to traverse the tree in preorder, one does something like

   @code
   while ((result = m0_xcode_next(it)) > 0) {
           if (m0_xcode_cursor_top(it)->s_flag == M0_XCODE_CURSOR_PRE) {
	           ... process the element ...
           }
   }
   @endcode
 */
M0_INTERNAL int m0_xcode_next(struct m0_xcode_cursor *it);

/**
   Abandons the iteration at the current level and returns one level up.
 */
M0_INTERNAL void m0_xcode_skip(struct m0_xcode_cursor *it);

/** Returns the topmost frame in the cursor's stack. */
M0_INTERNAL struct m0_xcode_cursor_frame *
m0_xcode_cursor_top(struct m0_xcode_cursor *it);

/** Returns the field currently being processed. */
M0_INTERNAL const struct m0_xcode_field *
m0_xcode_cursor_field(const struct m0_xcode_cursor *it);

/** @} iteration. */

/**
   @name xcoding.

   Encoding-decoding (collectively xcoding) support is implemented on top of
   introspection facilities provided by the xcode module. xcoding provides 3
   operations:

       - sizing (m0_xcode_length()): returns the size of a buffer sufficient to
         hold serialized object representation;

       - encoding (m0_xcode_encode()): constructs a serialized object
         representation in a given buffer;

       - decoding (m0_xcode_decode()): constructs an in-memory object, given its
         serialized representation.

   xcoding traverses the tree of sub-objects, starting from the topmost object
   to be xcoded. For each visited object, if a method, corresponding to the
   xcoding operation (encode, decode, length) is not NULL in object's type
   m0_xcode_type_ops vector, this method is called and no further processing of
   this object is done. Otherwise, "standard xcoding" takes place.

   Standard xcoding is non-trivial only for leaves in the sub-object tree (i.e.,
   for objects of ATOM aggregation type):

       - for encoding, place object's value into the buffer, convert it to
         desired endianness and advance buffer position;

       - for decoding, extract value from the buffer, convert it, store in the
         in-memory object and advance buffer position;

       - for sizing, increment required buffer size by the size of atomic type.

   In addition, decoding allocates memory as necessary.
 */
/** @{ xcoding */

/** Endianness (http://en.wikipedia.org/wiki/Endianness) */
enum m0_xcode_endianness {
	/** Little-endian. */
	M0_XEND_LE,
	/** Big-endian. */
	M0_XEND_BE,
	M0_XEND_NR
};

/** Human-readable names of values in m0_xcode_endianness */
extern const char *m0_xcode_endianness_name[M0_XEND_NR];

/** xcoding context.

    The context contains information about attributes of xcoding operation and
    its progress.
 */
struct m0_xcode_ctx {
	/** Endianness of serialized representation. */
	enum m0_xcode_endianness xcx_end;
	/**
	    Current point in the buffer vector.

	    The cursor points to the where encoding will write the next byte and
	    from where decoding will read the next byte.

	    It should be initialised with m0_bufvec_cursor_init() prior to
	    m0_xcode_encode() or m0_xcode_decode() call. The size of the cursors
	    buffer should be not less than the size of serialised structure
	    representation.
	 */
	struct m0_bufvec_cursor  xcx_buf;
	/**
	   State of the iteration through object tree.
	 */
	struct m0_xcode_cursor   xcx_it;
	/**
	   Allocation function used by decoding to allocate the topmost object
	   and all its non-inline sub-objects (arrays and opaque sub-objects).
	 */
	void                  *(*xcx_alloc)(struct m0_xcode_cursor *, size_t);
	void                   (*xcx_free)(struct m0_xcode_cursor *ctx);
	/**
	   This function is called every time type is traversed with
	   m0_xcode_next().
	 */
	int                    (*xcx_iter)(const struct m0_xcode_cursor *it);
	/**
	   This function is called when xcode.c:ctx_walk() function called from
	   m0_xcode_encode(), m0_xcode_decode(), m0_xcode_length() ends
	   processing of given xcode context and xcode object embeded into it.
	 */
	void                  (*xcx_iter_end)(const struct m0_xcode_cursor *it);
};

/**
   Sets up the context to start xcoding of a given object.
 */
M0_INTERNAL void m0_xcode_ctx_init(struct m0_xcode_ctx *ctx,
				   const struct m0_xcode_obj *obj);

/**
   @see m0_xcode_ctx::xcx_buf
 */
M0_INTERNAL int m0_xcode_decode(struct m0_xcode_ctx *ctx);

/**
   @see m0_xcode_ctx::xcx_buf
 */
M0_INTERNAL int m0_xcode_encode(struct m0_xcode_ctx *ctx);

/** Calculates the length of serialized representation. */
M0_INTERNAL int m0_xcode_length(struct m0_xcode_ctx *ctx);
/**
 * Iterates (recurses) over fields of the given type and their types.
 *
 * "t" is invoked for each type embedded in "xt" (including "xt" itself).
 *
 * "f" is invoked for each field embedded in "xt".
 */
M0_INTERNAL void m0_xcode_type_iterate(struct m0_xcode_type *xt,
				       void (*t)(struct m0_xcode_type *,
						 void *),
				       void (*f)(struct m0_xcode_type *,
						 struct m0_xcode_field *,
						 void *), void *datum);

enum m0_xcode_what {
	M0_XCODE_ENCODE = 0,
	M0_XCODE_DECODE = 1,
};

/**
    Initializes xcode context and encodes or decodes the xcode
    object in the cursor based on @what value.
 */
M0_INTERNAL int m0_xcode_encdec(struct m0_xcode_obj *obj,
				struct m0_bufvec_cursor *cur,
				enum m0_xcode_what  what);

/** Allocates buffer and places there encoded object. */
M0_INTERNAL int m0_xcode_obj_enc_to_buf(struct m0_xcode_obj *obj,
					void **buf, m0_bcount_t *len);

/** Takes buffer with encoded object and builds original object. */
M0_INTERNAL int m0_xcode_obj_dec_from_buf(struct m0_xcode_obj *obj,
					  void *buf, m0_bcount_t len);

/** Initializes xcode context and returns the length of xcode object. */
M0_INTERNAL int m0_xcode_data_size(struct m0_xcode_ctx *ctx,
				   const struct m0_xcode_obj *obj);

M0_INTERNAL void *m0_xcode_alloc(struct m0_xcode_cursor *it, size_t nob);

/**
   True iff "xt" is an array of bytes.
 */
M0_INTERNAL bool m0_xcode_is_byte_array(const struct m0_xcode_type *xt);

/**
   Handles memory allocation during decoding.

   This function takes an xcode iteration cursor and, if necessary, allocates
   memory where the object currently being decoded will reside.

   The pointer to the allocated memory is returned in m0_xcode_obj::xo_ptr. In
   addition, this pointer is stored at the appropriate offset in the parent
   object.
 */
M0_INTERNAL ssize_t
m0_xcode_alloc_obj(struct m0_xcode_cursor *it,
		   void *(*alloc)(struct m0_xcode_cursor *, size_t));
/** @} xcoding. */

/**
 * Reads an object from a human-readable string representation.
 *
 * String has the following EBNF grammar:
 *
 *     S           ::= RECORD | UNION | SEQUENCE | ARRAY | ATOM | CUSTOM
 *     RECORD      ::= '(' [S-LIST] ')'
 *     S-LIST      ::= S | S-LIST ',' S
 *     UNION       ::= '{' TAG '|' [S] '}'
 *     SEQUENCE    ::= STRING | COUNTED
 *     ARRAY       ::= '<' [S-LIST] '>'
 *     STRING      ::= '"' CHAR* '"'
 *     COUNTED     ::= '[' COUNT ':' [S-LIST] ']'
 *     ATOM        ::= EMPTY | NUMBER
 *     TAG         ::= ATOM
 *     COUNT       ::= ATOM
 *     CUSTOM      ::= '^' CHAR* | '@' CHAR*
 *
 * Where CHAR is any non-NUL character, NUMBER is anything recognizable by
 * sscanf(3) as a number and EMPTY is the empty string. White-spaces (\n, \t,
 * \v, \r, space and comments) between tokens are ignored. Comments start with a
 * hash symbol and run to the end of line.
 *
 * Custom type representations start with a caret (^) and are recognised by
 * m0_xcode_type_ops::xto_read() call-backs.
 *
 * Custom field representations start with with '@' and are recognised by a
 * call-back stored in m0_xcode_field::xf_read()
 *
 * Examples:
 * @verbatim
 * (0, 1)
 * (0, (1, 2))
 * ()
 * {1| (1, 2)}
 * {2| 6}
 * {3|}               # a union with invalid discriminant or with a void value
 * [0]                # 0 sized array
 * [3: 6, 5, 4]
 * [: 1, 2, 3]        # fixed size sequence
 * <7, 12, 0>         # fixed size array
 * "incomprehensible" # a byte (U8) sequence with 16 elements
 * 10                 # number 10
 * 0xa                # number 10
 * 012                # number 10
 * (0, "katavothron", {42| [3: ("x"), ("y"), ("z")]}, "paradiorthosis")
 * @endverbatim
 *
 * Typedefs and opaque types require no special syntax.
 *
 * @retval 0 success
 * @retval -EPROTO syntax error
 * @retval -EINVAL garbage in string after end of representation
 * @retval -ve other error (-ENOMEM, &c.)
 *
 * Error or not, the caller should free the (partially) constructed object with
 * m0_xcode_free().
 */
M0_INTERNAL int m0_xcode_read(struct m0_xcode_obj *obj, const char *str);

/**
 * Prints an xcode object to a string.
 *
 * This function is (almost) the inverse of m0_xcode_read().
 *
 * @note No attempt at pretty-printing is done. All atomic values are output in
 * hexadecimal, etc. Its main intended use is logging and debugging. Byte arrays
 * that contain only printable ASCII values are printed in the "string literal"
 * format.
 */
M0_INTERNAL int m0_xcode_print(const struct m0_xcode_obj *obj,
			       char *str, int nr);

M0_INTERNAL void m0_xcode_free_obj(struct m0_xcode_obj *obj);
M0_INTERNAL void m0_xcode_free(struct m0_xcode_ctx *ctx);
M0_INTERNAL int m0_xcode_cmp(const struct m0_xcode_obj *o0,
			     const struct m0_xcode_obj *o1);
M0_INTERNAL int m0_xcode_dup(struct m0_xcode_ctx *dest,
			     struct m0_xcode_ctx *src);
/**
 * Returns true iff the type has all "on" and none of "off" bits in
 * m0_xcode_type::xct_flags and the same holds recursively for types of all its
 * fields.
 * Doesn't check xcode types with aggregation type specified by aggr_umask.
 */
M0_INTERNAL bool m0_xcode_type_flags(struct m0_xcode_type *xt,
				     uint32_t on, uint32_t off,
				     uint64_t aggr_umask);

/**
   Returns the address of a sub-object within an object.

   @param obj     - typed object
   @param fieldno - ordinal number of field
   @param elno    - for a SEQUENCE or ARRAY field, index of the element to
                    return the address of.

   The behaviour of this function for SEQUENCE objects depends on "elno"
   value. SEQUENCE objects have the following structure:

   @code
   struct x_seq {
           scalar_t  xs_nr;
           struct y *xs_body;
   };
   @endcode

   where xs_nr stores a number of elements in the sequence and xs_body points to
   an array of the elements.

   With fieldno == 1, m0_xcode_addr() returns

       - &xseq->xs_body when (elno == ~0ULL) and

       - &xseq->xs_body[elno] otherwise.
 */
M0_INTERNAL void *m0_xcode_addr(const struct m0_xcode_obj *obj, int fieldno,
				uint64_t elno);

/**
 * Returns the value of the given atomic field.
 */
M0_INTERNAL uint64_t m0_xcode_atom(const struct m0_xcode_obj *obj);

/**
   Helper macro to return field value cast to a given type.
 */
#define M0_XCODE_VAL(obj, fieldno, elno, __type) \
        ((__type *)m0_xcode_addr(obj, fieldno, elno))

/**
   Constructs a m0_xcode_obj instance representing a sub-object of a given
   object.

   Address of sub-object (subobj->xo_ptr) is obtained by calling
   m0_xcode_addr().

   Type of sub-object (subobj->xo_type) is usually the type stored in the parent
   object's field (m0_xcode_field::xf_type), but for opaque fields it is
   obtained by calling m0_xcode_field::xf_opaque().
 */
M0_INTERNAL int m0_xcode_subobj(struct m0_xcode_obj *subobj,
				const struct m0_xcode_obj *obj, int fieldno,
				uint64_t elno);

/**
   Returns the value of first field in a given object, assuming this field is
   atomic.

   This function is suitable to return discriminator of a UNION object or
   element count of a SEQUENCE object.

   @note when the first field has M0_XT_VOID type, the tag
   (m0_xcode_field::xf_tag) of this field is returned.
 */
M0_INTERNAL uint64_t m0_xcode_tag(const struct m0_xcode_obj *obj);

/**
 * Finds and returns (in *place) first (in m0_xcode_next() order) field of type
 * xt in the given object.
 *
 * Returns -ENOENT if no field is found.
 */
M0_INTERNAL int m0_xcode_find(struct m0_xcode_obj *obj,
			      const struct m0_xcode_type *xt, void **place);

bool m0_xcode_type_invariant(const struct m0_xcode_type *xt);

/**
 * Starts construction of a "dynamic union".
 *
 * With the help of m0_xcode_union_init(), m0_xcode_union_add() and
 * m0_xcode_union_close() a discriminated union type (M0_XA_UNION) can be
 * constructed at run-time. A use case is a situation where branches of the
 * union are defined in separate modules. With dynamic union, new branches can
 * be added without modifying central code.
 *
 * After a call to m0_xcode_union_init(), new branches are added with
 * m0_xcode_union_add(). When all branches are added, m0_xcode_union_close()
 * completes the construction of union xcode type. The result can be used as a
 * usual xcode type.
 *
 * The implementation is deliberately simplistic to avoid issues with sizeof and
 * alignment calculations. Union discriminator is always M0_XT_U64.
 *
 * @param un - xcode type to be initialised. The user has to allocate this
 *             together with at least @maxbranches m0_xcode_field instances. See
 *             conf/db.c:conx_obj for example.
 *
 * @praam name - xcode type name, assigned to un->xct_name
 * @param discriminator - the name of the first field
 * @param maxbranch - maximal number of branches that can be added
 *
 * @see m0_xcode_union_add(), m0_xcode_union_close()
 */
M0_INTERNAL void m0_xcode_union_init(struct m0_xcode_type *un, const char *name,
				     const char *discriminator,
				     size_t maxbranches);

/**
 * Finalises a "dynamic union".
 *
 * @see m0_xcode_union_init(), m0_xcode_union_close()
 */
M0_INTERNAL void m0_xcode_union_fini(struct m0_xcode_type *un);

/**
 * Adds another branch to the dynamic union.
 *
 * @see m0_xcode_union_init(), m0_xcode_union_close()
 */
M0_INTERNAL void m0_xcode_union_add (struct m0_xcode_type *un, const char *name,
				     const struct m0_xcode_type *xt,
				     uint64_t tag);
/**
 * Completes construction of dynamic union, calculates sizeof.
 *
 * @see m0_xcode_union_init(), m0_xcode_union_add()
 */
M0_INTERNAL void m0_xcode_union_close(struct m0_xcode_type *un);

extern const struct m0_xcode_type M0_XT_VOID;
extern const struct m0_xcode_type M0_XT_U8;
extern const struct m0_xcode_type M0_XT_U32;
extern const struct m0_xcode_type M0_XT_U64;

extern const struct m0_xcode_type M0_XT_OPAQUE;

/**
   Void type used by ff2c in places where C syntax requires a type name.
 */
typedef char m0_void_t[0];

/**
   Returns a previously unused "decoration number", which can be used as an
   index in m0_xcode_field::xf_decor[] and m0_xcode_type::xct_decor[] arrays.

   This number can be used to associate additional state with xcode
   introspection elements:

   @code
   // in module foo
   foo_decor_num = m0_xcode_decor_register();

   ...

   struct m0_xcode_type  *xt;
   struct m0_xcode_field *f;

   xt->xct_decor[foo_decor_num] = m0_alloc(sizeof(struct foo_type_decor));
   f->xf_decor[foo_decor_num] = m0_alloc(sizeof(struct foo_field_decor));
   @endcode
 */
M0_INTERNAL int m0_xcode_decor_register(void);

struct m0_bob_type;

/**
 * Partially initializes a branded object type from a xcode type descriptor.
 *
 * @see bob.h
 */
M0_INTERNAL void m0_xcode_bob_type_init(struct m0_bob_type *bt,
					const struct m0_xcode_type *xt,
					size_t magix_field, uint64_t magix);

M0_INTERNAL void *m0_xcode_ctx_top(const struct m0_xcode_ctx *ctx);

#define M0_XCODE_OBJ(type, ptr) (struct m0_xcode_obj){ \
	.xo_type = type,                               \
	.xo_ptr  = ptr,                                \
}

void m0_xc_u8_init(void);
void m0_xc_u16_init(void);
void m0_xc_u32_init(void);
void m0_xc_u64_init(void);
void m0_xc_void_init(void);
void m0_xc_opaque_init(void);

/** @} end of xcode group */

/* __MERO_XCODE_XCODE_H__ */
#endif

/*
 *  Local variables:
 *  c-indentation-style: "K&R"
 *  c-basic-offset: 8
 *  tab-width: 8
 *  fill-column: 80
 *  scroll-step: 1
 *  End:
 */
