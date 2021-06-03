import pulsar
import sys

client = pulsar.Client('pulsar://localhost:6650')

producer = client.create_producer('test')

for i in range(int(sys.argv[1])):
    producer.send(bytes(('{"numberI": %d, "padding": "some reaaaaally very veeeeeeery long loooong loooooooong string, i mean reeaaaaaaaaaaaaaaaally big string"}' % i), 'utf-8'))

client.close()
