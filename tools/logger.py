#!/usr/bin/env python3

import pika
import time
import sys
import json

DFLT_LOG_DIR  = "/var/log/sdr/"
DFLT_LOG_PATH = "{}sdr.log".format(DFLT_LOG_DIR)
log_path      = DFLT_LOG_PATH

if len(sys.argv) > 1:
    if sys.argv[1][0] == '/':
        log_path = sys.argv[1]
    else:
        log_path = DFLT_LOG_DIR + sys.argv[1]


def callback(ch, method, properties, body):
    try:
        f = open(log_path, 'a')

        if properties.content_type == 'application/json':
            # do things
            msg = json.loads(body)
            if 'timestamp' in msg.keys() and 'message' in msg.keys():
                f.write("{:.6f} : {}\n".format(msg['timestamp'], msg['message']))
        else:
            f.write("{:.6f} : {}\n".format(time.time(), body))
        
        f.close()

    except IOError as e:
        print("Failed to write to log file {}: {}".format(log_path, e.strerror) )


def main():
    print("Starting Performance Analyzer...")
    connection = pika.BlockingConnection(pika.ConnectionParameters(host='localhost'))
    
    channel = connection.channel()

    channel.exchange_declare(exchange='logs', exchange_type='fanout')

    channel.queue_declare(queue='event_logs') # delete when connection closes

    channel.basic_consume(queue='event_logs', auto_ack=False, on_message_callback=callback)

    channel.start_consuming()



if __name__ == '__main__':
    main()

