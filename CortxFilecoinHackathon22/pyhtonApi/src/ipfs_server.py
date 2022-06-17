import ipfshttpclient


client = ipfshttpclient.connect()

res = client.add('/README.md')

print(res)
client.close()
