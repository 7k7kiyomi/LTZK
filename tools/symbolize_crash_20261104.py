import ctypes as c
from ctypes import wintypes as w
dia=c.WinDLL(r'E:\VS 2019\DIA SDK\bin\amd64\msdia140.dll'); D=c.WinDLL(r'C:\Program Files (x86)\Windows Kits\10\App Certification Kit\DbgHelp.dll',use_last_error=True)
h=c.c_void_p(0x12345)
D.SymInitializeW.argtypes=[c.c_void_p,w.LPCWSTR,w.BOOL];D.SymInitializeW.restype=w.BOOL
D.SymSetOptions(0x10|0x4|0x2)
print('init',D.SymInitializeW(h,r'E:\LTZK\plugins',False),c.get_last_error())
D.SymLoadModuleExW.argtypes=[c.c_void_p,c.c_void_p,w.LPCWSTR,w.LPCWSTR,c.c_uint64,w.DWORD,c.c_void_p,w.DWORD];D.SymLoadModuleExW.restype=c.c_uint64
base=0x7ffd00760000
print('load',hex(D.SymLoadModuleExW(h,None,r'E:\LTZK\plugins\plugin_map_quality_check_tools.dll',None,base,0,None,0)),c.get_last_error())
class SI(c.Structure):
 _fields_=[('SizeOfStruct',w.ULONG),('TypeIndex',w.ULONG),('Reserved',c.c_uint64*2),('Index',w.ULONG),('Size',w.ULONG),('ModBase',c.c_uint64),('Flags',w.ULONG),('Value',c.c_uint64),('Address',c.c_uint64),('Register',w.ULONG),('Scope',w.ULONG),('Tag',w.ULONG),('NameLen',w.ULONG),('MaxNameLen',w.ULONG),('Name',c.c_char*2048)]
class LINE(c.Structure):
 _fields_=[('SizeOfStruct',w.DWORD),('Key',c.c_void_p),('LineNumber',w.DWORD),('FileName',c.c_char_p),('Address',c.c_uint64)]
D.SymFromAddr.argtypes=[c.c_void_p,c.c_uint64,c.POINTER(c.c_uint64),c.POINTER(SI)];D.SymFromAddr.restype=w.BOOL
D.SymGetLineFromAddr64.argtypes=[c.c_void_p,c.c_uint64,c.POINTER(w.DWORD),c.POINTER(LINE)];D.SymGetLineFromAddr64.restype=w.BOOL
for off in [0x85b13,0x1cc45f,0x1ccf59,0x1b644a,0x1a30ff,0x1a9e60]:
 si=SI();si.SizeOfStruct=88;si.MaxNameLen=2048;disp=c.c_uint64()
 ok=D.SymFromAddr(h,base+off,c.byref(disp),c.byref(si))
 print(hex(off),ok,si.Name.decode('utf8','replace'),hex(disp.value),'err',c.get_last_error())
 li=LINE();li.SizeOfStruct=c.sizeof(LINE);dd=w.DWORD()
 if D.SymGetLineFromAddr64(h,base+off,c.byref(dd),c.byref(li)):print('LINE',li.FileName.decode('utf8','replace'),li.LineNumber,dd.value)

