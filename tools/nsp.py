# nsp.py NSP OUTDIR [--list] : decrypt the program NCA's ExeFS (section 0) out of an NSP, using prod.keys / the NSP's ticket.
# Also: --section N --raw OUT dumps a decrypted section image (for RomFS work).
import struct, sys, os
from Crypto.Cipher import AES
from Crypto.Util import Counter
KEYS={}
for l in open(os.path.join(os.path.dirname(__file__),'..','..','prod.keys')):
    if '=' in l: k,v=l.split('=',1); KEYS[k.strip()]=bytes.fromhex(v.strip())
def pfs0(f,base):
    f.seek(base); h=f.read(0x10); assert h[:4]==b'PFS0',h[:4]; n,stsz=struct.unpack_from('<II',h,4)
    ents=f.read(n*0x18); strs=f.read(stsz); data=base+0x10+n*0x18+stsz; out=[]
    for i in range(n):
        off,sz,no=struct.unpack_from('<QQI',ents,i*0x18); nm=strs[no:strs.index(b'\0',no)].decode(); out.append((nm,data+off,sz))
    return out
def xts_dec(key,data,sector0,ss=0x200):
    k1=AES.new(key[:16],AES.MODE_ECB); k2=AES.new(key[16:],AES.MODE_ECB); out=bytearray()
    for s in range(len(data)//ss):
        tw=bytearray(k2.encrypt((sector0+s).to_bytes(16,'big'))); blk=data[s*ss:(s+1)*ss]
        for j in range(0,ss,16):
            x=bytes(a^b for a,b in zip(blk[j:j+16],tw)); y=k1.decrypt(x); out+=bytes(a^b for a,b in zip(y,tw))
            c=0
            for b in range(16):
                nb=((tw[b]<<1)|c)&0xff; c=tw[b]>>7; tw[b]=nb
            if c: tw[0]^=0x87
    return bytes(out)
def main():
    nsp,outdir=sys.argv[1],sys.argv[2]; f=open(nsp,'rb'); files=pfs0(f,0)
    for nm,o,s in files: print(nm,hex(s))
    tik=[x for x in files if x[0].endswith('.tik')]; titlekey=None
    if tik: f.seek(tik[0][1]+0x180); titlekey=f.read(16); f.seek(tik[0][1]+0x2a0); rid=f.read(16)
    ncas=sorted([x for x in files if x[0].endswith('.nca')],key=lambda x:-x[2])
    for nm,o,s in ncas:
        f.seek(o); hdr=xts_dec(KEYS['header_key'],f.read(0xc00),0)
        if hdr[0x200:0x204]!=b'NCA3': continue
        ctype=hdr[0x205]; print(nm,'type',hdr[0x204],'content',ctype)
        if hdr[0x205]!=0: continue
        if hdr[0x204]!=0: continue   # program
        gen=max(hdr[0x206],hdr[0x220]); gen= gen-1 if gen>0 else 0
        rights=hdr[0x230:0x240]
        if any(rights): key=AES.new(KEYS['titlekek_%02x'%gen],AES.MODE_ECB).decrypt(titlekey); print('titlekey gen',gen)
        else: key=AES.new(KEYS['key_area_key_application_%02x'%gen],AES.MODE_ECB).decrypt(hdr[0x300+0x20:0x300+0x30])
        sec=int(sys.argv[sys.argv.index('--section')+1]) if '--section' in sys.argv else 0
        st,en=struct.unpack_from('<II',hdr,0x240+sec*0x10); st*=0x200; en*=0x200
        fsh=hdr[0x400+sec*0x200:0x600+sec*0x200]; ftype,htype,etype=fsh[2],fsh[3],fsh[4]; print('section',sec,hex(st),hex(en),'fs',ftype,'hash',htype,'enc',etype)
        nonce=fsh[0x140:0x148][::-1]
        def rd(off,n):   # decrypted read at section offset
            a=off&~15; f.seek(o+st+a); d=f.read(n+(off-a)+15&~15)
            ctr=Counter.new(64,prefix=nonce,initial_value=(st+a)>>4)
            return AES.new(key,AES.MODE_CTR,counter=ctr).decrypt(d)[off-a:off-a+n]
        if '--raw' in sys.argv:
            out=open(sys.argv[sys.argv.index('--raw')+1],'wb'); n=en-st; p=0
            while p<n: k=min(1<<24,n-p); out.write(rd(p,k)); p+=k
            print('raw done'); return
        # pfs0 hash data: hash table then pfs0 at hashinfo layer offset
        hoff,hsz=struct.unpack_from('<QQ',fsh,0x8+0x20+0x8)   # sha256 hashdata: master hash(0x20) blocksize(4) layercount(4) region0 off/size, region1 off/size
        bs,lc=struct.unpack_from('<II',fsh,0x28); r0=struct.unpack_from('<QQ',fsh,0x30); r1=struct.unpack_from('<QQ',fsh,0x40)
        pbase=r1[0]; raw=rd(pbase,0x10); n,stsz=struct.unpack_from('<II',raw,4); hd=rd(pbase,0x10+n*0x18+stsz)
        strs=hd[0x10+n*0x18:]; data=pbase+0x10+n*0x18+stsz; os.makedirs(outdir,exist_ok=True)
        for i in range(n):
            off,sz,no=struct.unpack_from('<QQI',hd,0x10+i*0x18); fn=strs[no:strs.index(b'\0',no)].decode()
            open(os.path.join(outdir,fn),'wb').write(rd(data+off,sz)); print(' ',fn,hex(sz))
        return
main()
