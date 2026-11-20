#!/usr/bin/env python3
"""Convert JUCE-generated VST3 moduleinfo.json to STRICT valid JSON.

JUCE 8.0.12 emits dangling commas after final array/object members.
Some Windows hosts accept this JSON5-like syntax, others reject it.
This post-link step preserves every string, field and numeric value.
Never modifies the VST3 PE DLL or private firmware bytes.
"""
from __future__ import annotations
import argparse
import json
from pathlib import Path

def strip_trailing_commas(source: str) -> tuple[str,int]:
    chars=[]
    quoted=False
    escaped=False
    drops=0
    for i,c in enumerate(source):
        if quoted:
            chars.append(c)
            if escaped:
                escaped=False
            elif c=='\\':
                escaped=True
            elif c=='"':
                quoted=False
        elif c=='"':
            quoted=True
            chars.append(c)
        elif c==',':
            j=i+1
            while j<len(source) and source[j] in ' \n\r\t':
                j+=1
            if j<len(source) and source[j] in '}]':
                drops+=1
            else:
                chars.append(c)
        else:
            chars.append(c)
    if quoted or escaped:
        raise ValueError("Unterminated JSON string")
    return ''.join(chars),drops

def main():
    ap=argparse.ArgumentParser()
    ap.add_argument('--root',type=Path,required=True)
    args=ap.parse_args()
    matches=list(args.root.rglob('moduleinfo.json'))
    if len(matches)!=1:
        raise SystemExit(f"Expected exactly 1 VST3 moduleinfo.json, found {len(matches)}")
    file=matches[0]
    raw=file.read_text(encoding='utf-8')
    cleaned,drops=strip_trailing_commas(raw)
    data=json.loads(cleaned)  # cannot hide any other malformed JSON
    if data.get('Name')!='125A Akai S3000XL' or len(data.get('Classes',[]))!=2:
        raise ValueError("Wrong VST3 module identity or class metadata")
    if drops:
        tmp=file.with_suffix('.json.new')
        tmp.write_text(json.dumps(data,ensure_ascii=False,indent=2)+'\n',encoding='utf-8')
        tmp.replace(file)
    assert json.loads(file.read_text(encoding='utf-8'))==data
    print(f"STRICT_MODULEINFO_JSON=PASS removed_trailing_commas={drops} file={file}")

if __name__=="__main__":
    main()
