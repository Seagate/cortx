import peewee
from addb2db import *
from playhouse.shortcuts import model_to_dict
import matplotlib.pyplot as plt

def query2dlist(model):
    out=[]
    for m in model:
        out.append(model_to_dict(m))
    return out

def draw_timeline(timeline, offset):
    for i in range(len(timeline)-1):
        color = ['red', 'green', 'blue', 'yellow', 'magenta'][i%5]
        start  = timeline[i]['time']
        end    = timeline[i+1]['time']
        label  = timeline[i]['state']

        plt.hlines(offset, start, end, colors=color, lw=5)
        plt.text(start, offset, label, rotation=90)

    end    = timeline[-1]['time']
    label  = timeline[-1]['state']
    plt.text(end, offset, label, rotation=90)

    label  = timeline[0]['label']
    plt.text(end, offset, label)


def main():
    parser = argparse.ArgumentParser(description="draws s3 request timeline")
    parser.add_argument('--s3reqs', nargs='+', type=int, required=True,
                        help="requests ids to draw")
    parser.add_argument('--db', type=str, required=False, default="m0play.db",
                        help="input database file")
    parser.add_argument('--no_clovis', action='store_true', required=False,
                        default=False, help="exclude clovis requests from timeline")
    args = parser.parse_args()

    db_init(args.db)
    db_connect()

    time_table=[]
    ref_time = []

    for s3id in args.s3reqs:
        s3_req_rel = s3_request_uid.select().where(s3_request_uid.s3_request_id == s3id).get()
        s3_req_d = query2dlist(s3_request_state.select().where(s3_request_state.s3_request_id == s3_req_rel.s3_request_id))
        l = "     {}".format(s3_req_rel.uid)
        for r in s3_req_d:
            r['label'] = l

        ref_time.append(min([t['time'] for t in s3_req_d]))
        time_table.append(s3_req_d)

        if not args.no_clovis:
            s3_to_clovis_d = query2dlist(s3_request_to_clovis.select().where(s3_request_to_clovis.s3_request_id == s3_req_rel.s3_request_id))

            for s3c in s3_to_clovis_d:
                clovis = query2dlist(clovis_req.select().where(clovis_req.id == s3c['clovis_id']))
                l = "      {}".format(s3c['clovis_id'])
                for c in clovis:
                    c['label'] = l
                time_table.append(clovis)

    db_close()

    min_ref_time = min(ref_time)
    for times in time_table:
        for time in times:
            time['time']=time['time'] - min_ref_time

    for times in time_table:
        times.sort(key=lambda time: time['time'])

    offset=1
    for times in time_table:
        draw_timeline(times, offset)
        offset=offset-1

    plt.grid(True)
    plt.show()


main()
