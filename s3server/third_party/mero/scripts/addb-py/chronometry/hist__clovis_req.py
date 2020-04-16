attr={ "name": "clovis_req" }
def query(from_, to_):
    q=f"""
    SELECT (fr.time-clovis_req.time) as time, clovis_req.state, fr.state, fr.id FROM clovis_req
    JOIN clovis_req fr ON fr.id=clovis_req.id AND fr.pid=clovis_req.pid
    WHERE clovis_req.state="{from_}"
    AND fr.state="{to_}";
    """
    return q

if __name__ == '__main__':
    import sys
    sys.exit(1)
