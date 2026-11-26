import struct, os
p=r'C:\Users\孟子腾\AppData\Local\CrashDumps\LTZK.exe.29744.dmp'
b=open(p,'rb').read()
u32=lambda o:struct.unpack_from('<I',b,o)[0]
u64=lambda o:struct.unpack_from('<Q',b,o)[0]
s={u32(o):(u32(o+4),u32(o+8)) for o in range(u32(12),u32(12)+u32(8)*12,12)}
mods=[]
r=s[4][1]
for o in range(r+4,r+4+u32(r)*108,108):
 n=u32(o+20); name=b[n+4:n+4+u32(n)].decode('utf-16le');mods.append((u64(o),u32(o+8),name))
def mod(a):
 for base,size,name in mods:
  if base<=a<base+size:return '%s+0x%x'%(name,a-base)
 return ''
e=s[6][1];tid=u32(e);ctx=u32(e+164)
print('EXCEPTION thread',tid,'code',hex(u32(e+8)),'address',mod(u64(e+24)))
print('parameters',[hex(u64(e+40+i*8)) for i in range(u32(e+32))])
print('RIP',hex(u64(ctx+248)),mod(u64(ctx+248)),'RSP',hex(u64(ctx+152)))
r=s[3][1]
for o in range(r+4,r+4+u32(r)*48,48):
 if u32(o)==tid:
  start=u64(o+24);size=u32(o+32);rva=u32(o+36);rsp=u64(ctx+152)
  print('STACK',hex(start),size)
  for i in range(max(0,rsp-start),min(size,rsp-start+4096),8):
   a=u64(rva+i);n=mod(a)
   if n:print(hex(start+i),hex(a),n)
print('PLUGIN MODULES')
for base,size,name in mods:
 if 'plugin' in name.lower():print(hex(base),hex(size),name)
