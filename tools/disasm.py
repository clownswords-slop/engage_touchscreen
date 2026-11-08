# dis.py MAIN.bin ADDR [N] : disassemble N instructions at a main offset; annotates bl targets with dump names
import capstone, sys, pickle, os, struct
img = open(sys.argv[1], 'rb').read(); a = int(sys.argv[2], 16); n = int(sys.argv[3]) if len(sys.argv) > 3 else 80
names = {}
pk = os.path.join(os.path.dirname(sys.argv[1]), 'dump.pkl')
if os.path.exists(pk):
    for c, d in pickle.load(open(pk, 'rb')).items():
        for m in d['methods']:
            if m[1]: names.setdefault(m[1], c + '::' + m[0])
md = capstone.Cs(capstone.CS_ARCH_ARM64, capstone.CS_MODE_ARM)
for i in md.disasm(img[a:a + 4 * n], a):
    s = '%x  %-8s %s' % (i.address, i.mnemonic, i.op_str)
    if i.mnemonic in ('bl', 'b') and i.op_str.startswith('#'):
        t = int(i.op_str[1:], 16); s += '   ; ' + names.get(t, '')
    print(s)
    if i.mnemonic == 'ret' and '--stop' in sys.argv: break
