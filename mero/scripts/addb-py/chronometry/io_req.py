import re
import math
import sys
import peewee
from addb2db import *
from typing import List, Dict
from graphviz import Digraph
from req_utils import *


def graph_draw(relations, ext_graph: Digraph=None, is_cob: bool=False):
    if is_cob:
        t = "cob"
    else:
        t = "ioo"

    graph = ext_graph if ext_graph is not None else Digraph(
        strict=True, format='png', node_attr = {'shape': 'plaintext'})

    #           relation  |     from    |    to       | table or direct | flags
    #                     |table/mapping|table/mapping|     mapping     |
    schema = [("clovis_id",  "clovis_id", f"{t}_id"   , f"clovis_to_{t}",  "C"),
              (f"{t}_id"  ,  f"{t}_id"  , "rpc_id"    , f"{t}_to_rpc"   ,  "C"),
              ("crpc_id"  ,  "crpc_id"  , "srpc_id"   , ""              , "C1"),
              ("crpc_id"  ,  "rpc_id"   , "bulk_id"   , "bulk_to_rpc"   , "Cs"),
              ("srpc_id"  ,  "srpc_id"  , "fom_id"    , ""              , "S1"),
              ("fom_id"   ,  "fom_id"   , "stio_id"   , "fom_to_stio"   ,  "S"),
              ("stio_id"  ,  ""         , ""          , ""              , "Sl"),
              ("fom_id"   ,  "fom_id"   , "tx_id"     , ""              , "S1"),
              ("tx_id"    ,  ""         , ""          , ""              , "Sl")]
    # flags: '1' - one to one mapping, 's' - stash samples, 'l' - leaf element

    graph_add_relations(graph, relations, schema)

    if ext_graph is None:
        graph.render(filename="{}_graph_{}".format(t, relations[0]['clovis_id']))

    return graph

# ================================================================================
# obtain some samples

query_template="""
SELECT

DISTINCT({io_req_type}_to_rpc.rpc_id),
clovis_to_{io_req_type}.clovis_id,
clovis_to_{io_req_type}.{io_req_type}_id,
rpc_to_sxid.opcode,
rpc_to_sxid.xid, rpc_to_sxid.session_id,
sxid_to_rpc.xid, sxid_to_rpc.session_id,
fom_desc.rpc_sm_id, fom_desc.fom_sm_id, fom_desc.fom_state_sm_id,
fom_to_tx.tx_id,
fom_to_stio.stio_id,
tx_to_gr.gr_id,
clovis_to_{io_req_type}.pid,
fom_desc.pid

FROM clovis_to_{io_req_type}
JOIN {io_req_type}_to_rpc  on clovis_to_{io_req_type}.{io_req_type}_id={io_req_type}_to_rpc.{io_req_type}_id
JOIN rpc_to_sxid on rpc_to_sxid.id={io_req_type}_to_rpc.rpc_id
JOIN sxid_to_rpc on rpc_to_sxid.xid=sxid_to_rpc.xid AND rpc_to_sxid.session_id=sxid_to_rpc.session_id
JOIN fom_desc    on sxid_to_rpc.id=fom_desc.rpc_sm_id
LEFT JOIN fom_to_tx   on fom_desc.fom_sm_id=fom_to_tx.fom_id
LEFT JOIN fom_to_stio on fom_desc.fom_sm_id=fom_to_stio.fom_id
LEFT JOIN tx_to_gr    on fom_to_tx.tx_id=tx_to_gr.tx_id

WHERE sxid_to_rpc.opcode in {opcodes} and rpc_to_sxid.opcode in {opcodes}
AND   rpc_to_sxid.xid        > 0
AND   rpc_to_sxid.session_id > 0
{pid_filter}
AND   clovis_to_{io_req_type}.pid={io_req_type}_to_rpc.pid AND clovis_to_{io_req_type}.pid=rpc_to_sxid.pid

AND   sxid_to_rpc.pid=fom_desc.pid
AND   (fom_to_stio.stio_id is NULL OR fom_desc.pid=fom_to_stio.pid)
AND   (fom_to_tx.tx_id is NULL OR fom_desc.pid=fom_to_tx.pid)
AND   (tx_to_gr.gr_id is NULL OR fom_desc.pid=tx_to_gr.pid)

AND   clovis_to_{io_req_type}.clovis_id={clovis_id};
"""

def get_timelines(clovis_id: str, grange: int, clovis_pid: int=None, create_attr_graph: bool=False,
                  export_only: bool=False, ext_graph: Digraph=None, is_cob: bool=False):
    time_table = []
    queue_table = []
    queue_start_time = []
    clovis_start_time = 0
    attr_graph = None

    if is_cob:
        io_req_type = "cob"
        opcodes = "(45,46,47,128,130)"
        io_req_table = cob_req
    else:
        io_req_type = "ioo"
        opcodes = "(41,42,45,46,47)"
        io_req_table = ioo_req

    pid_filter = f"AND   clovis_to_{io_req_type}.pid={clovis_pid}" if clovis_pid is not None else ""
    query = query_template.format(io_req_type=io_req_type, opcodes=opcodes,
                                  clovis_id=clovis_id, pid_filter=pid_filter)

    with DB.atomic():
        cursor = DB.execute_sql(query)
        fields = list(cursor.fetchall())
    labels = ("crpc_id", "clovis_id", f"{io_req_type}_id", "opcode", "cxid",
              "csess", "sxid", "ssess", "srpc_id", "fom_id",
              "fom_state_id", "tx_id", "stio_id", "gr_id",
              "cli_pid", "srv_pid")
    relations = [dict(zip(labels, f)) for f in fields]

    #print(relations)

    clovis_id = set([o['clovis_id'] for o in relations]).pop()
    io_req_id = set([o[f'{io_req_type}_id'] for o in relations]).pop()
    rpc_fom_s = set([(o['opcode'],
                      o['cli_pid'],
                      o['srv_pid'],
                      o['crpc_id'],
                      o['srpc_id'],
                      o['fom_id'],
                      o['fom_state_id'],
                      o['tx_id'],
                      o['stio_id'],
                      o['gr_id']) for o in relations])
    # 1
    clovis_req_d = query2dlist(
        clovis_req.select().where(clovis_req.id==clovis_id) if clovis_pid is None else
        clovis_req.select().where((clovis_req.id==clovis_id)&(clovis_req.pid==clovis_pid)))
    times_tag_append(clovis_req_d, 'op', f"clovis {clovis_id}")
    time_table.append(clovis_req_d)
    clovis_start_time=min([t['time'] for t in clovis_req_d])

    #2
    io_req_d = query2dlist(
        io_req_table.select().where(io_req_table.id==io_req_id) if clovis_pid is None else
        io_req_table.select().where((io_req_table.id==io_req_id)&(io_req_table.pid==clovis_pid)))
    times_tag_append(io_req_d, 'op', f"{io_req_type} {io_req_id}")
    time_table.append(io_req_d)

    #3, 4, 5, 6
    for opcode, cli_pid, srv_pid, crpc_id, srpc_id, fom_id, fom_state_id, tx_id, stio_id, gr_id in rpc_fom_s:
        for tag,(rpc_id,pid) in {"c": (crpc_id, cli_pid),
                                 "s": (srpc_id, srv_pid)}.items():
            rpc_req_d = query2dlist(rpc_req.select().where((rpc_req.id==rpc_id)&
                                                           (rpc_req.pid==pid)))
            times_tag_append(rpc_req_d, 'op', f"{tag}rpc[{opcode}] {rpc_id}")
            time_table.append(rpc_req_d)

        fom_req_st_d = query2dlist(fom_req_state.select().where((fom_req_state.id==fom_state_id)&
                                                                (fom_req_state.pid==srv_pid)))
        times_tag_append(fom_req_st_d, 'op', f"fom-state {fom_state_id}")
        time_table.append(fom_req_st_d)

        fom_req_d = query2dlist(fom_req.select().where((fom_req.id==fom_id)&
                                                       (fom_req.pid==srv_pid)))
        times_tag_append(fom_req_d, 'op', f"fom-phase {fom_id}")
        time_table.append(fom_req_d)

        if stio_id:
            stio_d = query2dlist(stio_req.select().where((stio_req.id==stio_id)&
                                                         (stio_req.pid==srv_pid)))
            times_tag_append(stio_d, 'op', f"stob-io {stio_id}")
            time_table.append(stio_d)

        if tx_id:
            be_tx_d = query2dlist(be_tx.select().where((be_tx.id==tx_id)&
                                                       (be_tx.pid==srv_pid)))
            times_tag_append(be_tx_d, 'op', f"be_tx {tx_id}")
            time_table.append(be_tx_d)

            left=[]
            right=[]
            if grange[0]>0:
                left  = query2dlist(fom_req.select().where((fom_req.id==gr_id)&
                                                           (fom_req.pid==srv_pid)&
                                                           (fom_req.time<clovis_start_time)).limit(grange[0]))
            if grange[1]>0:
                right = query2dlist(fom_req.select().where((fom_req.id==gr_id)&
                                                           (fom_req.pid==srv_pid)&
                                                           (fom_req.time>=clovis_start_time)).limit(grange[1]))
            if grange[0]>0 or grange[1]>0:
                gr_d = [*left, *right]
                times_tag_append(gr_d, 'op', f"tx-gr-phase {gr_id}")
                time_table.append(gr_d)

    # process clovis requests and subrequests
    # time_table=[t for t in time_table if t != []] ### XXX: filter non-exsitent txs!

    if not export_only:
        prepare_time_table(time_table)

    # attr
    if create_attr_graph:
        attr_graph = graph_draw(relations, ext_graph, is_cob)

    return time_table, queue_table, queue_start_time, clovis_start_time, attr_graph


def parse_args():
    parser = argparse.ArgumentParser(prog=sys.argv[0], description="""
    io_req.py: Display significant performance counters for Mero stack.
    """)
    parser.add_argument("-p", "--pid", type=int, default=None,
                        help="Clovis pid to get requests for")
    parser.add_argument("-c", "--cob", action='store_true',
                        help="Whether clovis request relates to COB")
    parser.add_argument("-m", "--maximize", action='store_true', help="Display in maximised window")
    parser.add_argument("-q", "--queues", action='store_true', help="Display queues also")
    parser.add_argument("-r", "--qrange", type=int, default=10,
                        help="Limit quantity of queue-related samples")
    parser.add_argument("-g", "--grange", default=[0, 0],
                        nargs=2, metavar=('left', 'right'), type=int,
                        help="Limit quantity of tx group-related samples")
    parser.add_argument("-a", "--attr", action='store_true', help="Create attributes graph")
    parser.add_argument("-v", "--verbose", action='count', default=0)
    parser.add_argument("-u", "--time-unit", choices=['ms','us'], default='us',
                        help="Default time unit")
    parser.add_argument("-d", "--db", type=str, default="m0play.db",
                        help="Performance database (m0play.db)")
    parser.add_argument("clovis_id", type=str, help="Clovis request id")

    return parser.parse_args()

if __name__ == '__main__':
    args=parse_args()

    db_init(args.db)
    db_connect()

    time_table, queue_table, queue_start_time, clovis_start_time, _ = \
        get_timelines(args.clovis_id, args.grange, args.pid, args.attr, False, None, args.cob)

    if args.queues:
        fill_queue_table(queue_table, queue_start_time)

    db_close()

    draw(time_table, queue_table, clovis_start_time, queue_start_time,
         args.time_unit, args.queues, args.maximize)

# ================================================================================
