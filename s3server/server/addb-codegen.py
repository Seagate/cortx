#!/usr/bin/python3.6

#
# This script generates source code files needed to support ADDB logging.
#
# Usage:
#   cd s3server.git/server/
#   ./addb-codegen.py
#
# Script scans through s3server code, identifies all Action classes, all
# Actions' task and generates ADDB enums and initialization code to register
# these classes in ADDB classes map. Additional mapping table is created for
# all Actions' tasks.
#
# The main goal of this code generation is to define two things:
#
# 1) Every action class that sends ADDB logs, has to have unique
# addb_action_type_id, so this codegen defines an ID for every such action
# class.
#
# 2) We need to be able to get addb_action_type_id at run time for a given
# instance to use that ID in ADDB log entry.  This needs a lot of mundane
# repetitive code, so this codegen creates that code.

import re
import glob

###########################################################################
# load action classes #
#######################

def load_action_classes():
  # Loads all "leaf" Action classes, i.e. classes which do not have further
  # descendants.  Base classes do not need ADDB ID, as they do not implement
  # any specific actions.  In all cases, a leaf class is always instantiated,
  # and implements code for certain API call, so we only need IDs for leaf
  # classes.

  # step 1: load all classes, both base and derived, with inheritance info
  parents,headers = load_classes()

  # step 2: Scan through and find only leaf classes of a base class Action:
  leaf_classes = find_leaves(parents, "Action")
  leaf_classes.sort()

  # return leaf classes, and their respective header files.
  return ( leaf_classes, { cls: headers[cls] for cls in leaf_classes } )

def load_classes():
  # Loads all classes defined in server/*.h files.  In the end we only need
  # Action classes, and, luckily, all of them are defined with a very simple
  # pattern:
  #
  #   class SomeActionClass : public SomeParentClass {
  #
  # So we'll search for classes that match this pattern.
  #
  # Examples of leaf classes:
  #
  # S3PutObjectAction:
  #   class S3PutObjectAction : public S3ObjectAction {
  #   class S3ObjectAction : public S3Action {
  #   class S3Action : public Action {
  #
  # S3PutBucketAction:
  #   class S3PutBucketAction : public S3Action {
  #   class S3Action : public Action {
  #
  # S3GetBucketAction:
  #   class S3GetBucketAction : public S3BucketAction {
  #   class S3BucketAction : public S3Action {
  #   class S3Action : public Action {
  #
  # MeroDeleteObjectAction:
  #   class MeroDeleteObjectAction : public MeroAction {
  #   class MeroAction : public Action {

  # We'll return two dictionaries:
  #   parents[ActionClassName] = ParentClassName
  #   headers[ActionClassName] = header_file_name
  parents={}
  headers={}
  rex = re.compile(r'^class\s+(\w+)\s+:\s+public\s+(\w+)\b')
  for header in glob.glob("*.h"): # assumes we're in server/ folder
    try:
      with open(header, encoding='utf8') as handle:
        for line in handle:
          match = rex.match(line)
          if match:
            cls=match.group(1)
            parent=match.group(2)
            parents[cls] = parent
            headers[cls] = header
    except:
      print("\nCannot process file {}\n".format(header))
      raise
  return (parents, headers)

def find_leaves(parents, parent_name):
  # Given the mapping of classes to their parents ("parents") argument, and the
  # name of the root of classes hierarchy, this function will build
  # (recursively) a list of leaf classes, i.e. classes which are derived from
  # the base class (possibly through some other transitory classes), and have
  # no descendants of their own.

  # step 1: for given parent name, find all children:
  children = [ child for child,parent in parents.items() if parent == parent_name ]

  if len(children) == 0: # no children found, so it is a leaf class
    return [parent_name]

  # got some 'child' classes, scan through them recursively:
  leaves = []
  for cls in children:
    leaves += find_leaves(parents, cls)

  return leaves

###########################################################################
# generate enums #
##################

def generate_enums(classes):
  # Create a mapping of class name to respective addb_action_type_id constant.
  return { cls: camel_to_c_upcase(cls) for cls in classes }

def camel_to_c_upcase(identifier):
  # Generate addb_action_type_id constant name for a given class name.
  # Find words:
  camel_case = identifier
  c_upcase = 'S3_ADDB'
  while len(camel_case) > 0:
    allcaps_word = re.match(r'([A-Z0-9]+)[A-Z]', camel_case)
    regular_word = re.match(r'[A-Z][a-z0-9_]+', camel_case)
    if allcaps_word:
      word = allcaps_word.group(1)
    elif regular_word:
      word = regular_word.group(0)
    else:
      print(f'Failed to convert class name {identifier} from camel-case to c-case; stuck here: {camel_case}\n')
      raise AssertionError
    c_upcase = c_upcase + '_' + word
    camel_case = camel_case[len(word):]
  return c_upcase.upper() + '_ID';

rex_task_add = re.compile(".*ACTION_TASK_ADD\((.*),.*\);")
rex_task_add_obj = re.compile(".*ACTION_TASK_ADD_OBJPTR\(.*,\s*(.*),.*\);")

def extract_task_name(task_line):
  mm = rex_task_add.match(task_line)
  if mm:
    return mm.group(1)

  mm = rex_task_add_obj.match(task_line)
  if mm:
    return mm.group(1)

  print("NOT MATCHED {}\n".format(task_line))
  assert(mm)
  return None

def find_task_names():
  cc_list = glob.glob("*.cc") + glob.glob("../ut/*.cc")
  func_names = []
  for cc_file in cc_list:
    try:
      task_line = ""
      in_proc_line = False
      with open(cc_file, encoding='utf8') as cur_file:
        for cur_line in cur_file:
          if in_proc_line:
            task_line += cur_line.strip()
          else:
            in_proc_line = "ACTION_TASK_ADD" in cur_line
            task_line = cur_line.strip()
          if in_proc_line and task_line.endswith(";"):
            extr_task_name = extract_task_name(task_line)
            if extr_task_name is not None:
              func_names.append(extr_task_name)
            task_line = ""
            in_proc_line = False
    except:
      print("\nCannot process file {}\n".format(cc_file))
      raise

  return func_names

###########################################################################
# Seagate copyright #
#####################

copyright=r"""
/* AUTO-GENERATED!  DO NOT EDIT, CHANGES WILL BE LOST! */

/* generated by server/addb-codegen.py */

/*
 * COPYRIGHT 2019 SEAGATE LLC
 *
 * THIS DRAWING/DOCUMENT, ITS SPECIFICATIONS, AND THE DATA CONTAINED
 * HEREIN, ARE THE EXCLUSIVE PROPERTY OF SEAGATE TECHNOLOGY
 * LIMITED, ISSUED IN STRICT CONFIDENCE AND SHALL NOT, WITHOUT
 * THE PRIOR WRITTEN PERMISSION OF SEAGATE TECHNOLOGY LIMITED,
 * BE REPRODUCED, COPIED, OR DISCLOSED TO A THIRD PARTY, OR
 * USED FOR ANY PURPOSE WHATSOEVER, OR STORED IN A RETRIEVAL SYSTEM
 * EXCEPT AS ALLOWED BY THE TERMS OF SEAGATE LICENSES AND AGREEMENTS.
 *
 * YOU SHOULD HAVE RECEIVED A COPY OF SEAGATE'S LICENSE ALONG WITH
 * THIS RELEASE. IF NOT PLEASE CONTACT A SEAGATE REPRESENTATIVE
 * http://www.seagate.com/contact
 *
 * Original author:  Ivan Tishchenko   <ivan.tishchenko@seagate.com>
 * Original creation date: 28-Oct-2019
 */
"""

###########################################################################
# generate header file #
########################

def generate_header_file(out_file_name, classes, enums):
  out = open(out_file_name, "w")
  out.write(copyright)
  out.write(r"""
#pragma once

#ifndef __S3_SERVER_ADDB_PLUGIN_AUTO_H__
#define __S3_SERVER_ADDB_PLUGIN_AUTO_H__

#include <addb2/addb2_internal.h>

/* See comments in addb-codegen.py and s3_addb.h for more details.
 *
 * This enum defines addb_action_type_id for ADDB logs.  These action types are
 * needed to distinguish different kinds of log entries from each other.
 */
enum S3AddbActionTypeId {

  /* See comment on ext_id_ranges.h above; had to use magic number here,
   * as mero headers are not yet merged to master. */
  S3_ADDB_RANGE_START = M0_ADDB2__EXT_RANGE_1,

  /* helper IDs e.g. for linking requests */

  /* ID of ADDB entry which keeps a mapping of addb_request_id to S3 API-level
   * UUID of the request. */
  S3_ADDB_REQUEST_ID = S3_ADDB_RANGE_START,

  /* ID of ADDB entry which keeps a link between s3 request and clovis op
   * request. */
  S3_ADDB_REQUEST_TO_CLOVIS_ID,

  /* Action classes identifiers: */
  S3_ADDB_FIRST_REQUEST_ID,

  /* S3_ADDB_ACTION_BASE_ID is to be used in constructors, while action class
   * is still being constructed and it is not possible to identify what is the
   * class name of the instance (derived class name). */
  S3_ADDB_ACTION_BASE_ID = S3_ADDB_FIRST_REQUEST_ID,

  /* Auto-generated IDs are listed below. Sorry for strange format, it had to
   * be done this way to pass git-clang-format check. */

""")

  for subclass in classes:
    out.write(f"  /* {subclass}: */\n  {enums[subclass]},\n")

  out.write(f"""
  /* End of auto-generated IDs. */
  S3_ADDB_LAST_REQUEST_ID = {enums[subclass]},

  /* End of S3 server range. */
  S3_ADDB_RANGE_END = S3_ADDB_LAST_REQUEST_ID
""")

  out.write("};\n\n#endif\n")


###########################################################################
# generate s3 impl.cc file #
############################

def generate_s3_cc_file(out_file_name, classes, enums, headers):
  out = open(out_file_name, "w")
  out.write(copyright)
  out.write(r"""
#include <typeinfo>
#include <typeindex>
#include <unordered_map>

#include "s3_addb.h"

// Main goal of the functions below is to support run-time identification of
// S3AddbActionTypeId for a given Action class.  This is done using C++ RTTI.
// We're going to store a map of type_index(typeid(class)) to an ID.  Init
// function initializes that map, lookup function searches through it.

// Include all action classes' headers:
""")

  for fname in sorted(set(headers.values())):
    out.write(f'#include "{fname}"\n')

  out.write(r"""
static std::unordered_map<std::type_index, enum S3AddbActionTypeId> gs_addb_map;

int s3_addb_init() {
  s3_log(S3_LOG_DEBUG, "", "Entering\n");
  // Sorry for the format, this had to be done this way to pass
  // git-clang-format check.
""")

  for cls in classes:
    out.write(f"  gs_addb_map[std::type_index(typeid({cls}))] =\n      {enums[cls]};\n")

  for cls in classes:
    out.write(f'''
  s3_log(S3_LOG_DEBUG, "",
         "  * id 0x%" PRIx64 "/%" PRId64  // suppress clang warning
         ": class {cls}\\n",
         (uint64_t){enums[cls]},
         (int64_t){enums[cls]});
''')

  out.write(r"""  return 0;
}

enum S3AddbActionTypeId Action::lookup_addb_action_type_id(
    const Action& instance) {
  enum S3AddbActionTypeId action_type_id =
      gs_addb_map[std::type_index(typeid(instance))];
  return (action_type_id != 0 ? action_type_id : S3_ADDB_ACTION_BASE_ID);
}
""")


def gen_task_name_map(task_names_list):
  task_list = list(set(task_names_list))
  task_list.sort()
  assert(len(task_list) > 0)
  with open("s3_addb_map_auto.c", "w") as out_c:
    out_c.write(copyright)
    up_lines = r'''
#include "s3_addb_map.h"

const uint64_t g_s3_to_addb_idx_func_name_map_size = '''

    up_lines += "{};\n".format(len(task_list))

    map_decl = r'''
const char* g_s3_to_addb_idx_func_name_map[] = {
'''
    tasks = ""
    for t in task_list[:-1]:
      tasks += '    "{}",\n'.format(t)
    tasks += '    "{}"'.format(task_list[-1])
    tasks += r'''};'''
    out_c.write("{}{}{}\n".format(up_lines, map_decl, tasks))

###########################################################################
# main() #
##########

def main():
  # Make sure we're in the right folder.  This script must be run from within
  # server/ folder.
  if len(glob.glob('addb-codegen.py')) != 1:
    print("ERROR (FATAL): Script must be run from server/ folder!")
    return 1
  classes, headers = load_action_classes()
  enums = generate_enums(classes)
  generate_header_file("s3_addb_plugin_auto.h", classes, enums)
  generate_s3_cc_file("s3_addb_plugin_auto.cc", classes, enums, headers)
  gen_task_name_map(find_task_names())

if __name__ == "__main__":
  main()
