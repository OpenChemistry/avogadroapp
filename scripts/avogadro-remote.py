#!/usr/bin/python

import sys
import json
import time
import socket
import struct

class Connection:
  def __init__(self, name = "avogadro"):
    # create socket
    self.sock = socket.socket(socket.AF_UNIX,
                              socket.SOCK_STREAM)

    # connect
    self.sock.connect("/tmp/" + name)

  def send_json(self, obj):
    self.send_message(json.dumps(obj))

  def send_message(self, msg):
    sz = len(msg)
    hdr = struct.pack('>I', sz)
    pkt = hdr + msg
    self.sock.send(pkt)

  def recv_message(self, size = 1024):
    pkt = self.sock.recv(size)

    return pkt[4:]

  def recv_json(self):
    msg = self.recv_message()

    try:
      return json.loads(msg)
    except Exception as e:
      print 'error: ' + str(e)
      return {}

  def close(self):
    # close socket
    self.sock.close()

if __name__ == '__main__':
  conn = Connection()

  method = sys.argv[1]

  if method == 'openFile':
    conn.send_json(
      {
        'jsonrpc' : '2.0',
          'id' : 0,
          'method' : 'openFile',
          'params' : {
            'fileName' : str(sys.argv[2])
          }
      }
    )

  elif method == 'kill':
    conn.send_json(
      {
        'jsonrpc' : '2.0',
          'id' : 0,
          'method' : 'kill'
      }
    )

  else:
    print 'unknown method: ' + method
    sys.exit(-1)
    conn.close()

  print 'reply: ' + str(conn.recv_message())
  conn.close()
