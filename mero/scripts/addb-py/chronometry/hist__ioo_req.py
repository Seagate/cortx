attr={ "name": "ioo_req" }
def query(from_, to_):
    q=f"""
    SELECT (fr.time-ioo_req.time) as time, ioo_req.state, fr.state, fr.id FROM ioo_req
    JOIN ioo_req fr ON fr.id=ioo_req.id AND fr.pid=ioo_req.pid
    WHERE ioo_req.state="{from_}"
    AND fr.state="{to_}";
    """
    return q

if __name__ == '__main__':
    import sys
    sys.exit(1)
