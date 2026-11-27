"""Extract the largest RT_ICON payload from torrenter.exe for verification."""
from pathlib import Path
import io
import struct

import pefile
from PIL import Image, ImageChops

exe = Path(r'C:/Users/Scott/Desktop/Torrenter/torrenter/build-auto/torrenter.exe')
out = exe.parent / 'exe-icon-check.png'
pe = pefile.PE(str(exe), fast_load=True)
pe.parse_data_directories(directories=[pefile.DIRECTORY_ENTRY['IMAGE_DIRECTORY_ENTRY_RESOURCE']])
groups = None
icons = {}
for entry in pe.DIRECTORY_ENTRY_RESOURCE.entries:
    if entry.id == pefile.RESOURCE_TYPE['RT_GROUP_ICON']:
        groups = entry
    elif entry.id == pefile.RESOURCE_TYPE['RT_ICON']:
        for e in entry.directory.entries:
            for leaf in e.directory.entries:
                icons[e.id] = pe.get_data(leaf.data.struct.OffsetToData, leaf.data.struct.Size)

assert groups is not None, 'no RT_GROUP_ICON'
g = groups.directory.entries[0].directory.entries[0]
group_data = pe.get_data(g.data.struct.OffsetToData, g.data.struct.Size)
count = struct.unpack_from('<H', group_data, 4)[0]
entries = []
for i in range(count):
    w, h, cc, res, planes, bpp, size, nid = struct.unpack_from('<BBBBHHIH', group_data, 6 + 14 * i)
    entries.append((w or 256, h or 256, size, nid))
print('exe group entries:', entries)
chosen = max(entries, key=lambda e: e[0])
data = icons[chosen[3]]
print('chosen:', chosen, 'signature:', data[:8])
if data[:8] == b'\x89PNG\r\n\x1a\n':
    out.write_bytes(data)
else:
    Image.open(io.BytesIO(data)).convert('RGBA').save(out)
print('saved:', out, Image.open(out).size, len(data), 'bytes')
ref = Image.open(r'C:/Users/Scott/Desktop/Torrenter/torrenter/tools/brand-generated/exe-icon-256.png').convert('RGBA')
chk = Image.open(out).convert('RGBA')
print('pixel-identical to SVG render:', ImageChops.difference(ref, chk).getbbox() is None)
