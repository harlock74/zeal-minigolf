#!/usr/bin/env python3
"""Reserve space above static storage for the Zeal runtime stack."""
import argparse
import re
from pathlib import Path

def check(path, reserve=2048):
    match=re.search(r'^\s*([0-9A-Fa-f]+)\s+s__HEAP\b',path.read_text(),re.MULTILINE)
    if not match: raise ValueError('Static-storage end missing from linker map')
    end=int(match.group(1),16)
    available=0x10000-end
    if end<0x4000 or available<reserve:
        raise ValueError(f'Static end 0x{end:X}: only {available} bytes above it; {reserve} required')
    print(f'Static end 0x{end:04X}: {available} bytes of address headroom; {reserve}-byte runtime reserve passes.')

if __name__=='__main__':
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('map',type=Path);parser.add_argument('--reserve',type=int,default=2048)
    args=parser.parse_args();check(args.map,args.reserve)
