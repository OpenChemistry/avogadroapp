#!/usr/bin/python

from __future__ import print_function

import sys
import json
import socket
import struct
import tempfile

class Connection:
    '''Process a JSON-RPC request'''

    def __init__(self, name="avogadro"):
        """
        Connect to the local named pipe

        :param name: The name of the named pipe.
        """
        # create socket
        self.sock = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)

        # connect
        try:
            self.sock.connect(tempfile.gettempdir() + "/" + name)
            # print the connection statement
            print("reply:" + str(self.receive_message()))
        except Exception as exception:
            print("error while connecting: " + str(exception))
            sys.exit(1)

    def send_json(self, obj):
        """
        Send a JSON-RPC request to the named pipe.

        :param obj: The JSON-RPC request object.
        """
        self.send_message(json.dumps(obj))

    def send_message(self, msg):
        """
        Send a message to the named pipe

        :param msg: The message to send.
        """
        size = len(msg)
        header = struct.pack(">I", size)
        packet = header + msg.encode("ascii")
        self.sock.send(packet)

    def receive_message(self, size=1024):
        """
        Receive a message from the named pipe.

        :param size: The maximum size of the message to receive.
        """
        packet = self.sock.recv(size)

        return packet[4:]

    def recv_json(self):
        '''Receive a JSON-RPC response'''
        msg = self.recv_message()

        try:
            return json.loads(msg)
        except Exception as exception:
            print("error: " + str(exception))
            return {}            
  
    def open_file(self, file):
        """Opens file"""
        self.send_json(
            {
                "jsonrpc": "2.0",
                "id": 0,
                "method": "openFile",
                "params": {"fileName": file},
            }
        )
  
    def save_graphic(self, file):
        """Save Graphic"""
        self.send_json(
            {
                "jsonrpc": "2.0",
                "id": 0,
                "method": "saveGraphic",
                "params": {"fileName": file},
            }
        )
  
    def kill(self):
        """To kill the current operation"""
        self.send_json(
            {
                "jsonrpc": "2.0",
                "id": 0,
                "method": "kill"
            }
        ) 
  
    def close(self):
        '''Close the socket to the named pipe'''
        self.sock.close()