attr={ "name": "s3_req" }
def query(from_, to_):
    q=f"""
    SELECT (s3r.time-s3_request_state.time) as time, s3_request_state.state, s3r.state, s3r.id as id FROM s3_request_state
    JOIN s3_request_state s3r ON s3r.id=s3_request_state.id AND s3r.pid=s3_request_state.pid
    WHERE s3_request_state.state="{from_}"
    AND s3r.state="{to_}";
    """
    return q

if __name__ == '__main__':
    import sys
    sys.exit(1)
