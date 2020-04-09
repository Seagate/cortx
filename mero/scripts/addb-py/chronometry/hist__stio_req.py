attr={ "name": "stio_req" }
def query(from_, to_):
    q=f"""
    SELECT (fr.time-stio_req.time) as time, stio_req.state, fr.state, fr.id FROM stio_req
    JOIN stio_req fr ON fr.id=stio_req.id AND fr.pid=stio_req.pid
    WHERE stio_req.state="{from_}"
    AND fr.state="{to_}";
    """
    return q

if __name__ == '__main__':
    import sys
    sys.exit(1)
