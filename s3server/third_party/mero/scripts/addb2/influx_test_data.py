import time
import argparse
import datetime
from influxdb import InfluxDBClient

USER = ''
PASSWORD = ''
DBNAME = 'm0play'

class InfluxDB:
    def __init__(self, user, password, dbname, host, port, bucket_size):
        self.client = InfluxDBClient(host, port, user, password, dbname)
        self.bucket = []
        self.bucket_size = bucket_size

    def createDB(self):
        self.client.create_database(self.dbname)

    def openDB(self):
        self.client.switch_database(self.dbname)

    def closeDB(self):
        client.write_points(self.bucket)

    def writePoint(self, point):
        self.bucket.append(point)
        if len(self.bucket) < bucket_size:
            client.write_points(self.bucket)


def main(host='localhost', port=8086):
    now = datetime.datetime.today()

    point = {
        "measurement": 'meas0',
        "time": now.isoformat(timespec='milliseconds')
        "fields": {
            "id": 11100111
        },
        "tags": {
            "type": 'XXX00YYY'
        }
    }

    db = InfluxDB(USER, PASSWORD, DBNAME, host, port, 10000)
    db.createDB()
    db.openDB()
    db.writePoint(point)
    db.closeDB()


def parse_args():
    """Parse the args."""
    parser = argparse.ArgumentParser(
        description='example code to play with InfluxDB')
    parser.add_argument('--host', type=str, required=False,
                        default='localhost',
                        help='hostname influxdb http API')
    parser.add_argument('--port', type=int, required=False, default=8086,
                        help='port influxdb http API')
    return parser.parse_args()


if __name__ == '__main__':
    args = parse_args()
    main(host=args.host, port=args.port)
