attr={ "name": "s3_req" }
def query(from_, to_):
    q=f"""
    SELECT (s3r.time-s3_request_state.time) as time, s3_request_state.state, s3r.state, s3r.s3_request_id as id FROM s3_request_state
    JOIN s3_request_state s3r ON s3r.s3_request_id=s3_request_state.s3_request_id
    WHERE s3_request_state.state="{from_}"
    AND s3r.state="{to_}";
    """
    return q

if __name__ == '__main__':
    import sys
    sys.exit(1)
