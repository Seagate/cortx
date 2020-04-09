attr={ "name": "fom_req_r" }

def query(from_, to_):
    q=f"""
    SELECT (fr.time-fom_req.time) as time, fom_req.state, fr.state, fr.id FROM fom_desc
    JOIN fom_req on fom_desc.fom_sm_id=fom_req.id and fom_desc.pid=fom_req.pid
    JOIN fom_req fr ON fr.id=fom_req.id and fr.pid=fom_req.pid

    WHERE fom_desc.req_opcode LIKE '%READV%'
    AND fom_req.state="{from_}"
    AND fr.state="{to_}";
    """
    return q

if __name__ == '__main__':
    import sys
    sys.exit(1)
