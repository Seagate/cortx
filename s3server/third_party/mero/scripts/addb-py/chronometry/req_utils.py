import math
import numpy as np
import matplotlib.pyplot as plt
from addb2db import *
from playhouse.shortcuts import model_to_dict
from typing import List, Dict
from graphviz import Digraph

# time convertor
CONV={"us": 1000, "ms": 1000*1000}

def draw_timeline(timeline, offset):
    for i in range(len(timeline)-1):
        color = ['red', 'green', 'blue', 'yellow', 'cyan'][i%5]
        start  = timeline[i]['time']
        end    = timeline[i+1]['time']
        label  = timeline[i]['state']

        plt.hlines(offset, start, end, colors=color, lw=5)
        plt.text(start, offset, label, rotation=90)

    start  = timeline[0]['time']
    end    = timeline[-1]['time']
    label  = timeline[-1]['state']
    plt.text(end, offset, label, rotation=90)

    center = (end-start)/2
    label  = timeline[0]['op'] + ": " + str(round((end-start)/1000000,3)) + "ms"
    plt.text(start+center, offset+0.01, label)

def prepare_time_table(time_table):
    ref_time = -1

    for times in time_table:
        times.sort(key=lambda time: time['time'])

        if ref_time == -1:
            ref_time = times[0]['time']
        else:
            ref_time = min(times[0]['time'], ref_time)

    for times in time_table:
        for time in times:
            time['time']=time['time']-ref_time

    return ref_time

def query2dlist(model):
    out=[]
    for m in model:
        out.append(model_to_dict(m))
    return out

def times_tag_append(times: List[Dict], tag_name: str, tag_value: str):
    for t in times:
        t[tag_name] = tag_value
queues_tag_append=times_tag_append

def draw_queue_line(queue, offset):
    mx=max([q['max'] for q in queue])
    mx=0.0000001 if mx == 0 else mx #XXX

    qnr  =[q['nr']           for q in queue]
    xdata=[q['time']         for q in queue]
    ydata=[q['avg']          for q in queue]
    _min =[q['min']          for q in queue]
    _max =[q['max']          for q in queue]
    dev  =[math.sqrt(q['dev']) for q in queue]
    avi  =[q['avg']-math.sqrt(q['dev']) for q in queue]
    ava  =[q['avg']+math.sqrt(q['dev']) for q in queue]

    plt.plot(xdata, [offset+y/mx for y in ydata], 'or')
    plt.plot(xdata, [offset+y/mx for y in ydata], '-', color='gray')
    plt.fill_between(xdata,
                     [offset+y/mx for y in avi],
                     [offset+y/mx for y in ava], color='gray', alpha=0.2)
    plt.fill_between(xdata,
                     [offset+y/mx for y in _min],
                     [offset+y/mx for y in _max], color='green', alpha=0.2)

    for x,y,nr,i,a,d in zip(xdata, ydata, qnr, _min, _max, dev ):
        plt.text(x,offset+y/mx, f"{round(y,2)} |{round(a,2)}|")

def draw(time_table, queue_table, clovis_start_time, queue_start_time,
         time_unit: str, show_queues: bool, maximize: bool):
    cursor={"x0":0, "y0":0, "x1": 0, "y1": 0, "on": False}
    undo=[]
    def onpress(event):
        if event.key == 'a':
            cursor.update({ "on": True })
        elif event.key == 'd':
            if undo:
                for an in undo.pop():
                    an.remove()
        event.canvas.draw()

    def onrelease(event):
        if not cursor["on"]:
            return
        cursor.update({ "x1": event.xdata, "y1": event.ydata })
        cursor.update({ "on": False })

        an1=event.inaxes.axvspan(cursor["x0"], cursor["x1"], facecolor='0.9', alpha=.5)
        an2=event.inaxes.annotate('', xy=(cursor["x0"], cursor["y0"]),
                                  xytext=(cursor["x1"], cursor["y0"]),
                                  xycoords='data', textcoords='data',
                                  arrowprops={'arrowstyle': '|-|'})
        an3=event.inaxes.annotate(str(round((cursor["x1"]-cursor["x0"])/CONV[time_unit],2))+f" {time_unit}",
                                  xy=(min(cursor["x1"], cursor["x0"])+
                                      abs(cursor["x1"]-cursor["x0"])/2, 0.5+cursor["y0"]),
                                  ha='center', va='center')
        undo.append([an1, an2, an3])
        event.canvas.draw()

    def onclick(event):
        if not cursor["on"]:
            return
        cursor.update({ "x0": event.xdata, "y0": event.ydata })

    fig = plt.figure()
    fig.canvas.mpl_connect('key_press_event', onpress)
    fig.canvas.mpl_connect('button_press_event', onclick)
    fig.canvas.mpl_connect('button_release_event', onrelease)
# ============================================================================

    plt.subplot({True:211, False:111}[show_queues])
    offset=0.0
    for times in time_table:
        draw_timeline(times, offset)
        offset=offset-2

    plt.yticks(np.arange(0.0, offset, -2), [t[0]['op'] for t in time_table])
    plt.grid(True)

    #queues
    if show_queues:
        for qt,subp,qstt in zip(queue_table, [223,224], queue_start_time):
            plt.subplot(subp)
            offset=0.0
            for queue in qt:
                draw_queue_line(queue, offset)
                offset=offset-2

            plt.axvline(x=(clovis_start_time-qstt))
            plt.yticks(np.arange(0.0, offset, -2), [t[0]['op'] for t in qt])
            plt.grid(True)

    #show
    if maximize:
        mng = plt.get_current_fig_manager()
        mng.window.maximize()
    plt.show()

def fill_queue_table(queue_table, queue_start_time, qrange: int):
    for q_conf in [["runq", "wail", "fom-active"],
                   ["stob-ioq-inflight", "stob-ioq-queued"]]:
        queue_table.append([])
        loc_nr = queues.select(fn.MAX(queues.locality)).where(queues.type==q_conf[0]).scalar()
        for nr in range(loc_nr+1):
            for queuei in q_conf:
                left  = queues.select().where((queues.min>-1)
                                              &(queues.locality==nr)
                                              &(queues.type==queuei)
                                              &(queues.time<clovis_start_time)).limit(qrange)
                right = queues.select().where((queues.min>-1)
                                              &(queues.locality==nr)
                                              &(queues.type==queuei)
                                              &(queues.time>=clovis_start_time)).limit(qrange)
                stmt = [*left, *right]
                #print(stmt.sql())
                q_d = query2dlist(stmt)
                _min = queues.select(fn.MIN(queues.min)).where((queues.min>-1)
                                                               &(queues.locality==nr)
                                                               &(queues.type==queuei)).scalar()
                _max = queues.select(fn.MAX(queues.max)).where((queues.min>-1)
                                                               &(queues.locality==nr)
                                                               &(queues.type==queuei)).scalar()
                queues_tag_append(q_d, 'op', f"{queuei}#{nr}[{_min}..{_max}]")
                queue_table[-1].append(q_d)

        queue_start_time.append(prepare_time_table(queue_table[-1]))


# =============== GRAPH-RELATED STUFF ===============

def graph_node_add(g: Digraph, name: str, header: str, attrs: Dict):
    node_template="""<
<TABLE BORDER="0" CELLBORDER="1" CELLSPACING="0" CELLPADDING="4">
  <TR>
    <TD>{}</TD>
  </TR>
  <TR>
    <TD>{}</TD>
  </TR>
</TABLE>>
"""
    label = "<BR/>".join([f"{k}={v}" for (k,v) in attrs.items()])
    g.node(name, node_template.format(header, label))

def graph_node_add_attr(g: Digraph, lid: int, rel: str, pid: int):
    c = lambda attr: re.sub("M0_AVI_.*_ATTR_", "", attr)
    attrd = query2dlist(attr.select().where( (attr.entity_id==lid)
                                            &(attr.pid==pid)))
    attrs = dict([(c(a['name']), a['val']) for a in attrd])
    graph_node_add(g, f"{lid}", f"{rel}({lid})", attrs)

def graph_add_relations(graph: Digraph, relations, schema):
    def pid_get(relations, flags: str):
        pidt=next(filter(lambda f: f in "SC", flags))
        return { 'S' : relations[0]['srv_pid'],
                 'C' : relations[0]['cli_pid'] }[pidt]
    '''
    Schema example:
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
    '''

    stash=[]
    for rel,fr,to,map,flags in schema:
        pid=pid_get(relations, flags)
        layer_ids=set([r[rel] for r in relations if r[rel] is not None])
        for lid in layer_ids:
            graph_node_add_attr(graph, lid, rel, pid)

            if "l" in flags:
                continue
            if "1" in flags:
                tid = next(r[to] for r in relations if r[fr]==lid)
                if tid:
                    graph.edge(f"{lid}", f"{tid}")
                continue

            cursor = DB.execute_sql(f"SELECT {to} FROM {map} WHERE {fr}={lid} and pid={pid}")
            for (tid,) in cursor:
                graph.edge(f"{lid}", f"{tid}")
                if "s" in flags:
                    stash.append((tid,to,pid))

    for lid,rel,pid in stash:
        graph_node_add_attr(graph, lid, rel, pid)
