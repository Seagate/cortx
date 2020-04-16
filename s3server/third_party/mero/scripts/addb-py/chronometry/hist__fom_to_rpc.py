attr={ "name": "fom_to_rpc" }

def query(from_, to_):
    q=f"""
    SELECT (rpc_req.time-fom_req.time), fom_req.state, rpc_req.state
    FROM fom_desc
    JOIN fom_req on fom_req.id=fom_sm_id
    JOIN rpc_req on rpc_req.id=rpc_sm_id
    WHERE fom_desc.req_opcode LIKE "%M0_IOSERVICE_%"
    AND rpc_req.pid=fom_req.pid
    AND rpc_req.state="{to_}" AND fom_req.state="{from_}";
    """
    return q

if __name__ == '__main__':
    import sys
    sys.exit(1)
