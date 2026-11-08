# dynsym.py IMG.bin : print dynamic symbols (value, size, defined/undefined, name) of a decompressed NSO image via MOD0
import struct, sys
img=open(sys.argv[1],'rb').read()
mod0=struct.unpack_from('<I',img,4)[0]; assert img[mod0:mod0+4]==b'MOD0'
dyn=mod0+struct.unpack_from('<i',img,mod0+4)[0]
tags={}
i=dyn
while True:
    t,v=struct.unpack_from('<qQ',img,i); i+=16
    if t==0: break
    tags.setdefault(t,v)
symtab,strtab,strsz=tags[6],tags[5],tags[10]
# symbol count: from DT_HASH nchain
if 4 in tags: n=struct.unpack_from('<II',img,tags[4])[1]
else: n=(strtab-symtab)//24
for k in range(n):
    nm,info,oth,shn,val,sz=struct.unpack_from('<IBBHQQ',img,symtab+k*24)
    s=img[strtab+nm:img.index(b'\0',strtab+nm)].decode('latin1')
    print('%08x %6x %s %s'%(val,sz,'D' if shn else 'U',s))
