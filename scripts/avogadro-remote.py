#!/usr/bin/python

from __future__ import print_function

import sys
import json
import time
import socket
import struct
import tempfile

class Connection:
  def __init__(self, name = "avogadro"):
    # create socket
    self.sock = socket.socket(socket.AF_UNIX,
                              socket.SOCK_STREAM)

    # connect
    self.sock.connect(tempfile.gettempdir() + '/' + name)

  def send_json(self, obj):
    self.send_message(json.dumps(obj))

  def send_message(self, msg):
    sz = len(msg)
    hdr = struct.pack('>I', sz)
    pkt = hdr + msg.encode('ascii')
    self.sock.send(pkt)

  def recv_message(self, size = 1024):
    pkt = self.sock.recv(size)

    return pkt[4:]

  def recv_json(self):
    msg = self.recv_message()

    try:
      return json.loads(msg)
    except Exception as e:
      print('error: ' + str(e))
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
    print('unknown method: ' + method)
    conn.close()
    sys.exit(-1)

  print('reply: ' + str(conn.recv_message()))
  conn.close()
