import os
import sys
import yaml
import peewee
import logging
import importlib
from addb2db import *
import matplotlib.pyplot as plt
from itertools import zip_longest as zipl

# time convertor
CONV={"us": 1000, "ms": 1000*1000}
# list of plugins
PLUG={}

def parse_args():
    parser = argparse.ArgumentParser(prog=sys.argv[0], description="""
    hist.py: creates timings histograms from performance samples.
    """)
    group0 = parser.add_mutually_exclusive_group(required=True)
    group0.add_argument("-p", "--plugin", type=str, help="plugin name")
    group0.add_argument("-l", "--list", action='store_true', help="prints plugin list")
    parser.add_argument("-v", "--verbose", action='count', default=0)
    parser.add_argument("-u", "--time-unit", choices=['ms','us'], default='us')
    parser.add_argument("-o", "--out", type=str, default="img.svg")
    parser.add_argument("-f", "--fmt", type=str, default="svg")
    parser.add_argument("-r", "--rows", type=int, default=1)
    parser.add_argument("-s", "--size", nargs=2, type=int, default=[12,4])
    parser.add_argument("range", nargs='?', help="""
    "[[from1, to1, [rend]], ... [from_n, to_n, [rend_n]]]"
    """)

    return parser.parse_args()

def query(from_, to_, range_end, plug_name, time_unit):
    q = PLUG[plug_name](from_, to_)
    logging.info(f"plug={plug_name} query={q}")

    DIV    = CONV[time_unit]
    fields = []
    with DB.atomic():
        cursor = DB.execute_sql(q)
        fields = [f[0]/DIV for f in cursor.fetchall()]

    max_f = round(max(fields), 2)
    min_f = round(min(fields), 2)
    avg_f = round(sum(fields) / len(fields), 2)
    ag_stat = f"total max/avg/min {max_f}/{avg_f}/{min_f}"

    in_range = [f for f in fields if range_end is None or f < range_end]
    plt.hist(in_range, 50)

    plt.title(f"{from_} \n {to_}")

    fs = len(fields)
    ir = len(in_range)
    stat = f"total/range/%: {fs}/{ir}/{round(ir/fs,2)}"
    logging.info(stat)
    plt.xlabel(f"time({time_unit}) \n {stat} \n {ag_stat}")
    plt.ylabel(f"frequency \n")

    plt.tight_layout()


def hist(plug, range, fmt="svg", out="img.svg", time_unit="us", rows=1, size=(12,4)):
    stages = yaml.safe_load(range)
    db_init("m0play.db")
    db_connect()

    plt.figure(figsize=size)
    nr_stages = len(stages)
    columns = nr_stages // rows + (1 if nr_stages % rows > 0 else 0)
    for nr,s in enumerate(stages, 1):
        r = dict(zipl(["from", "to", "end"], s, fillvalue=None))
        plt.subplot(rows, columns, nr)
        plt.grid(True)
        query(r["from"], r["to"], r["end"], plug, time_unit)

    db_close()
    plt.savefig(fname=out, format=fmt)

def load():
    for _,_,file in os.walk("."):
        for f in file:
            if f.endswith(".py") and f.startswith("hist__"):
                try:
                    plug = importlib.import_module(os.path.splitext(f)[0])
                    PLUG[plug.attr['name']] = plug.query
                    logging.info(f"Plugin loaded: file={f}, " \
                                 f"plugin={plug.attr['name']}")
                except:
                    logging.debug(f"File {f} is not a plugin")

if __name__ == "__main__":
    args=parse_args()

    verbosity = { 0: logging.WARN, 1: logging.INFO, 2: logging.DEBUG }
    logging.basicConfig(format='%(asctime)s - %(levelname)-8s %(message)s',
                        level=verbosity[args.verbose if args.verbose < max(verbosity)
                                        else max(verbosity)])
    load()
    if args.list:
        print("Loaded plugins:")
        for k in PLUG.keys():
            print(k)

    if args.plugin:
        hist(args.plugin, args.range, args.fmt, args.out, args.time_unit, args.rows, args.size)
