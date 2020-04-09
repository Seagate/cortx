# Failure mode to be tested for IO
# - FAILED_DEVICES
#   - IO testing is performed on failed devices, that is, dgmode IO
#     testing is performed
# - REPAIRED_DEVICES
#   - IO testing is performed on repaired devices
# - BOTH_DEVICES
#   - IO testing is performed sequentially on both failed and repaired devices
#   - This is the default mode to be used
FAILED_DEVICES="FAILED_DEVICES"
REPAIRED_DEVICES="REPAIRED_DEVICES"
BOTH_DEVICES="BOTH_DEVICES"       # default

export failure_mode=$BOTH_DEVICES

# File kind to be used during multiple 'dd' operations
# - SINGLE_FILE
#   - Same file name is used during every 'dd' operation
# - SEPARATE_FILE
#   - Separate file name is used during every 'dd' operation
# - BOTH_FILE_KINDS
#   - Both SINGLE and SEPARATE file kind testing is covered sequentially
#   - This is the default file kind to be used
SINGLE_FILE="SINGLE_FILE"
SEPARATE_FILE="SEPARATE_FILE"
BOTH_FILE_KINDS="BOTH_FILE_KINDS" # default

export file_kind=$BOTH_FILE_KINDS
#export file_kind=$SEPARATE_FILE
# Data pattern to be used while creating data files for testing
# - ABCD
#   - Each alphabet from 'a' to 'z' is written 4096 times each serially
#   - Above sequence unit is repeated until EOF
# - RANDOM1
#   - (RANDOM is not used since it is a special variable)
#   - /dev/urandom is used to create a source file with random pattern
#   - This same source file is used as input to every 'dd' command
#     execution
# - ALTERNATE
#   - dd_count is the count of 'the number of times dd has been used
#     in the ST to write to any file'
#   - If $dd_count is odd, then RANDOM pattern is used
#   - If $dd_count is even, then ABCD pattern is used
#   - This is the default data pattern to be used
ABCD="ABCD"
RANDOM1="RANDOM1"
ALTERNATE="ALTERNATE"             # default

export pattern=$ALTERNATE

# ABCD source file sizes (as set in m0t1fs_io_file_pattern.c)
NUM_ALPHABETS=26
NUM_ALPHABET_REPEATITIONS=4096
NUM_ITERATIONS=200 # required to create ~2 MB data file
ABCD_SOURCE_SIZE=$(($NUM_ALPHABETS * $NUM_ALPHABET_REPEATITIONS *
		    $NUM_ITERATIONS))

# Debug level
# - DEBUG_LEVEL_OFF
#   - equivalent to how the ST had been functioning in the past
#   - no debug data would be printed on the console
# - DEBUG_LEVEL_1
#   - linux stob is used instead of ad stob
# - DEBUG_LEVEL_2
#   - including what is covered by DEBUG_LEVEL_1
#   - whole stob contents are printed for the latest file written, after any
#     data discrepancy issue is encountered during the test
#   - this debug level is supported with the file_kind as SINGLE_FILE only
#   - this debug level is ideal for testing using automated scripts but when
#     the code is still in development
#   - this way little data is available to refer to from the ST output, in case
#     any issue is encountered
# - DEBUG_LEVEL_3
#   - including what is covered by DEBUG_LEVEL_2
#   - whole stob contents are printed for the latest file written, after each
#     dd execution and after each repair
#   - this debug level is supported with the file_kind as SINGLE_FILE only
#   - this debug level is ideal to be used with the pattern "ABCD"
#   - this debug level is ideal for testing using automated scripts and when
#     some data is required to debug some issue that is already known to exist
#   - this way, all the stob contents are available in the ST output, from the
#     regular intervals
# - DEBUG_LEVEL_INTERACTIVE
#   - including what is covered by DEBUG_LEVEL_1
#   - this mode gives an opportunity to the user to perform some manual
#     debugging at certain intervals e.g. to read some specific part of the
#     stob contents
#   - after each dd execution followed by file compare, it asks the user
#     whether to continue with the script execution and keeps waiting until
#     reply is received
#   - if user enters 'yes' then the script continues
#   - if user enters 'no', then the script unmounts m0t1fs, stops the mero
#     service and then exits
#   - this debug level is ideal for manually debugging some issue that is
#     known to exist
# - DEBUG_LEVEL_STTEST
#   - this mode does not at all help mero testing but helps to quickly verify
#     other modifications to the ST script those may be ongoing
#   - mero service is not started
#   - m0t1fs is not created/mounted and instead, data is created in 'tmp'
#     directory inside $dgmode_sandbox directory.
#   - this debug level is ideal to be used while making some modifications to
#     the ST which are to be exercised quickly by avoiding the time taken to
#     start services and to create and mount m0t1fs
DEBUG_LEVEL_OFF=0                 # default
DEBUG_LEVEL_1=1
DEBUG_LEVEL_2=2
DEBUG_LEVEL_3=3
DEBUG_LEVEL_INTERACTIVE=4
DEBUG_LEVEL_STTEST=5

export debug_level=$DEBUG_LEVEL_OFF
