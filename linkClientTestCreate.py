from ctypes import *
import os
import ctypes

so_dir= os.path.abspath(os.path.dirname("cgi-bin"))
so_file=os.path.join(so_dir,"so_testCreate.so")
cFunction=ctypes.CDLL(so_file)

address="127.0.0.1"
port="9000"
username="Michle"
password="1023kejntre"

# endoe to byte order
b_address=address.encode('UTF-8')
b_port=port.encode('UTF-8')
b_username=username.encode('UTF-8')
b_password=password.encode('UTF-8')

# limit required types of parameters needed to be pass
cFunction.createAccount.argtypes= [ctypes.c_char_p, ctypes.c_char_p, ctypes.c_char_p, ctypes.c_char_p] 
cFunction.createAccount(b_address,b_port,b_username,b_password)

#cFunction.client(address,port)