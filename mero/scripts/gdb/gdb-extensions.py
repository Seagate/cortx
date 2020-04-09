#
# To know more about Python APIs for gdb visit:
# http://sourceware.org/gdb/current/onlinedocs/gdb/Python-API.html#Python-API
#
# Nice series of tutorials about Writing gdb commands in Python
# http://sourceware.org/gdb/wiki/PythonGdbTutorial
#
def field_type(container, field):
	cmd = "&((({0} *)0)->{1})".format(container, field)
	tmp = gdb.parse_and_eval(cmd)
	return tmp.type.target()

def offset_of(container, field):
	macro_call = "offsetof({0}, {1})".format(container, field)
	offset = long(gdb.parse_and_eval(macro_call))
	return offset

def human_readable(count):
	k = 1024
	m = 1024 * k
	g = 1024 * m
	saved_count = count
	result = ""
	if count >= g:
		c = count // g
		result += "{0}G".format(c)
		count %= g

	if count >= m:
		c = count // m
		result += "{0}M".format(c)
		count %= m

	if count >= k:
		c = count // k
		result += "{0}K".format(c)
		count %= k

	if count != 0 or (count == 0 and result == ""):
		result += "{0}B".format(count)

	return str(saved_count) + "<" + result.strip() + ">"

def sum(start_addr, count):
	a = gdb.parse_and_eval("(unsigned char *){0:#x}".format(start_addr))
	s = 0
	for i in range(count):
		s += a[i]

	return s

#
#==============================================================================
#
class M0ListPrint(gdb.Command):
	"""Prints m0_list/m0_tl elements.

Usage: m0-list-print [&]list [[struct|union] tag link [visit|"in-detail"]]

First argument is only mandatory argument. It can be of type
- struct m0_list or struct m0_list *,
- struct m0_tl or struct m0_tl *
If this is the only argument supplied, then m0-list-print prints
address of each of links forming the list.
Example:
(gdb) m0-list-print session->s_slot_table[0]->sl_item_list
0x6257d0
0x7fffd80009d0
Total: 2

Later three arguments can be either all present or all absent.
The three arguments together specify name of link field
within ambient object. This form of m0-list-print prints pointers to
ambient objects.
Example:
(gdb) m0-list-print session->s_slot_table[0]->sl_item_list struct m0_rpc_item ri_slot_refs[0].sr_link
0x6256e0
0x7fffd80008e0
Total: 2

The last parameter is optional, if present controls how the elements
are displayed:
- If the last parameter == "in-detail", then it prints all the contents of
  ambient objects(not addreses) of the list
- Otherwise the last argument 'visit' if present is name of a user-defined command
  that takes one argument(think m0-list-print as a list traversing function
  with pointer to 'visit' function as arguemnt).
  The visit command will be executed for each object in list.
  The implementation of visit command can decide which objects to print and how.
Example:
(gdb) m0-list-print fop_types_list struct m0_fop_type ft_linkage in-detail
<Prints all m0_fop_type objects from fop_types_list>

(gdb) define session_visit
>set $s = (struct m0_rpc_session *)$arg0
>printf "session %p id %lu state %d\\n", $s, $s->s_session_id, $s->s_state
>end
(gdb) m0-list-print session->s_conn->c_sessions struct m0_rpc_session s_link session_visit
session 0x604c60 id 191837184000000002 state 4
session 0x624e50 id 0 state 4
Total: 2
(gdb) m0-list-print session->s_conn->c_sessions struct m0_rpc_session s_link
0x604c60
0x624e50
Total: 2
"""

	def __init__(self):
		gdb.Command.__init__(self, "m0-list-print",
				     gdb.COMMAND_SUPPORT, gdb.COMPLETE_SYMBOL)

	def invoke(self, arg, from_tty):
		argv = gdb.string_to_argv(arg)
		argc = len(argv)
		if argc not in (1, 4, 5):
			print 'Error: Usage: m0-list-print [&]list' \
				' [[struct|union] tag link [visit|"in-detail"]]'
			return

		vhead, head, ok = self.get_head(argv)
		if not ok:
			return
		offset, elm_type, ok = self.get_offset(argv)
		if not ok:
			return

		visit = argv[4] if argc == 5 else None
		vnd   = vhead['l_head']
		nd    = long(vnd)
		total = 0

		while nd != head:
			obj_addr = nd - offset
			if visit is None:
				print "0x%x" % obj_addr
			elif visit == "in-detail":
				cmd = "p *({0} *){1}".format(str(elm_type), obj_addr)
				gdb.execute(cmd)
			else:
				cmd = "{0} {1}".format(visit, obj_addr)
				gdb.execute(cmd)

			vnd    = vnd['ll_next']
			nd     = long(vnd)
			total += 1

		print "Total: %d" % total

	def get_head(self, argv):
		ok    = True
		head  = 0
		vhead = gdb.parse_and_eval(argv[0])
		type  = str(vhead.type)
		if type.startswith('const '):
			type = type[len('const '):]

		if type == "struct m0_list":
			head = long(vhead.address)
		elif type == "struct m0_list *":
			head = long(vhead)
		elif type in ("struct m0_tl", "struct m0_tl *"):
			vhead = vhead['t_head']
			head = long(vhead.address)
		else:
			print "Error: Invalid argument type: '%s'" % type
			ok = False
		return vhead, head, ok

	def get_offset(self, argv):
		argc     = len(argv)
		offset   = 0
		elm_type = None

		if argc in (4, 5):
			if argv[1] not in ("struct", "union"):
				print 'Error: Argument 2 must be ' + \
				      'either "struct" or "union"'
				return 0, None, False

			str_elm_type = "{0} {1}".format(argv[1], argv[2])
			anchor = argv[3]
			try:
				elm_type = gdb.lookup_type(str_elm_type)
			except:
				print "Error: type '{0}' does not exist".format(str_elm_type)
				return 0, None, False

			type = str(field_type(str_elm_type, anchor))
			if type not in ("struct m0_list_link", "struct m0_tlink"):
				print "Error: Argument 4 must be of type m0_list_link or m0_tlink"
				return 0, None, False

			if type == "struct m0_tlink":
				anchor = anchor.strip() + ".t_link"

			offset = offset_of(str_elm_type, anchor)

		return offset, elm_type, True

#
#==============================================================================
#

class M0BufvecPrint(gdb.Command):
	"""Prints segments in m0_bufvec

Usage: m0-bufvec-print [&]m0_bufvec
For each segment, the command prints,
- segment number,
- [start address, end_address),
- offset of first byte of segment, inside entire m0_bufvec, along with its
  human readable representation,
- number of bytes in segment in human readable form,
- sumation of all the bytes in the segment. sum = 0, implies all the bytes in
  the segment are zero.
"""
	def __init__(self):
		gdb.Command.__init__(self, "m0-bufvec-print", \
				     gdb.COMMAND_SUPPORT, gdb.COMPLETE_SYMBOL)

	def invoke(self, arg, from_tty):
		argv = gdb.string_to_argv(arg)
		argc = len(argv)

		if argc != 1:
			print "Error: Usage: m0-bufvec-print [&]m0_bufvec"
			return

		vbufvec = gdb.parse_and_eval(argv[0])
		t = str(vbufvec.type)
		if t != "struct m0_bufvec" and t != "struct m0_bufvec *":
			print "Error: Argument 1 must be either 'struct m0_bufvec' or" + \
					" 'struct m0_bufvec *' type"
			return

		nr_seg   = long(vbufvec['ov_vec']['v_nr'])
		buf_size = 0
		offset   = 0
		sum_of_bytes_in_buf = 0
		print "seg:index start_addr end_addr offset bcount sum"
		for i in range(nr_seg):
			start_addr = long(vbufvec['ov_buf'][i])
			count      = long(vbufvec['ov_vec']['v_count'][i])
			end_addr   = start_addr + count
			sum_of_bytes_in_seg = sum(start_addr, count)
			print "seg:{0} {1:#x} {2:#x} {3} {4} {5}".format(i, \
				start_addr, end_addr, human_readable(offset), \
				human_readable(count), sum_of_bytes_in_seg)
			buf_size += count
			offset   += count
			sum_of_bytes_in_buf += sum_of_bytes_in_seg

		print "nr_seg:", nr_seg
		print "buf_size:", human_readable(buf_size)
		print "sum:", sum_of_bytes_in_buf

#
#==============================================================================
#

class M0IndexvecPrint(gdb.Command):
	"""Prints segments in m0_indexvec

Usage: m0-indexvec-print [&]m0_indexvec
"""
	def __init__(self):
		gdb.Command.__init__(self, "m0-indexvec-print", \
				     gdb.COMMAND_SUPPORT, gdb.COMPLETE_SYMBOL)

	def invoke(self, arg, from_tty):
		argv = gdb.string_to_argv(arg)
		argc = len(argv)

		if argc != 1:
			print "Error: Usage: m0-indexvec-print [&]m0_indexvec"
			return

		v_ivec = gdb.parse_and_eval(argv[0])
		t = str(v_ivec.type)
		if t != "struct m0_indexvec" and t != "struct m0_indexvec *":
			print "Error: Argument 1 must be of either 'struct m0_indexvec' or" + \
					" 'struct m0_indexvec *' type."
			return

		nr_seg      = long(v_ivec['iv_vec']['v_nr'])
		total_count = 0

		print "   :seg_num index count"
		for i in range(nr_seg):
			index = long(v_ivec['iv_index'][i])
			count = long(v_ivec['iv_vec']['v_count'][i])
			print "seg:", i, index, count
			total_count += count

		print "nr_seg:", nr_seg
		print "total:", total_count

# List of macros to be defined
macros = [ \
"offsetof(typ,memb) ((unsigned long)((char *)&(((typ *)0)->memb)))", \
"container_of(ptr, type, member) " + \
	"((type *)((char *)(ptr)-(char *)(&((type *)0)->member)))" \
]

# Define macros listed in macros[]
for m in macros:
	gdb.execute("macro define %s" % m)

M0ListPrint()
M0BufvecPrint()
M0IndexvecPrint()

print "Loading python gdb extensions for Mero..."
#print "NOTE: If you've not already loaded, you may also want to load gdb-extensions"
