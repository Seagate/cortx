<center>
<title>Fast Galois Field Arithmetic Library in C/C++</title>
<h1>Fast Galois Field Arithmetic Library in C/C++</h1>
<h3>James S. Plank</h3>
<h3>plank@cs.utk.edu</h3>
<h3><a href=http://www.cs.utk.edu/~plank>http://www.cs.utk.edu/~plank</a></h3>
<p>
Technical Report UT-CS-07-593<br>
Department of Computer Science</br>
University of Tennessee<br>
<p>
The online home for this document is:<br>
<a href=http://www.cs.utk.edu/~plank/plank/papers/CS-07-593>
<i>http://www.cs.utk.edu/~plank/plank/papers/CS-07-593</i></a>
</center>
<hr>
<h1>If You Use This Code</h1>

Please send me email (<a href=mailto:plank@cs.utk.edu>plank@cs.utk.edu</a>) 
and let me know.   One of the ways in which I am evaluated is 
the impact of my work, and hard data on how many people use this code is
important.

<h2>Acknowledgements</h2>

This web site is based upon work supported by the National Science Foundation 
under Grants CNS-0615221 and CNS-0437508.   I would also like to thank Cheng
Huang, Lihao Xu, Jin Li and Mochan Shrestha for helpful discussions.

<hr>
<h1>Introduction</h1>

Galois Field arithmetic is fundamental to many applications, especially 
Reed-Solomon coding.  With a Galois Field <i>GF(2<sup>w</sup>)</i>, 
addition, subtraction, multiplication and division operations are defined
over the numbers 0, 1, ..., <i>2<sup>w</sup>-1.</i> in such a way that:
<p>
<UL>
<LI> They are closed -- if <i>a</i> and <i>b</i> are elements of the field,
     then so are <i>(a+b)</i>, <i>(a-b)</i>, <i>(a*b)</i> and <i>(a/b)</i>.
<p>
<LI> They adhere to all the well-known properties of addition/subtraction/multiplication/division.
     For example, addition and multiplication are commutative; addition, multiplication and 
     division are all associative; addition/multiplication are distributive; etc.
<p>
<LI> Every element has a multiplicative inverse.  This is the most important property.
It means that if <i>a</i> is an element of the field and <i>a &ne; 0</i>, then
there exists an element <i>b</i> that is also an element of the field such that <i>ab = 1</i>.
</UL>
<p>
When <i>w = 8</i>, the Galois Field <i>GF(2<sup>8</sup>)</i> comprises the elements
0, 1, ..., 255.  This is an important field because it allows you to perform arithmetic
that adheres to the above properties on single bytes.
Again, this is essential in Reed-Solomon coding
and other kinds of coding.  Similarly, <i>w=16</i> and <i>w=32</i> are other important
fields.
<p>
There are useful applications for fields with other values of <i>w</i>.  For example,
Cauchy Reed-Solomon coding employs Galois Fields with other values of <i>w</i>, and 
converts them to strictly binary (XOR) codes.  
<p>
Galois Fields are covered in standard texts on Error Correcting Codes such as
Peterson & Weldon <a href=#PW>[PW72]</a>, 
MacWilliams and Sloane <a href=#MS>[MS77]</a>, 
and Van Lint <a href=#VL>[VL82]</a>.  These treatments are thorough, and take
a bit of time to understand.  For a briefer and more pragmatic 
treatment, see Plank's 1997 tutorial on Reed-Solomon coding <a href=#P97>[P97]</a>, 
<a href=#PD05>[PD05]</a>.
<hr>
<h1>The Library Files</h1>

This library comes as a <b>tar</b> file: <a href=galois.tar><b>galois.tar</b></a>.
The files that compose <b>galois.tar</b> are:
<UL>
<LI> <a href=README.html><b>README.html</b></a> - This file.
<LI> <a href=galois.h><b>galois.h</b></a> - The header file for the library.
<LI> <a href=galois.c><b>galois.c</b></a> - The implementation of the library.
<LI> <a href=gf_mult.c><b>gf_mult.c</b></a> - A command line Galois Field multiplier.
<LI> <a href=gf_div.c><b>gf_div.c</b></a> - A command line Galois Field divided.
<LI> <a href=gf_xor.c><b>gf_xor.c</b></a> - A command line XOR (Galois field addition/subtraction).
<LI> <a href=gf_inverse.c><b>gf_inverse.c</b></a> - A command line Galois Field inverter.
This is the same as dividing one by a number.
<LI> <a href=gf_log.c><b>gf_log.c</b></a> - Discrete logarithm for a Galois Field.
<LI> <a href=gf_ilog.c><b>gf_ilog.c</b></a> - Discrete inverse logarithm for a Galois Field.
<LI> <a href=gf_basic_tester.c><b>gf_basic_tester.c</b></a> - A correctness and timing tester for
     Galois Field arithmetic.
<LI> <a href=gf_xor_tester.c><b>gf_xor_tester.c</b></a> - A timing tester for XOR.
<LI> <a href=makefile><b>makefile</b></a> - The makefile.
<LI> <a href=primitive-polynomial-table.txt><b>primitive-polynomial-table.txt</b></a> - 
A table of primitive polynomials.
</UL>

<hr>
<h1>Using the Command Line Tools</h1>

Type <b>make</b> to compile the library and the command line tools.  Testing the 
command line tools is a nice way to see how things work with Galois Fields.  First,
addition and subtraction in a Galois Field are equivalent -- they are equal to the
bitwise exclusive-or operation.  The program <b>gf_xor</b> allows you to take the
bitwise exclusive-or of two numbers:
<p><center><table border=3 cellpadding=3><td><pre>
Unix-Prompt> <b>gf_xor 15 7</b>
8
Unix-Prompt> <b>gf_xor 8 7</b>
15
Unix-Prompt> <b>gf_xor 230498 2738947</b>
2772833
Unix-Prompt> <b>gf_xor 2772833 230498</b>
2738947
Unix-Prompt> <b></b>
</pre></td></table></center><p>
The program <b>gf_mult</b> takes three arguments: <i>a</i>, 
<i>b</i> and <i>w</i>, and prints out the product of 
<i>a</i> and <i>b</i> in <i>GF(2<sup>w</sup>)</i>.  The
program <b>gf_div</b> performs division, and the program
<b>gf_inverse</b> returns the multiplicative inverse of
a number in <i>GF(2<sup>w</sup>)</i>:
<p><center><table border=3 cellpadding=3><td><pre>
Unix-Prompt> <b>gf_mult 3 7 4</b>
9
Unix-Prompt> <b>gf_div 9 3 4</b>
7
Unix-Prompt> <b>gf_div 9 7 4</b>
3
Unix-Prompt> <b>gf_mult 1234567 2345678 32</b>
1404360778
Unix-Prompt> <b>gf_div 1404360778 1234567 32</b>
2345678
Unix-Prompt> <b>gf_div 1404360778 2345678 32</b>
1234567
Unix-Prompt> <b>gf_inverse 1404360778 32</b>
106460795
Unix-Prompt> <b>gf_mult 1404360778 106460795 32</b>
1
Unix-Prompt> <b></b>
</pre></td></table></center><p>
The other command line tools are discussed at the end of this document.

<hr>
<h1>Using the Library</h1>

<p>
The files <a href=galois.h><b>galois.h</b></a> and
<a href=galois.c><b>galois.c</b></a> implement a library
of procedures for Galois Field Arithmetic in <i>GF(2<sup>w</sup>)</i>
for <i>w</i> between 1 and 32.  The library is written in C, but will work
in C++ as well.
It is especially tailored for <i>w</i> equal to
8, 16 and 32, but it is also applicable for any other value of <i>w</i>.  For the 
smaller values of <i>w</i> (where multiplication or logarithm tables fit into memory),
these procedures should be very fast.  
<p>
In the following sections, we describe the procedures implemented by the library.
<p>
<h3>1. General purpose multiplication and division:
  <i>galois_single_multiply()</i> and <i>galois_single_divide()</i></h3>

The syntax of these two calls is:
<p>
<center>
<table border=3><td><pre>

int galois_single_multiply(int a, int b, int w);
int galois_single_divide(int a, int b, int w);
</pre></td></table>
</center>

<p>
<b>Galois_single_multiply()</b> 
returns the product of <i>a</i> and <i>b</i> in <i>GF(2<sup>w</sup>)</i>,
and <b>galois_single_divide()</b> 
returns the qoutient of <i>a</i> and <i>b</i> in <i>GF(2<sup>w</sup>)</i>.
<i>w</i> may have any value from 1 to 32.  
<p>
The decision to make this procedure use regular, signed integers instead of
unsigned integers was largely for convenience.  It only makes a difference
when <i>w</i> equals 32, in which case the sign bit of <i>a</i>, <i>b</i>,
or the return values may be set.  If it matters, simply convert the integers
to unsigned integers.  The procedures in this library will work regardless --
when <i>w</i> equals 32, they are treated as streams of bits and not integers.
<p>
It is anticipated that most applications that need to perform single multiplications
and divisions only need reasonable performance, which is what these two procedures
give you.  If you need faster multiplication and division, then see the 
procedures below, which allow you to get much faster performance.

<h3>2. Multiplying a region of bytes by a single number in 
<i>GF(2<sup>8</sup>)</i>,
<i>GF(2<sup>16</sup>)</i> and
<i>GF(2<sup>32</sup>)</i></h3>

A common use of Galois Field arithmetic is multplying a region of bytes by a single
number.  This is the basic operation of Reed-Solomon encoding and decoding.
This library provides the following three procedures for performing region multiplication:

<p>
<center>
<table border=3><td><pre>

void galois_w08_region_multiply(char *region, int multby, int nbytes, char *r2, int add);
void galois_w16_region_multiply(char *region, int multby, int nbytes, char *r2, int add);
void galois_w32_region_multiply(char *region, int multby, int nbytes, char *r2, int add);
</pre></td></table></center><p>

These multiply the region of bytes specified by <b>region</b> and <b>nbytes</b> by the 
number <b>multby</b> in the field specified by the procedure's name.   
<b>Region</b> should be long-word aligned, otherwise these routines will generate a bus
error.  There are three
separate functionalities of these procedures denoted by the values of <b>r2</b> and <b>add</b>.

<p>
<OL>
<LI> If <b>r2</b> is <b>NULL</b>, the bytes in <b>region</b> are replaced by their products
with <b>multby</b>.
<LI> If <b>r2</b> is not <b>NULL</b> and <b>add</b> is zero, then the products are placed in
the <b>nbytes</b> of memory starting with <b>r2</b>.  The two regions should not overlap unless
<b>r2</b> is less than <b>region</b>.
<LI> If <b>r2</b> is not <b>NULL</b> and <b>add</b> is one, then the products are calculated
and then XOR'd into existing bytes of <b>r2</b>.
</OL>

<p> The performance of these procedures has been tuned to be very fast.  A multiplication
table is employed when <i>w=8</i>.  Log and inverse log tables are employed with <i>w=16</i>,
and seven multiplication tables are employed when <i>w=32</i>.  

<h3>3. XOR-ing a region of bytes</h3>

The following procedure allows you to perform the bitwise exclusive-or of a region 
of bytes.
<p>
<center>
<table border=3><td><pre>

void galois_region_xor(char *r1, char *r2, char *r3, int nbytes);
</pre></td></table></center><p>

<b>R3</b> may equal either <b>r1</b> or <b>r2</b> if you wish to overwrite either of them.
Again, all pointers should be long-word aligned.

<hr>
<h2>Advanced uses -- fast single multplications and divisions</h2>

While <b>galois_single_multiply()</b> and 
<b>galois_single_divide()</b> are nice general-purpose tools, their generality makes
them slower than they need to be.  For high-performance, you may want to employ their
underlying implementations, described below:
<p>
<h3>4. Using a multiplication table: <i>galois_multtable_multiply()</i> and <i>galois_multtable_divide()</i></h3>

When <i>w</i> is small, the fastest way to perform multiplication and division is to 
employ multiplication and division tables.  These tables consume <i>2<sup>(2w+2)</sup></i>
bytes each, so they are only applicable when <i>w</i> is reasonably small.  For example,
when <i>w=8</i>, this is 256 KB per table.  
<p>
To use multiplication and division tables directly, use one or more of the following
routines:
<p>
<p>
<center>
<table border=3><td><pre>

int galois_create_mult_tables(int w);   
int galois_multtable_multiply(int a, int b, int w);
int galois_multtable_divide(int a, int b, int w);
int *galois_get_mult_table(int w);
int *galois_get_div_table(int w);
</pre></td></table></center><p>

<b>Galois_create_mult_tables(w)</b> creates multiplication and division tables for a
given value of <b>w</b> and stores them internally.  If you call it twice with the
same value of <b>w</b>, it will not create new tables the second time.  You may call
it with different values of <b>w</b> and the tables will be stored separately.
<p>
If successful, <b>galois_create_mult_tables()</b> will return 0.  Otherwise, it will
return -1, and any allocated memory will be freed.  
<p>
<b>Galois_multtable_multiply()</b> and <b>galois_multtable_divide()</b> work just like
<b>galois_single_multiply()</b> and <b>galois_single_divide()</b>, except they assume
that you have called <b>galois_create_mult_tables()</b>  for the appropriate value of
<b>w</b> and that it was successful.  They do not error-check, so if you have not
called <b>galois_create_mult_tables()</b>, they will seg-fault.  This decision was made
for speed -- although for small values of <i>w</i> (between 1 and 9), <b>galois_single_multiply()</b> uses
<b>galois_multtable_multiply()</b>, it is significantly slower because of the error checking
that it does.
<p>
Finally, to free yourself of procedure call overhead, the routines <b>galois_get_mult_table()</b>
and <b>galois_get_div_table()</b> return the tables themselves.  The product/quotient of <i>a</i>
and <i>b</i> is in element <i>a*2<sup>w</sup>+b</i>, which of course may be computed quickly via
bit arithmetic as <b>( (a << w) | b)</b>.
You do not need to call <b>galois_create_mult_tables()</b> before calling <b>galois_get_mult_table()</b>
or <b>galois_get_div_table()</b>.
<p>

<p>
<h3>5. Using log/anti-log tables: <i>galois_logtable_multiply()</i> and <i>galois_logtable_divide()</i></h3>

When multiplication tables cannot be employed, the next fastest way to 
perform multiplication and division is to 
use log and inverse log tables, as described in <a href=#P97>[P97]</a>.
The log table consumes
<i>2<sup>(w+2)</sup></i> bytes and the inverse log table consumes
<i>3*2<sup>(w+2)</sup></i> bytes,
which means that middling values of <i>w</i> may
be handled.  For example, when <i>w=16</i>, this is 1 MB of tables.
<p>
To use the log tables, use one or more of the following routines:
<p>
<center>
<table border=3><td><pre>

int galois_create_log_tables(int w);   
int galois_logtable_multiply(int a, int b, int w);
int galois_logtable_divide(int a, int b, int w);
int galois_log(int value, int w);
int galois_ilog(int value, int w);
int *galois_get_log_table(int w);
int *galois_get_ilog_table(int w);
</pre></td></table></center><p>

<b>Galois_create_log_tables(w)</b> creates log and inverse log tables for the 
given value of <b>w</b>, and stores them internally.  If you call it twice with the
same value of <b>w</b>, it will not create new tables the second time.  You may call
it with different values of <b>w</b> and the tables will be stored separately.
<p>
If successful, <b>galois_create_log_tables()</b> will return 0.  Otherwise, it will
return -1, and any allocated memory will be freed.  
<p>
<b>Galois_logtable_multiply()</b> and <b>galois_logtable_divide()</b> work just like
<b>galois_single_multiply()</b> and <b>galois_single_divide()</b>, except they assume
that you have called <b>galois_create_log_tables()</b>  for the appropriate value of
<b>w</b> and that it was successful.  They do not error-check, so if you have not
called <b>galois_create_log_tables()</b>, they will seg-fault.  This decision was made
for speed -- although for medium values of <i>w</i> (between 10 and 22), 
<b>galois_single_multiply()</b> uses
<b>galois_logtable_multiply()</b>, it is significantly slower because of the error checking
that it does.
<p>
<b>Galois_log()</b> and 
<b>galois_ilog()</b> return the log and inverse log of an element of <i>GF(2<sup>w</sup>)</i>.
You can use them to multiply using the following identity:
<p>
<center>
    a * b = ilog[ (log[a] + log[b]) % ((1 << w)-1) ]<br>
    a / b = ilog[ (log[a] - log[b] + (1 << w)) % ((1 << w)-1) ]
</center>
<p>
The division identity takes into account C's weird definition of modular arithmetic
with negative numbers.
<p>
To perform the fastest multiplication and division with these tables, you should get access
to the tables themselves using <b>galois_get_log_table(int w)</b>
and <b>galois_get_ilog_table(int w)</b>.  Then you may calculate the product/quotient of <i>a</i> and <i>b</i>
as:
<p>
<center>
    a * b = <tt>ilog [ log[a] + log[b] ]</tt><br>
    a / b = <tt>ilog [ log[a] - log[b] ]</tt><br>
</center>
<p>
You do not have to worry about modular arithmetic because the <b>ilog</b> table contains three
copies of the inverse logs, and is defined for indices between <i>-2<sup>w</sup>+1</i> and
<i>2<sup>2w</sup>-2</i>.  This saves a few instructions.
<p>

<p>
<h3>6. Shift-multiplication and slow division:  <i>galois_shift_multiply()</i> and <i>galois_shift_divide()</i></h3>

When tables are unusable, general-purpose multiplication and division is 
implemented with the following two procedures:

<p>
<center>
<table border=3><td><pre>

int galois_shift_multiply(int a, int b, int w);
int galois_shift_divide(int a, int b, int w);
</pre></td></table></center><p>

<b>Galois_shift_multiply()</b> converts <i>b</i> into a <i>w * w</i> bit matrix
and multiplies it by the bit vector <i>a</i> to create the product vector.  You may
see a quasi-tutorial description of this technique in the paper
<a href=#P05>[P05]</a>.
It is significantly slower than the methods that use tables.  However, it is general-purpose
and requires no preallocation of memory.
<p>
For division, <b>Galois_shift_divide()</b> also converts <i>b</i> into a
bit matrix, inverts it, and then multiplies the inverse by <i>a</i>.  As such, 
it is *really* slow.  If I get a clue how to implement this one faster, I will.
<p>
<b>Galois_single_multiply()</b> uses <b>galois_shift_multiply()</b> for 
<i>w</i> between 23 and 31.
<b>Galois_single_divide()</b> uses <b>galois_shift_divide()</b> for 
<i>w</i> between 23 and 32.


<p>
<h3>7. The special case of <i>w=32</i>:  <i>galois_split_w8_multiply()</i></h3>

Finally, for <i>w = 32</i> the following procedures are defined:

<p>
<center>
<table border=3><td><pre>

int galois_create_split_w8_tables();
int galois_split_w8_multiply(int a, int b);
</pre></td></table></center><p>

<p>
<b>Galois_create_split_w8_tables()</b> creates seven tables that are 256 MB each.
<b>Galois_split_w8_multiply()</b> employs these tables to multiply the 32-bit numbers
by breaking them into four eight-bit numbers each, and then performing sixteen
multiplications and exclusive-ors to calculate the product.  It's a cool technique
suggested to me by Cheng Huang of Microsoft, and is a good 16 times faster than
using <b>galois_shift_multiply()</b>.
<p>
"But couldn't you use this technique for other values of <i>w</i>?"  Yes, you 
could, but I'm not implementing it, because I don't think it's that important.
If that view changes, I'll fix it.
<p>
<b>Galois_single_multiply()</b> uses <b>galois_split_w8_multiply()</b> for 
<i>w = 32</i>.

<hr>
<h1>Thread Safety</h1>

The only possible race conditions in these codes are when the various tables
are created.  For that reason, 
<b>galois_create_mult_tables()</b> and
<b>galois_create_log_tables()</b> should be protected by a mutex if thread
safety is a concern. 
<p>
Since <b>galois_single_multiply()</b> and <b>galois_single_divide()</b> call
the table creation routines whenever the tables do not exist, if you are
worried about thread safety, then for each value of <i>w</i> that you will 
use, you should make sure that the first call to 
<b>galois_single_multiply()</b> or <b>galois_single_divide()</b> is protected.
After that, no protection is required.

<hr>
<h1>Testing Applications</h1>

The programs <b>gf_mult</b>, <b>gf_div</b>, <b>gf_log</b>, <b>gf_ilog</b>
<b>gf_inverse</b> and <b>gf_xor</b> are straightforward and allow you to 
test the various routines for various values of <i>w</i>.  
<p>
<b>Gf_basic_tester</b> and <b>gf_xor_tester</b> test both correctness and speed.
Call <b>gf_xor_tester</b> with no arguments to test the speed of <b>gf_region_xor()</b>
on your system.  Here it is on a Macbook whose CPU is a little busy doing other things:
<pre>
Unix-Prompt> <b>gf_xor_tester</b>
XOR Tester
Seeding random number generator with 1172533188
Passed correctness test -- doing 10-second timing
1827.79986 Megabytes of XORs per second
Unix-Prompt>
</pre>
<p>
<b>Gf_basic_tester</b> takes the following command line arguments:
<p>
<UL>
<LI> <i>W</i>: 1 through 32
<LI> <i>Method</i>: This is one of the following words: <b>default</b>, 
<b>multtable</b>, 
<b>logtable</b>, 
<b>shift</b>, 
<b>splitw8</b>.  It specifies how multiplication/division will be performed.
<b>Default</b> uses <b>gf_single_multiply()</b> and 
<b>gf_single_divide()</b>.
<LI> <i>Ntrials</i>: This specifies how many random multiplies/divides to test for
correctness.
</UL>

After testing for correctness, <b>gf_basic_tester</b> tests for speed.  There are three
special cases.  When <i>method</i>=<b>default</b> and <i>w</i> is 8, 16 and 32, 
<b>gf_basic_tester</b> also tests the speed of <b>gf_w<i>xx</i>_region_multiply()</b>.
<p>
For example (again, on my MacBook):
<pre>
Unix-Prompt> <b>gf_basic_tester 16 default 100000</b>
W: 16
Method: default
Seeding random number generator with 1172533569
Doing 100000 trials for single-operation correctness.
Passed Single-Operations Correctness Tests.

Doing galois_w16_region_multiply correctness test.
Passed galois_w16_region_multiply correctness test.

Speed Test #1: 10 Seconds of Multiply operations
Speed Test #1: 42.23862 Mega Multiplies per second
Speed Test #2: 10 Seconds of Divide operations
Speed Test #2: 43.42448 Mega Divides per second

Doing 10 seconds worth of region_multiplies - Three tests:
   Test 0: Overwrite initial region
   Test 1: Products to new region
   Test 2: XOR products into second region

   Test 0: 253.45548 Megabytes of Multiplies per second
   Test 1: 238.59569 Megabytes of Multiplies per second
   Test 2: 167.99968 Megabytes of Multiplies per second
</pre>


<hr>
<h1>References</h1>
<UL>
<LI> <a NAME=MS>[MS77]</a>
F. J. MacWilliams and N. J. A. Sloane,
<u>The Theory of Error-Correcting Codes, Part I</u>,
North-Holland Publishing Company,
Amsterdam, New York, Oxford,
1977.
<p>
<LI> <a NAME=PW>[PW72]</a>
W. W. Peterson and E. J. Weldon, Jr., 
<u>Error-Correcting Codes, Second Edition</u>,
The MIT Press,
Cambridge, Massachusetts,
1972, ISBN: 0-262-16-039-0.
<p>
<LI> <a NAME=P97>[P97]</a>
J. S. Plank,
"A Tutorial on Reed-Solomon Coding for
                Fault-Tolerance in RAID-like Systems,"
<i>Software -- Practice & Experience</i>,
27(9), September, 1997, pp. 995-1012.
<a href=http://www.cs.utk.edu/~plank/plank/papers/SPE-9-97.html>
<i>http://www.cs.utk.edu/~plank/plank/papers/SPE-9-97.html</i></a>.
<p>
<LI> <a NAME=P05>[P05]</a>
J. S. Plank,
"Optimizing Cauchy Reed-Solomon Codes for Fault-Tolerant Storage Applications",
Technical Report CS-TR-05-569, University of Tennessee, November, 2005.
<a href=http://www.cs.utk.edu/~plank/plank/papers/CS-05-569.html>
<i>http://www.cs.utk.edu/~plank/plank/papers/CS-05-569.html</i></a>.
<p>
<p>
<LI> <a NAME=PD05>[PD05]</a>
J. S. Plank and Y. Ding,
"Note: Correction to the 1997 Tutorial on Reed-Solomon Coding",
<i>Software, Practice & Experience</i>, Volume 35, Issue 2, February, 2005, pp. 189-194.
<a href=http://www.cs.utk.edu/~plank/plank/papers/SPE-04.html>
<i>http://www.cs.utk.edu/~plank/plank/papers/SPE-04.html</i></a>.
<p>
<LI> <a NAME=VL>[VL82]</a>
J. H. van Lint,
<u>Introduction to Coding Theory</u>,
Springer-Verlag,
New York,
1982.
</UL>
<p>

