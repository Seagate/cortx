import re
import math
import sys
import peewee
from addb2db import *
from typing import List, Dict
from graphviz import Digraph
from copy import deepcopy
from req_utils import *


def _graph_add_relations(graph, relations, is_meta=False):
    #           relation    |      from     |     to        | table or direct| flags
    #                       | table/mapping | table/mapping |     mapping    |
    schema = [("clovis_id"  ,  "clovis_id"  , "dix_id"      , ""             , "C1"),
              ("cas_id"     ,  "cas_id"     , "rpc_id"      , "cas_to_rpc"   , "C" ),
              ("crpc_id"    ,  "crpc_id"    , "srpc_id"     , ""             , "C1"),
              ("crpc_id"    ,  "rpc_id"     , "bulk_id"     , "bulk_to_rpc"  , "Cs"),
              ("srpc_id"    ,  "srpc_id"    , "fom_id"      , ""             , "S1"),
              ("fom_id"     ,  "fom_id"     , "tx_id"       , ""             , "S1"),
              ("tx_id"      ,  ""           , ""            , ""             , "Sl"),
              ("fom_id"     ,  "fom_id"     , "crow_fom_id" , ""             , "S1"),
              ("crow_fom_id",  "crow_fom_id", "crow_tx_id"  , ""             , "S1"),
              ("crow_tx_id" ,  ""           , ""            , ""             , "Sl")]
    # flags: '1' - one to one mapping, 's' - stash samples, 'l' - leaf element

    if is_meta:
        schema.insert(1, ("dix_id",  "dix_id", "mdix_id", "dix_to_mdix", "C"))
        schema.insert(2, ("mdix_id", "dix_id",  "cas_id",  "dix_to_cas", "C"))
    else:
        schema.insert(1, ("dix_id",  "dix_id",  "cas_id",  "dix_to_cas", "C"))

    graph_add_relations(graph, relations, schema)

def graph_draw(m_relations, relations, ext_graph: Digraph=None):
    graph = ext_graph if ext_graph is not None else Digraph(
        strict=True, format='png', node_attr = {'shape': 'plaintext'})

    if m_relations:
        _graph_add_relations(graph, m_relations, True)
    _graph_add_relations(graph, relations)

    if ext_graph is None:
        graph.render(filename='md_graph_{}'.format(relations[0]['clovis_id']))
    
    return graph

# ================================================================================
# obtain some samples

query_gen="""
SELECT

DISTINCT(cas_to_rpc.rpc_id),
clovis_to_dix.clovis_id,
clovis_to_dix.dix_id,
{}
dix_to_cas.cas_id,
rpc_to_sxid.opcode,
rpc_to_sxid.xid, rpc_to_sxid.session_id,
sxid_to_rpc.xid, sxid_to_rpc.session_id,
fom_desc.rpc_sm_id, fom_desc.fom_sm_id, fom_desc.fom_state_sm_id,
fom_to_tx.tx_id, tx_to_gr.gr_id,
crow_fom_desc.fom_sm_id, crow_fom_desc.fom_state_sm_id,
crow_fom_to_tx.tx_id,
clovis_to_dix.pid,
fom_desc.pid

FROM clovis_to_dix
{}
JOIN dix_to_cas  on {}=dix_to_cas.dix_id
JOIN cas_to_rpc  on dix_to_cas.cas_id=cas_to_rpc.cas_id
JOIN rpc_to_sxid on cas_to_rpc.rpc_id=rpc_to_sxid.id
JOIN sxid_to_rpc on rpc_to_sxid.xid=sxid_to_rpc.xid AND rpc_to_sxid.session_id=sxid_to_rpc.session_id
JOIN fom_desc    on sxid_to_rpc.id=fom_desc.rpc_sm_id
LEFT JOIN fom_to_tx   on fom_desc.fom_sm_id=fom_to_tx.fom_id
LEFT JOIN tx_to_gr    on fom_to_tx.tx_id=tx_to_gr.tx_id
LEFT JOIN cas_fom_to_crow_fom on fom_desc.fom_sm_id=cas_fom_to_crow_fom.fom_id
LEFT JOIN fom_desc crow_fom_desc on cas_fom_to_crow_fom.crow_fom_id=crow_fom_desc.fom_sm_id
LEFT JOIN fom_to_tx crow_fom_to_tx on crow_fom_desc.fom_sm_id=crow_fom_to_tx.fom_id

WHERE sxid_to_rpc.opcode in (230,231,232,233) and rpc_to_sxid.opcode in (230,231,232,233)
AND   rpc_to_sxid.xid        > 0
AND   rpc_to_sxid.session_id > 0
{}
AND   clovis_to_dix.pid=dix_to_cas.pid
AND   clovis_to_dix.pid=cas_to_rpc.pid AND clovis_to_dix.pid=rpc_to_sxid.pid

AND   sxid_to_rpc.pid=fom_desc.pid
AND   (fom_to_tx.tx_id is NULL OR fom_desc.pid=fom_to_tx.pid)
AND   (tx_to_gr.gr_id is NULL OR fom_desc.pid=tx_to_gr.pid)

AND   (crow_fom_desc.fom_sm_id is NULL OR fom_desc.pid=crow_fom_desc.pid)
AND   (crow_fom_to_tx.tx_id is NULL OR fom_desc.pid=crow_fom_to_tx.pid)
AND   (tx_to_gr.gr_id is NULL OR fom_desc.pid=tx_to_gr.pid)

AND   clovis_to_dix.clovis_id={};
"""

def update_tables_common(relations, grange: int, time_table, clovis_start_time):
    rpc_fom_s = set([(o['cas_id'],
                      o['opcode'],
                      o['cli_pid'],
                      o['srv_pid'],
                      o['crpc_id'],
                      o['srpc_id'],
                      o['fom_id'],
                      o['fom_state_id'],
                      o['tx_id'],
                      o['gr_id'],
                      o['crow_fom_id'],
                      o['crow_fom_state_id'],
                      o['crow_tx_id']) for o in relations])

    for cas_id, opcode, cli_pid, srv_pid, crpc_id, srpc_id, fom_id, fom_state_id, \
        tx_id, gr_id, crow_fom_id, crow_fom_state_id, crow_tx_id in rpc_fom_s:
        cas_req_d = query2dlist(cas_req.select().where((cas_req.id==cas_id)&
                                                       (cas_req.pid==cli_pid)))
        times_tag_append(cas_req_d, 'op', f"cas {cas_id}")
        time_table.append(cas_req_d)

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

        if crow_fom_id:
            crow_fom_req_st_d = query2dlist(fom_req_state.select().\
                                            where((fom_req_state.id==crow_fom_state_id)&
                                                  (fom_req_state.pid==srv_pid)))
            times_tag_append(crow_fom_req_st_d, 'op', f"crow-fom-state {crow_fom_state_id}")
            time_table.append(crow_fom_req_st_d)

            crow_fom_req_d = query2dlist(fom_req.select().where((fom_req.id==crow_fom_id)&
                                                                (fom_req.pid==srv_pid)))
            times_tag_append(crow_fom_req_d, 'op', f"crow-fom-phase {crow_fom_id}")
            time_table.append(crow_fom_req_d)

            if crow_tx_id:
                crow_be_tx_d = query2dlist(be_tx.select().where((be_tx.id==crow_tx_id)&
                                                                (be_tx.pid==srv_pid)))
                times_tag_append(crow_be_tx_d, 'op', f"crow_be_tx {crow_tx_id}")
                time_table.append(crow_be_tx_d)


def get_timelines(clovis_id: str, grange: int, clovis_pid: int=None, create_attr_graph: bool=False,
                  export_only: bool=False, ext_graph: Digraph=None):
    time_table = []
    queue_table = []
    queue_start_time = []
    clovis_start_time = 0
    attr_graph = None

    pid_filter = f"AND   clovis_to_dix.pid={clovis_pid}" if clovis_pid is not None else ""
    query  = query_gen.format("", "", "clovis_to_dix.dix_id", pid_filter, clovis_id)
    mquery = query_gen.format("dix_to_mdix.mdix_id,",
                              "JOIN dix_to_mdix on clovis_to_dix.dix_id=dix_to_mdix.dix_id",
                              "dix_to_mdix.mdix_id", pid_filter, clovis_id)

    with DB.atomic():
        cursor = DB.execute_sql(query)
        fields = list(cursor.fetchall())
        cursor = DB.execute_sql(mquery)
        m_fields = list(cursor.fetchall())
    labels = ("crpc_id", "clovis_id", "dix_id", "cas_id", "opcode",
              "cxid", "csess", "sxid", "ssess", "srpc_id", "fom_id",
              "fom_state_id", "tx_id", "gr_id", "crow_fom_id", "crow_fom_state_id",
              "crow_tx_id", "cli_pid", "srv_pid")
    m_labels = labels[:3] + ("mdix_id",) + labels[3:]
    relations = [dict(zip(labels, f)) for f in fields]
    m_relations = [dict(zip(m_labels, f)) for f in m_fields]

    #print(m_relations)
    #print(relations)

    clovis_id = set([o['clovis_id'] for o in relations]).pop()
    dix_id    = set([o['dix_id']    for o in relations]).pop()
    if m_relations:
        m_dix_id  = set([o['mdix_id'] for o in m_relations]).pop()

    clovis_req_d = query2dlist(
        clovis_req.select().where(clovis_req.id==clovis_id) if clovis_pid is None else
        clovis_req.select().where((clovis_req.id==clovis_id)&(clovis_req.pid==clovis_pid)))
    times_tag_append(clovis_req_d, 'op', f"clovis {clovis_id}")
    time_table.append(clovis_req_d)
    clovis_start_time=min([t['time'] for t in clovis_req_d])

    dix_req_d = query2dlist(
        dix_req.select().where(dix_req.id==dix_id) if clovis_pid is None else
        dix_req.select().where((dix_req.id==dix_id)&(dix_req.pid==clovis_pid)))
    times_tag_append(dix_req_d, 'op', f"dix {dix_id}")
    time_table.append(dix_req_d)

    if m_relations:
        m_dix_req_d = query2dlist(
            dix_req.select().where(dix_req.id==m_dix_id) if clovis_pid is None else
            dix_req.select().where((dix_req.id==m_dix_id)&(dix_req.pid==clovis_pid)))
        times_tag_append(m_dix_req_d, 'op', f"mdix {m_dix_id}")
        time_table.append(m_dix_req_d)
        update_tables_common(m_relations, grange, time_table, clovis_start_time)

    update_tables_common(relations, grange, time_table, clovis_start_time)

    # process clovis requests and subrequests
    #time_table=[t for t in time_table if t != []] ### XXX: filter non-exsitent txs!
    if not export_only:
        prepare_time_table(time_table)

    # attr
    if create_attr_graph:
        attr_graph = graph_draw(m_relations, relations, ext_graph)

    return time_table, queue_table, queue_start_time, clovis_start_time, attr_graph

def parse_args():
    parser = argparse.ArgumentParser(prog=sys.argv[0], description="""
    md_req.py: Display significant performance counters for MD Mero stack.
    """)
    parser.add_argument("-p", "--pid", type=int, default=None,
                        help="Clovis pid to get requests for")
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
        get_timelines(args.clovis_id, args.grange, args.pid, args.attr, False)

    if args.queues:
        fill_queue_table(queue_table, queue_start_time)

    db_close()

    draw(time_table, queue_table, clovis_start_time, queue_start_time,
         args.time_unit, args.queues, args.maximize)

# ================================================================================
