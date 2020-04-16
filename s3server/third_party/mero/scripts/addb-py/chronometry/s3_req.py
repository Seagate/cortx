import io_req as ior
import md_req as mdr
from addb2db import *
from graphviz import Digraph
from req_utils import *


def graph_prepare(graph: Digraph, relations):
    #           relation  |     from    |    to       | table or direct | flags
    #                     |table/mapping|table/mapping|     mapping     |
    schema = [("s3_request_id", "s3_request_id", "clovis_id", "s3_request_to_clovis",  "C")]
    # flags: '1' - one to one mapping, 's' - stash samples, 'l' - leaf element

    graph_add_relations(graph, relations, schema)

def get_timelines_ioo(clovis_id: str, grange: int, clovis_pid: int, create_attr_graph: bool,
                      export_only: bool, ext_graph: Digraph):
    return ior.get_timelines(clovis_id, grange, clovis_pid, create_attr_graph,
                             export_only, ext_graph, False)

def get_timelines_cob(clovis_id: str, grange: int, clovis_pid: int, create_attr_graph: bool,
                      export_only: bool, ext_graph: Digraph):
    return ior.get_timelines(clovis_id, grange, clovis_pid, create_attr_graph,
                             export_only, ext_graph, True)

def get_timelines(s3reqs: List[str], pid: str, create_attr_graph: bool = False, verbose: bool = False):
    get_timelines_fns = [mdr.get_timelines, get_timelines_ioo, get_timelines_cob]
    time_table = []
    attr_graph = None

    for s3id in s3reqs:
        if "-" in s3id:
            s3_req_rel_d = s3_request_uid.select().where(s3_request_uid.uuid == s3id)
        else:
            s3_req_rel_d = s3_request_uid.select().where(s3_request_uid.id == s3id)

        if pid is not None:
            s3_req_rel_d = filter(lambda x: x.pid == pid, s3_req_rel_d)

        for s3_req_rel in s3_req_rel_d:
            s3_req_d = query2dlist(s3_request_state.select().where(
                (s3_request_state.id == s3_req_rel.id) &
                (s3_request_state.pid == s3_req_rel.pid)))
            for r in s3_req_d:
                r['op'] = s3_req_rel.uuid

            time_table.append(s3_req_d)

            s3_to_clovis_d = query2dlist(s3_request_to_clovis.select().where(
                (s3_request_to_clovis.s3_request_id == s3_req_rel.id) &
                (s3_request_to_clovis.pid == s3_req_rel.pid)))

            s3_to_clovis_d.sort(key=lambda s3rc: s3rc['clovis_id'])

            print("Processing S3 request {} (id {}, pid {}), clovis requests: "
                  .format(s3_req_rel.uuid, s3_req_rel.id, s3_req_rel.pid), end='')
            cids_str = ""
            for s3c in s3_to_clovis_d:
                cids_str += "{}, ".format(s3c['clovis_id'])
            print(cids_str[0:-2])

            if not verbose:
                sql_tmpl = """
select clovis_req.*, 'clovis[' || ifnull(cob.op, '') || ifnull(dix.op, '') || ifnull(ioo.op, '') || '] ' || clovis_req.id as 'op' from
clovis_req LEFT JOIN (select 'cob' as 'op', clovis_id, pid from clovis_to_cob) as cob ON cob.clovis_id = clovis_req.id AND cob.pid = clovis_req.pid
LEFT JOIN (select 'dix' as 'op', clovis_id, pid from clovis_to_dix) as dix ON dix.clovis_id = clovis_req.id AND dix.pid = clovis_req.pid
LEFT JOIN (select 'ioo' as 'op', clovis_id, pid from clovis_to_ioo) as ioo ON ioo.clovis_id = clovis_req.id AND ioo.pid = clovis_req.pid
WHERE clovis_req.id = {clvid} and clovis_req.pid = {clvpid};
"""
                for s3c in s3_to_clovis_d:
                    clvreq = sql_tmpl.format(clvid=s3c['clovis_id'], clvpid=s3c['pid'])
                    lbls = ["time", "pid", "id", "state", "op"]
                    clov_req_d = list(map(lambda tpl: dict(zip(lbls, tpl)),
                                          DB.execute_sql(clvreq).fetchall()))
                    time_table.append(clov_req_d)
                continue

            if create_attr_graph:
                attr_graph = Digraph(strict=True, format='png', node_attr = {'shape': 'plaintext'})
                relations = [dict(s3_request_id = s3_req_rel.id, cli_pid = s3_req_rel.pid, srv_pid = None)]
                graph_prepare(attr_graph, relations)

            for s3c in s3_to_clovis_d:
                found = False
                i = 0
                ext_tml = []

                print("Processing clovis request {} (pid {})...".format(s3c['clovis_id'], s3c['pid']))

                while not found and i < len(get_timelines_fns):
                    try:
                        ext_tml, _, _, _, _ = get_timelines_fns[i](s3c['clovis_id'], [0, 0], s3c['pid'],
                                                                   create_attr_graph, True, attr_graph)
                        found = True
                    except Exception:
                        pass
                    i = i + 1

                if found:
                    time_table += ext_tml
                    print("Done")
                else:
                    print("Failed")
                    print("Could not build timelines for clovis request {} (pid: {})".format(s3c['clovis_id'], s3c['pid']))

            if create_attr_graph:
                attr_graph.render(filename='s3_graph_{}'.format(s3_req_rel.uuid))

    return time_table

def parse_args():
    parser = argparse.ArgumentParser(description="draws s3 request timeline")
    parser.add_argument('--s3reqs', nargs='+', type=str, required=True,
                        help="requests ids to draw")
    parser.add_argument("-p", "--pid", type=int, required=False, default=None,
                        help="Clovis pid to get requests for")
    parser.add_argument('--db', type=str, required=False, default="m0play.db",
                        help="input database file")
    parser.add_argument("-a", "--attr", action='store_true', help="Create attributes graph")
    parser.add_argument("-m", "--maximize", action='store_true', help="Display in maximised window")
    parser.add_argument("-u", "--time-unit", choices=['ms','us'], default='us',
                        help="Default time unit")
    parser.add_argument("-v", "--verbose", action='store_true',
                        help="Display detailed request structure")
    parser.add_argument("-i", "--index", action='store_true',
                        help="Create indexes before processing")

    return parser.parse_args()

def create_table_index(tbl_model):
    index_query = "CREATE INDEX {} ON {} ({});"
    tbl_name = tbl_model.__name__
    tbl_fields = filter(lambda nm: (("id" in nm) or ("time" in nm)) and "__" not in nm,
                        tbl_model.__dict__.keys())
    for f in tbl_fields:
        iq = index_query.format(f"idx_{tbl_name}_{f}", tbl_name, f)
        try:
            DB.execute_sql(iq)
        except:
            pass

def create_indexes():
    for tbl in db_create_delete_tables:
        create_table_index(tbl)

if __name__ == '__main__':
    args=parse_args()

    db_init(args.db)
    db_connect()

    if args.index:
        create_indexes()

    time_table = get_timelines(args.s3reqs, args.pid, args.attr, args.verbose)
    prepare_time_table(time_table)

    db_close()

    print("Plotting timelines...")

    draw(time_table, None, 0, None, args.time_unit, False, args.maximize)
