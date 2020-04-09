#!/usr/bin/env python

import fileinput
import record
import getopt
import sys

def filter(argv):
    tr = record.trace(height = 10, width = 1000, loc_nr = 1, duration = 1,
                      step = 1)
    rec = ""
    fname = ""
    f = None
    for line in fileinput.input([]):
        params = line[1:].split()
        if line[0] == "*":
            if rec != "":
                name = "out." + node + "." + pid + "." + time
                if name != fname:
                    if f != None:
                        f.close()
                    f = open(name, "w+")
                    fname = name
                f.write(rec)
                rec = ""
            time = params[0][0:19]
            keep = record.keep(params[1])
        elif params[0] == "node":
            node = params[1]
        elif params[0] == "pid":
            pid = params[1]
        if keep:
            rec += line
    f.close()

if __name__ == "__main__":
    filter(sys.argv)

        
