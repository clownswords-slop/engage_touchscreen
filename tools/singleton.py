# singleton.py MAIN.bin CLASS [lo hi] : find the metadata slot of SingletonClass<CLASS>.get_Instance's MethodInfo by looking for
# `adrp xN, P; ldr xN, [xN, #o]; ... ldr x0, [xN]; bl get_Instance(0x1c3130)` followed by a call to one of CLASS's methods.
import capstone, sys, pickle, os, re
img = open(sys.argv[1], 'rb').read(); cls = sys.argv[2]
db = pickle.load(open(os.path.join(os.path.dirname(sys.argv[1]), 'dump.pkl'), 'rb'))
targets = {m[1] for m in db[cls]['methods'] if m[1]}
lo, hi = (int(sys.argv[3], 16), int(sys.argv[4], 16)) if len(sys.argv) > 4 else (0, 0x3f47038)
md = capstone.Cs(capstone.CS_ARCH_ARM64, capstone.CS_MODE_ARM); md.skipdata = True
ins = list(md.disasm(img[lo:hi], lo)); slots = {}
regpage = {}; regslot = {}
for k, i in enumerate(ins):
    if i.mnemonic == 'adrp': r, v = i.op_str.split(', #'); regpage[r] = int(v, 16)
    elif i.mnemonic == 'ldr':
        m = re.match(r'(x\d+), \[(x\d+), #0x([0-9a-f]+)\]$', i.op_str)
        if m and m.group(2) in regpage and m.group(1) == m.group(2): regslot[m.group(1)] = regpage[m.group(2)] + int(m.group(3), 16)
    if i.mnemonic == 'bl' and i.op_str == '#0x1c3130':
        # which register's slot fed x0?
        for j in range(k - 1, max(k - 6, 0), -1):
            m = re.match(r'x0, \[(x\d+)\]$', ins[j].op_str)
            if ins[j].mnemonic == 'ldr' and m and m.group(1) in regslot:
                s = regslot[m.group(1)]
                for t in ins[k + 1:k + 8]:
                    if t.mnemonic == 'bl' and int(t.op_str[1:], 16) in targets: slots[s] = slots.get(s, 0) + 1
                break
for s, n in sorted(slots.items(), key=lambda x: -x[1]): print(hex(s), n)
