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
            print("reply:" + str(self.__json("recv_msg",None)))
        except Exception as exception:
            print("error while connecting: " + str(exception))
            sys.exit(1)

    def __json(self, method, file):
        """
        Send a JSON-RPC request to the named pipe.
        :param method: The JSON-RPC request method.
        Send a message to the named pipe
        :param file: file corresponding to method.

        """
        if method == "recv_msg":
        
            size = 1024
            packet = self.sock.recv(size)
            return packet[4:]
            # try:
            #     return json.loads(msg_packet)
            # except Exception as exception:
            #     print("error: " + str(exception))
            #     return {} 
        else:     
            msg = {
                    "jsonrpc": "2.0",
                    "id": 0,
                    "method": method,
                    "params": {"fileName": file},
                }
            json_msg = json.dumps(msg)
            size = len(json_msg)
            header = struct.pack(">I", size)
            packet = header + json_msg.encode("ascii")
            self.sock.send(packet)
                 
    def open_file(self, file):
        """Opens file"""
        # param: file is filename input by the user in string
        method = "openFile"
        self.__json(method,file)        

    def save_graphic(self, file):
        """Save Graphic"""
        method = "saveGraphic"
        self.__json(method,file)
  
    def kill(self):
        """To kill the current operation"""
        method = "kill"
        self.__json(method,None)
  
    def close(self):
        '''Close the socket to the named pipe'''
        self.sock.close()