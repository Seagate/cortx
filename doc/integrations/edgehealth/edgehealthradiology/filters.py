import arrow


def datetimeformat(date_str):
    dt = arrow.get(date_str)
    return dt.humanize()