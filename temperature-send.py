import sys
import os
import time
from kafka import KafkaProducer

# USAGE:
# python3 ./temperature-send.py host:port kafka-topic device_id
# 
# Messages are sent to the kafka broker residing at host:port with topic kafka-topic
# They are being formatted as a key-value pair consisting of device_id->temperature
# 
# @author patryk.szczypien@gmail.com


def publish_message(producer_instance, topic_name, key, value):
    try:
        key_bytes = bytes(key, encoding='utf-8')
        value_bytes = bytes(value, encoding='utf-8')
        producer_instance.send(topic_name, key=key_bytes, value=value_bytes)
        producer_instance.flush()
        print('Message published successfully.')
    except Exception as ex:
        print('Exception in publishing message')
        print(str(ex))


def connect_kafka_producer(server):
    _producer = None
    try:
        _producer = KafkaProducer(bootstrap_servers=[server], api_version=(0, 10))
    except Exception as ex:
        print('Exception while connecting Kafka')
        print(str(ex))
    finally:
        return _producer


def getCPUtemperature():
    res = os.popen('vcgencmd measure_temp').readline()
    return(res.replace("temp=","").replace("'C\n",""))
if len(sys.argv) != 4:
    print ("""
USAGE:
   python3 ./temperature-send.py host:port kafka-topic device_id
""")
    sys.exit()

print (getCPUtemperature());

server = sys.argv[1]
topic = sys.argv[2]
host_key = sys.argv[3]
producer = connect_kafka_producer(server)

try:
    while True:
        publish_message(producer, topic, host_key, getCPUtemperature())
        time.sleep(5)
except KeyboardInterrupt:
        print('Manual break by user')
