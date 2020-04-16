attr={ "name": "srpc_to_crpc" }

def query(from_, to_):
    q=f"""
    SELECT (crq.time-srq.time), crq.state, srq.state

    FROM rpc_to_sxid
    JOIN sxid_to_rpc on rpc_to_sxid.xid=sxid_to_rpc.xid AND rpc_to_sxid.session_id=sxid_to_rpc.session_id
    JOIN rpc_req crq on rpc_to_sxid.id=crq.id AND rpc_to_sxid.pid=crq.pid
    JOIN rpc_req srq on sxid_to_rpc.id=srq.id AND sxid_to_rpc.pid=srq.pid

    WHERE sxid_to_rpc.opcode in (41,42,45,46,47) and rpc_to_sxid.opcode in (41,42,45,46,47)
    AND   rpc_to_sxid.xid        > 0
    AND   rpc_to_sxid.session_id > 0
    AND   crq.state="{to_}"
    AND   srq.state="{from_}";
    """
    return q

if __name__ == '__main__':
    import sys
    sys.exit(1)
