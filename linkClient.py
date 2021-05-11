from ctypes import *
import os
import ctypes

so_dir= os.path.abspath(os.path.dirname("cgi-bin"))
so_file=os.path.join(so_dir,"so_client.so")
cFunction=ctypes.CDLL(so_file)

address="127.0.0.1"
port="9000"

# endoe to byte order
b_address=address.encode('UTF-8')
b_port=port.encode('UTF-8')



# limit required types of parameters needed to be pass
cFunction.client.argtypes= [ctypes.c_char_p,ctypes.c_char_p] 
cFunction.client(b_address,b_port)

#cFunction.client(address,port)