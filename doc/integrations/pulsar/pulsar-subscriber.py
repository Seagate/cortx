import pulsar

client = pulsar.Client('pulsar://localhost:6650')

consumer = client.subscribe('test', 'my-subscription1')


while True:
  msg = consumer.receive()
  try:
    print("Received message {} {}", msg.data(), msg.message_id())
    # process the message
    consumer.acknowledge(msg)
  except KeyboardInterrupt:
    print('Hello user you have pressed ctrl-c button.')

    break
  except:
    consumer.negative_acknowledge(msg)


client.close()
