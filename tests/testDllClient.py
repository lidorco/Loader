import sys
import socket


if len(sys.argv) < 2:
	print("Usage: testDllClient.py some.dll")
	exit()


s = socket.socket(socket.AF_INET ,socket.SOCK_STREAM)

s.connect(("localhost", 9999))

dll_file = open(sys.argv[1], 'rb')


binary_dll = dll_file.read()


s.sendall(binary_dll)
s.close()

