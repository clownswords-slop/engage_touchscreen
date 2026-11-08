# il2dump.py MAIN.bin global-metadata.dat OUT.txt
# Minimal IL2CPP (metadata v27, 64-bit) dumper: classes, parents, fields with offsets and types, methods with main offsets.
# MAIN.bin is the decompressed main (tools/nso.py); its R_AARCH64_RELATIVE relocations are applied with base 0, so every pointer
# read from it is a main offset. Writes OUT.txt and OUT.pkl (dict for scripted lookups).
import struct, sys, pickle, re
img = bytearray(open(sys.argv[1], 'rb').read())
md = open(sys.argv[2], 'rb').read()
out = sys.argv[3]
u32 = lambda b, o: struct.unpack_from('<I', b, o)[0]
s32 = lambda b, o: struct.unpack_from('<i', b, o)[0]
u64 = lambda b, o: struct.unpack_from('<Q', b, o)[0]

# ---- relocate main ----
mod0 = u32(img, 4); dyn = mod0 + s32(img, mod0 + 4); tags = {}
i = dyn
while True:
    t, v = struct.unpack_from('<qQ', img, i); i += 16
    if t == 0: break
    tags.setdefault(t, v)
rela, relasz = tags[7], tags[8]
nrel = 0
for o in range(rela, rela + relasz, 24):
    off, info, add = struct.unpack_from('<QQq', img, o)
    if (info & 0xffffffff) == 0x403: struct.pack_into('<Q', img, off, add & 0xffffffffffffffff); nrel += 1
print('relocs', nrel)

# ---- metadata ----
assert u32(md, 0) == 0xfab11baf and s32(md, 4) == 27
H = [struct.unpack_from('<II', md, 8 + 8 * k) for k in range(31)]
(strlit, strlitdata, strings, events, props, methods, pdef, fdef, dvdata, marsh, params, fields, gparams, gpc, gcont, nested,
 ifaces, vtm, ioff, typedefs, images, assemblies, fieldrefs, refasm, attrinfo, attrtypes, uvcpt, uvcpr, wrtn, wrts, exported) = H
def S(idx):
    o = strings[0] + idx; e = md.index(b'\0', o); return md[o:e].decode('utf8', 'replace')
TD = 88
ntd = typedefs[1] // TD
tds = []
for k in range(ntd):
    o = typedefs[0] + k * TD
    v = struct.unpack_from('<7iI8i8H2I', md, o)
    tds.append(dict(name=S(v[0]), ns=S(v[1]), byval=v[2], decl=v[3], parent=v[4], elem=v[5], gc=v[6], flags=v[7],
                    fieldStart=v[8], methodStart=v[9], eventStart=v[10], propStart=v[11], nestedStart=v[12],
                    mcount=v[16], pcount=v[17], fcount=v[18], ecount=v[19], ncount=v[20], vcount=v[21], icount=v[22],
                    bits=v[24], token=v[25]))
def meth(k):
    o = methods[0] + k * 32; v = struct.unpack_from('<5iI4H', md, o)
    return dict(name=S(v[0]), decl=v[1], ret=v[2], pstart=v[3], gc=v[4], token=v[5], flags=v[6], iflags=v[7], slot=v[8], pcount=v[9])
def fld(k):
    o = fields[0] + k * 12; v = struct.unpack_from('<iiI', md, o); return dict(name=S(v[0]), type=v[1], token=v[2])
def par(k):
    o = params[0] + k * 12; v = struct.unpack_from('<iIi', md, o); return dict(name=S(v[0]), type=v[2])
imgs = []
for k in range(images[1] // 40):
    v = struct.unpack_from('<10i', md, images[0] + k * 40); imgs.append(dict(name=S(v[0]), tstart=v[2], tcount=v[3]))

# ---- find the registrations in main ----
# MetadataRegistration: fieldOffsetsCount (+0x50) == typeDefinitionsSizesCount (+0x60) == number of type definitions
mreg = None
for o in range(0x3f48000, len(img) - 0x80, 8):
    if u64(img, o) == ntd and u64(img, o + 16) == ntd:
        b = o - 0x50
        if u64(img, b + 0x30) > ntd and u64(img, b + 0x38) < len(img):
            mreg = b; break
print('metadataRegistration', hex(mreg))
ntypes, ptypes = u64(img, mreg + 0x30), u64(img, mreg + 0x38)
pfo = u64(img, mreg + 0x58)
# CodeRegistration: codeGenModules: pointer array of modules whose first field points to an image name
name_at = {}
for im in imgs:
    nb = im['name'].encode() + b'\0'
    for m in re.finditer(re.escape(nb), img):
        if m.start() == 0 or img[m.start() - 1] == 0: name_at[m.start()] = im['name']
mods = {}
for o in range(0x3f48000, len(img) - 8, 8):
    p = u64(img, o)
    if p in name_at and name_at[p] not in mods:
        cnt, mp = u64(img, o + 8), u64(img, o + 16)
        if 0 <= cnt < 200000 and (cnt == 0 or mp < len(img)): mods[name_at[p]] = (cnt, mp)
print('codegen modules', len(mods), 'of', len(imgs))

# ---- types ----
PRIM = {1: 'void', 2: 'bool', 3: 'char', 4: 'sbyte', 5: 'byte', 6: 'short', 7: 'ushort', 8: 'int', 9: 'uint', 10: 'long', 11: 'ulong',
        12: 'float', 13: 'double', 14: 'string', 0x18: 'IntPtr', 0x19: 'UIntPtr', 0x1c: 'object', 0x16: 'TypedRef'}
def tdname(k):
    t = tds[k]; n = t['name']
    if t['decl'] >= 0: return tdname(type_td(t['decl'])) + '.' + n
    return (t['ns'] + '.' if t['ns'] else '') + n
def type_td(ti):
    o = u64(img, ptypes + 8 * ti); return u64(img, o) & 0xffffffff
def tname(ti, depth=0):
    if ti < 0 or ti >= ntypes or depth > 6: return '?'
    o = u64(img, ptypes + 8 * ti); data = u64(img, o); bits = u32(img, o + 8); ty = (bits >> 16) & 0xff; byref = (bits >> 29) & 1
    return tname_raw(data, ty, depth) + ('&' if byref else '')
def tname_ptr(o, depth):
    data = u64(img, o); bits = u32(img, o + 8); ty = (bits >> 16) & 0xff; return tname_raw(data, ty, depth)
def tname_raw(data, ty, depth):
    if ty in PRIM: return PRIM[ty]
    if ty in (0x11, 0x12): return tdname(data & 0xffffffff)
    if ty == 0x1d: return tname_ptr(data, depth + 1) + '[]'
    if ty == 0x14: return tname_ptr(u64(img, data), depth + 1) + '[,]'
    if ty == 0x0f: return tname_ptr(data, depth + 1) + '*'
    if ty in (0x13, 0x1e):
        o = gparams[0] + (data & 0xffffffff) * 16
        if (data & 0xffffffff) * 16 < gparams[1]: return S(s32(md, o + 4))
        return 'T'
    if ty == 0x15:   # generic instance: Il2CppGenericClass { typeDefinitionIndex/type, context { class_inst, method_inst }, cached }
        gtype = u64(img, data); base = tname_ptr(gtype, depth + 1) if gtype < len(img) and gtype > 0x1000 else '?'
        ci = u64(img, data + 8); args = []
        if 0 < ci < len(img):
            n, argv = u64(img, ci), u64(img, ci + 8)
            for a in range(min(n, 8)): args.append(tname_ptr(u64(img, argv + 8 * a), depth + 1))
        return base.split('`')[0] + '<' + ','.join(args) + '>'
    return 'ty%x' % ty

# constant values (Il2CppFieldDefaultValue: fieldIndex, typeIndex, dataIndex)
fconst = {}
for o in range(fdef[0], fdef[0] + fdef[1], 12):
    fi, ti, di = struct.unpack_from('<iii', md, o)
    if di >= 0: fconst[fi] = (ti, dvdata[0] + di)
def constval(fi):
    ti, o = fconst[fi]
    oo = u64(img, ptypes + 8 * ti); ty = (u32(img, oo + 8) >> 16) & 0xff
    if ty == 0x11 or ty == 0x12:   # enum: underlying type of its value__ field
        td = tds[u64(img, oo) & 0xffffffff]
        if td['fcount']: F = fld(td['fieldStart']); oo = u64(img, ptypes + 8 * F['type']); ty = (u32(img, oo + 8) >> 16) & 0xff
    sz = {2: 1, 3: 2, 4: 1, 5: 1, 6: 2, 7: 2, 8: 4, 9: 4, 10: 8, 11: 8, 12: 4, 13: 8}.get(ty)
    if not sz: return None
    v = int.from_bytes(md[o:o + sz], 'little', signed=ty in (4, 6, 8, 10))
    return v
# image of each type, method pointers
td_img = {}
for im in imgs:
    for k in range(im['tstart'], im['tstart'] + im['tcount']): td_img[k] = im['name']
lines = []; db = {}
for k, t in enumerate(tds):
    full = tdname(k); im = td_img.get(k, '?')
    par_ = tname(t['parent']) if t['parent'] >= 0 else ''
    lines.append('\n// %s  td %d' % (im, k))
    lines.append('class %s : %s  (flags %x)' % (full, par_, t['flags']))
    fo = u64(img, pfo + 8 * k) if pfo else 0
    fl = []
    for f in range(t['fcount']):
        F = fld(t['fieldStart'] + f)
        off = s32(img, fo + 4 * f) if 0 < fo < len(img) else -1
        tt = tname(F['type'])
        o = u64(img, ptypes + 8 * F['type']); attrs = u32(img, o + 8) & 0xffff
        st = 'static ' if attrs & 0x10 else ''
        if attrs & 0x40: st = 'const '
        cv = constval(t['fieldStart'] + f) if (attrs & 0x40) and (t['fieldStart'] + f) in fconst else None
        if cv is not None: lines.append('  %s%s %s = %d;' % (st, tt, F['name'], cv))
        else: lines.append('  %s%s %s; // 0x%x' % (st, tt, F['name'], off & 0xffffffff))
        fl.append((F['name'], off, tt, st.strip()))
    ml = []
    cnt, mp = mods.get(im, (0, 0))
    for m in range(t['mcount']):
        M = meth(t['methodStart'] + m)
        rid = M['token'] & 0xffffff; addr = u64(img, mp + 8 * (rid - 1)) if 0 < rid <= cnt else 0
        ps = []
        for a in range(M['pcount']):
            P = par(M['pstart'] + a); ps.append(tname(P['type']) + ' ' + P['name'])
        st = 'static ' if M['flags'] & 0x10 else ''
        sl = (' slot %d' % M['slot']) if M['slot'] != 0xffff else ''
        lines.append('  %s%s %s(%s); // 0x%x%s' % (st, tname(M['ret']), M['name'], ', '.join(ps), addr, sl))
        ml.append((M['name'], addr, M['slot'], [p.split(' ')[0] for p in ps], tname(M['ret'])))
    db[full] = dict(td=k, parent=par_, fields=fl, methods=ml, image=im, nested=t['ncount'])
open(out, 'w', encoding='utf8').write('\n'.join(lines))
pickle.dump(db, open(out.rsplit('.', 1)[0] + '.pkl', 'wb'))
print('types', ntd, 'written', out)
