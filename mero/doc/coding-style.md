This document describes Mero project coding style. This document
does NOT describe code documentation practices, they are explained
elsewhere.

The first thing to remember is that a primary purpose that a coding
style exists for is to make understanding code easier for a project
participant. To this end, a code should be as uniform and idiomatic as
possible similarly with the properties that make usual human speech
easier to understand.

Mero coding style is based on the Linux kernel coding style, which
everyone is advised to familiarize with at
[Linux Kernel Coding Style](http://www.kernel.org/doc/Documentation/CodingStyle)

To summarise:

  * tabs are 8 characters;

  * block indentation is done with tabs;

  * braces placement and spacing is as following:

          if (condition) {
                  branch0;
          } else {
                  branch1;
          }

          func(arg0, arg1, ...);

          while (condition) {
                  body;
          }

          switch (expression) {
          case VALUE0:
                  branch0;
          case VALUE1:
                  branch1;
          ...
          default:
                  defaultbranch;
          }

  * no white spaces at the end of a line;

  * braces around single statement blocks can be optionally omitted;

  * comment formatting follows Linux kernel style;

  * British spelling is used in the documentation, comments and
    variable names;

  * continuation line starts one column after the last unclosed
    opening parenthesis:

          M0_ASSERT(ergo(service != NULL,
                         m0_rpc_service_invariant(service) &&
                         service->svc_state == M0_RPC_SERVICE_STATE_INITIALISED));

  * a new line should not start with an operator:

          if (pl_oldrec->pr_let_id != stl->ls_enum->le_type->let_id ||
              pl_oldrec->pr_attr.pa_N != pl->pl_attr.pa_N) {
          }

  * align identifier names, not asterisks or other type-declaration related
    decorations:

          struct foo {
                  const char        *f_name;
                  uint32_t           f_id;
                  const struct bar  *f_bar;
                  int              (*f_callback)(struct foo *f, int flag);
          };

      This rule applied to block-level variable declarations too.

In addition to the above syntactic conventions, Mero code should
try to adhere to some higher level idioms.

  * a loop repeated N times is written

          for (i = 0; i < N; ++i) {
                  body;
          }

      or, if appropriate,

          m0_forall(i, N, body);

  * an infinite loop is written as

          while (1) {
                  ...
          }

  * names of struct and union members (fields) are have a short
    (1--4 characters) prefix, derived from the union or struct tag:

          struct misc_imperium_translatio {
                  destination_t mit_rome[3]; /* there shall be no fourth Rome */
                  enum reason   mit_why;
          };

      (Rationale: his makes search for field name usage easier.)

  * typedefs are used only for "scalar" data types, including
    function pointers, but excluding enums. Compound data types
    (structs and unions) should never be aliased with typedef.

      Names introduced by typedef end with `_t` ;

  * sizeof expr is preferred to sizeof(type):

          struct foo *bar = m0_alloc(sizeof *bar);

      (Rationale: when bar's type changes code remains correct.);

  * to iterate over indices of an array X use ARRAY_SIZE(X) macro
    instead of an explicit array size:

          #define MAX_DEGREE_OF_SEPARATION (7)

          int degrees_of_separation[MAX_DEGREE_OF_SEPARATION + 1];

          for (i = 0; i < ARRAY_SIZE(degrees_of_separation); ++i) {
                  body;
          }

      (Rationale: when array's declaration changes code remains correct.);

  * in the spirit of the two examples above, always try to make the
    code as autonomous as possible, so that the code correctness
    survives changes;

  * use difference between NULL, 0 and "false" to emphasize whether
    an expression is used as a pointer, integer (including success
    or failure code) or boolean:

          if (p == NULL) { /* assumes that p is a pointer */
          } else if (q != 0) { /* q is an integer */
          } else if (r) { /* r is a boolean */
          }

      Specifically, never use if (r == true);

  * use `?:` form of ternary operator (a gcc-extension) like

          a ?: b ?: c ?: ...

      This expression means "return first non-zero value among a, b,
      c". Operands, including "a" can have any suitable type.

  * simplify:

            return q != 0;            to     return q;
            return expr ? 0 : 1;      to     return !expr;

      Specifically, never use `(x == true)` or `(x == false)` instead of
      `(x)` or `(!x)` respectively;

      (Rationale: if `(x == true)` is clearer than `(x)`, then `((x == true) == true)`
      is even more clearer.)

  * use `!!x` to convert a "boolean" integer into an "arithmetic" integer;

  * use C99 bool type;

  * naming:

      globally visible name:

          struct m0_<module>_<noun> { ... }; /* data-type */
          int    m0_<module>_<noun>[size];   /* variable */
          int    m0_<module>_<noun>_<verb>(...); /* function */
          bool   m0_<module>_<noun>_is_<adjective>(...); /* predicate function */

      Static names don't have "m0_" prefix. Function pointers within
      "operation" structs count as static. Names of constants are
      capitalized.

      Functions which are not static, not globally exported, and are shared
      only across multiple files within a module - shall be prefixed with
      `m0_<module-name>__` (that is double _). This rule applies to invariants as
      well;

  * use C99 "designated initializers":

          static const struct foo bar = { /* initialize a struct */
                  .field0 = ...,
                  .field1 = { /* initialize an array */
                          [3] = ...,
                          [0] = ...
                  },
                  ...
          };

  * avoid implicit field initialization using designated initializers;

      (Rationale: it helps to find all struct field usage and it documents
      default value of the field.);

  * use enums to define numerical constants:

          enum LSD_HASHTABLE_PARAMS {
                  LHP_PRIME   = 32416190071ULL,
                  LHP_ORDER   = 11,
                  LHP_SIZE    = 1 << LHP_ORDER,
                  LHP_MASK    = LHP_SIZE - 1,
                  LHP_FACTOR0 = 0.577215665,
                  LHP_FACTOR1,
                  LHP_FACTOR2
          };

      (Rationale: enums (as opposed to #defines) have types, visible
      in a debugger, etc.)

  * inline functions are preferable to macros

      (Rationale: type-checking, sane argument evaluation rules.);

  * not inline functions are preferable to inline functions, unless
    performance measurements show otherwise.

      (Rationale: breakpoint can be placed within a non-inline function. Stack
      trace is more reliable with non-inline functions. Instruction cache
      pollution is reduced.);

  * macros should be used only when other language constructs cannot
    achieve the required goal. When creating a macro take care to:

      - evaluate arguments only once,

      - type-check (see min_t() macro in the Linux kernel),

      - never affect control flow from a macro,

      - capitalize macro name,

      - properly parenthesize so that macro works in any context;

  * return code conventions follow linux: return 0 for success,
    negated error code for a failure, positive values for other non
    failure conditions;

  * use "const" as a documentation and help for type-checker. Do not
    use casts to trick type-checking system into believing your
    consts. A typical scenario is a function that doesn't modify its
    input struct argument except for taking and releasing a lock
    inside of the struct. Don't use "const" in this case. Instead,
    document why the argument is not technically a constant;

  * control flow statement conditions ought to have no side-effects:

          alive = qoo_is_alive(elvis);
          if (alive) { /* rather than if (qoo_is_alive(elvis)) */
                  ...
          }

      (Rationale: with this convention statement coverage metric is more adequate.);

  * use C precedence rules to omit noise in _obvious_ expressions:

          (left <= x && x < right)  /* not ((left <= x) && (x < right)) */

      but don't overdo:

          (mask << (bits & 0xf)) /* not (mask << bits & 0xf) */

  * use assertions freely to verify state invariants. An asserted
    expression should have no side-effects;

  * factor common code. Always prefer creating a common helper
    function to copying code

      (Rationale: avoids duplication of bugs.);

  * use standard scalar data type with explicit width, instead of
    "long" or "int".  E.g., int32_t, int64_t, uint32_t, uint64_t
    should be used to represent 32-bits, 64-bits integers, unsigned
    32-bits, unsigned 64-bits integers respectively

      (Rationale: avoids inconsistent data structures on different arch);

  * no comparison between signed vs. unsigned without explicit casting;

  * the canonical order of type qualifiers in declarations and
    definitions is

          {static|extern|auto} {const|volatile} typename;

  * when using long or long long qualifiers, omit int;

  * declare one variable per line;

  * avoid bit-fields. Instead, use explicit bit manipulations with
    integer types;

      (Rationale: eliminates non-atomic access to bit-fields and implicit
      integer promotion.)

  * avoid dead assignments and initializations (i.e., assignments
    which are overwritten before the variable is read)

          int x = 0;

          if (y)
                  x = foo();
          else
                  x = bar();

      Instead, initialize a variable with a meaningful value, when the
      latter is known.

      (Rationale: dead initializations potentially hide errors. If,
      after the code restructuring, the variable remains
      un-initialized in a conditional branch or in a loop that might
      execute 0 times, the initializer suppresses compiler warning.);

  * all header files should begin with '#pragma once' followed by a
    conventional '#ifndef' include guard:

          #pragma once

          #ifndef __MERO_SUBSYS_HEADER_H__
          #define __MERO_SUBSYS_HEADER_H__
          ...
          #endif /* __MERO_SUBSYS_HEADER_H__ */

      notice, that include guards should use names conforming to the
      following regular expression:

          __MERO_\w+_H__

      this is required for a build script which automatically checks
      correctness of include guards and reports duplicates;

  * specify invariants as a conjunction of positive properties one can rely
    upon, rather than as a disjunction of exceptions. Use m0_*_forall() macros
    to build conjunctions over containers and sequences;

  * in invariants use _0C() macro to record a failed conjunct;

  * a header file should include only headers, which are necessary for the
    header to pass compilation. Forward declarations should be used instead of
    includes where possible. .c files should include all necessary headers
    directly, without relaying on headers included in already included
    headers. Unneeded headers should not be included. When a header is
    included only for a few definitions (as opposed to for a whole interface
    defined in this header) these symbols should be mentioned in the comment
    on the #include line.

      (Rationale: reduces dependencies between modules, makes inclusion tree
      re-structuring easier and compilation faster.).

  * use M0_LOG() from lib/trace.h instead of printf(3)/printk() in
    all source files which are part of libmero.so library or
    m0mero.ko module (UT, ST and various helper utilities and
    modules should use printf(3)/printk()).

  * consider using M0_LOG() with some meaningful information to
    describe important error conditions; preferably it should be
    done close to the place where the error is detected:

          reply = m0_fop_alloc(&m0_reply_fopt, NULL);
          if (reply == NULL)
                  M0_LOG(M0_ERROR, "failed to allocate reply fop");

      try to describe error using current context, and not a low-level
      (which might be already logged by the other func), for example
      it would be bad to report the above error like this:

          "failed to allocate memory"

  * choose appropriate trace level for each M0_LOG(), a general
    guidelines for this can be found in lib/trace.h in
    documentation of m0_trace_level enum.

  * consider using M0_ENTRY()/M0_LEAVE() at function's entry and exit points,
    as well as M0_RC() and M0_ERR_INFO() to explicitly return from function,
    which conforms to the standard return code convention.

  * When a function is about to return a "leaf level" error (i.e., an
    error initially produced by this function, rather than returned
    from a lower level Mero function), it should wrap the error code in
    M0_ERR():

          result = M0_ERR(-EFAULT);
          ...
          return M0_RC(result);

      or

          return M0_ERR(-EIO);

      (an error, returned by a non-Mero function, is considered leaf).

      A non-leaf errors should be reported optionally, when this
      doesn't lead to artificial code complication for reporting
      sake. For example,

          result = m0_foo(bar);
          if (result != 0)
                  return M0_ERR(result);

      but usually not

          result = m0_foo(bar);
          ...
          return result == 0 ? M0_RC(0) : M0_ERR(result);

      Rationale: error reporting through M0_ERR() is important for log
      analysis. Reporting leaf errors is more important, because
      call-chain can usually be traced upward easily.

Things to look after:

  * locks should outlive the object(s) they are protecting.

      The code below illustrates a common mistake:

          struct foo {
                  ...
                  /* Protects foo object from concurrent modifications. */
                  struct m0_mutex f_lock;
          };

          int foo_init(struct foo *foo)
          {
                  m0_mutex_init(&foo->f_lock);
                  m0_mutex_lock(&foo->f_lock);
                  /* ... Initialize foo ... */
                  m0_mutex_unlock(&foo->f_lock);
          }

          void foo_fini(struct foo *foo)
          {
                  m0_mutex_lock(&foo->f_lock);
                  /* ... Finalize foo ... */
                  m0_mutex_unlock(&foo->f_lock);
                  m0_mutex_fini(&foo->f_lock);           /* <--- Thread A */
          }

          int foo_modify(struct foo *foo, ...)
          {
                  m0_mutex_lock(&foo->f_lock);
                  /* ... Modify field(s) of foo ... */   /* <--- Thread B */
                  m0_mutex_unlock(&foo->f_lock);
          }

      Here it is possible that some thread (B) tries to unlock the
      mutex, which is already finalized by another thread (A).

      A general rule of thumb is that object creation and destruction
      should be protected by "existential lock(s)", with a life-time
      longer than that of the object.

Code organization guidelines.

The following is not a substitute for design guidelines, which are defined
elsewhere.

Traditional code organization techniques, taught in universities, include
modularity, layering, information hiding, and maintaining abstraction
boundaries. They tend to produce code, which is easy to modify and re-factor,
and are, hence, very important. Their utility is highest in the projects that
experience constant frequent modifications. Such projects (or phases of
projects) cannot be long. In a long term project, where code lives around for
many years, different considerations start playing an increasing role.

Consider an example. In a project that is in a stable phase, i.e., sees
relatively infrequent addition of the new features, most typical use of source
code by a programmer is bug analysis. That is starting from a failure report (or
performance degradation, or test failure) a programmer looks through the area of
code that is most likely to be the culprit. Failing to find the problem here
(which is usually the case, because all obvious bugs are already ironed out),
the programmer proceeds through the other involved modules, recursively.

Two observations are of import here:

  * the code is mostly read, not written. The stabler the project, the more
    predominant reads are, because only harder to find bugs remain and more
    code has to be analyzed for each of them;

  * the code is read under an assumption that it is incorrect.

The last point goes contrary to the principles of information hiding and
abstraction boundaries: when a module A, which uses a module B, is analyzed
under an assumption that there is a bug in either, abstraction boundary is not
only not helpful, but directly detrimental, because every call from A to B has
to be followed anyway (cannot rely on invariants!) and the more rigorous is
abstraction, the more effort is spent jumping around the abstraction wall.

The experience with large long term projects, such as Lustre and Linux kernel,
demonstrated that after a certain threshold readability is at least as important
as modifiability. In such projects, abstraction and modularity are properties of
the software *design*, whereas the code, produced from the design, is optimised
toward the long term readability.

Some concrete consequences:

  * keep the code *visually* compact. The amount of code visible at the screen
    at once is very important, if you stare at it for hours. Blank lines are
    precious resource;

  * all kinds of redundant Hungarian notations should be eschewed. For
    example, don't put information about parameters in function name, because
    parameters are already present at a call-site. A typical call for
    m0_mod_call_with_bar() would look like m0_mod_call_with_bar(foo, bar). Not
    only "bar" is redundant, it is also ugly. Use thesaurus to deal with
    "call_with_x" vs. "call_with_y";

  * wrapping field access in an accessor function is a gratuitous abstraction,
    which should only be used sparingly, if it makes code more compact: field
    accesses have nice properties (like side-effect freedom), which are
    important for code analysis and which function wrapper hides. Besides, C
    type system doesn't allow correct handling of constness in this case,
    unless you have *two* wrappers;

  * more generally, abstractions should be introduced for design purposes,
    e.g., to mark a point of possible variability. Sub-modules,
    data-structures and operation vectors should not be created simply to
    "keep things small". Remember, that in the long term, refactoring is easy.

LocalWords:  struct enums structs sizeof summarise accessor
