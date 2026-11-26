exec(open(r'E:\AAALTZK\tools\inspect_crash_20261104.py',encoding='utf-8-sig').read().split("print('EXCEPTION")[0])
ctx=u32(e+164);tid=u32(e)
r=s[3][1]
for o in range(r+4,r+4+u32(r)*48,48):
 if u32(o)==tid:st=u64(o+24);sz=u32(o+32);sr=u32(o+36)
def mem(a):
 if st<=a<st+sz-7:return u64(sr+a-st)
 raise Exception('unavailable memory '+hex(a))
cache={}
def pe(path):
 if path in cache:return cache[path]
 d=open(path,'rb').read();p=struct.unpack_from('<I',d,60)[0];ns=struct.unpack_from('<H',d,p+6)[0];opt=p+24;sec=opt+struct.unpack_from('<H',d,p+20)[0]
 ss=[struct.unpack_from('<IIII',d,sec+i*40+8) for i in range(ns)]
 def rv(a):
  for vs,va,raw,ptr in ss:
   if va<=a<va+max(vs,raw):return ptr+a-va
  return a
 er,es=struct.unpack_from('<II',d,opt+112+3*8);ep=rv(er)
 funcs=[struct.unpack_from('<III',d,i) for i in range(ep,ep+es,12)]
 cache[path]=(d,rv,funcs);return cache[path]
regs=[u64(ctx+120+i*8) for i in range(16)]
rsp=regs[4]+8;rip=mem(regs[4])
print('UNWOUND FRAMES, first runtime return -> plugin')
for frame in range(18):
 matches=[m for m in mods if m[0]<=rip<m[0]+m[1]]
 if not matches:break
 base,size,path=matches[0];a=rip-base;print(frame,hex(rsp),path,'+0x%x'%a)
 try:
  d,rv,funcs=pe(path);entries=[f for f in funcs if f[0]<=a<f[1]]
  if not entries:rip=mem(rsp);rsp+=8;continue
  begin,end,unw=entries[0];print(' function range',hex(begin),hex(end))
  while True:
   q=rv(unw);verflags,prolog,count,fr=struct.unpack_from('<BBBB',d,q);i=0
   while i<count:
    code,op=struct.unpack_from('<BB',d,q+4+i*2);i+=1;kind=op&15;info=op>>4
    if kind==0:regs[info]=mem(rsp);rsp+=8
    elif kind==1:
     if info==0:rsp+=struct.unpack_from('<H',d,q+4+i*2)[0]*8;i+=1
     else:rsp+=struct.unpack_from('<I',d,q+4+i*2)[0];i+=2
    elif kind==2:rsp+=info*8+8
    elif kind==3:rsp=regs[fr&15]-(fr>>4)*16
    elif kind==4:i+=1
    elif kind==5:i+=2
    elif kind==8:i+=1
    elif kind==9:i+=2
    else:raise Exception('unknown op '+str(kind))
   if verflags&32:
    begin,end,unw=struct.unpack_from('<III',d,q+4+((count+1)//2)*4)
   else:break
  rip=mem(rsp);rsp+=8
 except Exception as ex:print(ex);break
