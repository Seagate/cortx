import pulsar

client = pulsar.Client('pulsar://localhost:6650')

msg_id = pulsar.MessageId.earliest 
reader = client.create_reader('test', msg_id)

while True:
    msg = reader.read_next()
    print("Received message '{}' id='{}'".format(msg.data(), msg.message_id()))

client.close()
