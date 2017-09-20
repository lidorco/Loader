import socket

s = socket.socket(socket.AF_INET ,socket.SOCK_STREAM)


s.connect(("localhost", 9999))

dll_file = open(r"Debug/TestDll.dll", 'rb')


binary_dll = dll_file.read()


s.sendall(binary_dll)
s.close()

