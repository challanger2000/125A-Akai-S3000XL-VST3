#!/usr/bin/env python3
"""Compile MAME's exact temp_r() function and prove STATUS semantics via a fake bus.

This verifies only MB89352 register semantics, not actual original-ROM CD loading.
"""
import argparse
import os
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile


def extract_method(text):
    signature = "uint8_t mb87030_device::temp_r()"
    start = text.find(signature)
    if start < 0 or text.find(signature, start + 1) >= 0:
        raise ValueError("Exactly one temp_r implementation required")
    first = text.find("{", start + len(signature))
    if first < 0:
        raise ValueError("Method body missing")
    depth = 0
    for pos in range(first, len(text)):
        if text[pos] == "{":
            depth += 1
        elif text[pos] == "}":
            depth -= 1
            if depth == 0:
                return text[start:pos+1]
    raise ValueError("Unterminated function")


def compile_probe(method):
    compiler = shutil.which("g++") or shutil.which("clang++")
    if not compiler:
        raise RuntimeError("No C++ compiler; refusing to SKIP")
    prefix = r'''
#include <cstdint>
#include <cstdio>
#define LOG(...) ((void)0)
constexpr uint8_t S_PHASE_STATUS=0x03;
constexpr uint8_t PCTL_PHASE_MASK=0x07;
constexpr uint8_t S_INP=0x40, S_REQ=0x80;
struct Bus {
    uint8_t control=0, data=0;
    int reads=0;
    uint8_t ctrl_r() { return control; }
    uint8_t data_r() { ++reads; return data; }
};
class mb87030_device {
public:
    Bus* m_scsi_bus=nullptr;
    uint8_t m_pctl=0,m_temp=0;
    int steps=0;
    bool mutate_ctrl_on_step=false;
    void step(bool) { ++steps; if (mutate_ctrl_on_step) m_scsi_bus->control=0; }
    uint8_t temp_r();
};
'''
    suffix = r'''
int main() {
    struct Case{const char* name;uint8_t phase,flags,expected;int reads;bool change;};
    const Case cases[]={
        {"status_ready",S_PHASE_STATUS,S_INP|S_REQ,0x5a,1,false},
        {"status_before_step",S_PHASE_STATUS,S_INP|S_REQ,0x5a,1,true},
        {"wrong_phase",0x01,S_INP|S_REQ,0x11,0,false},
        {"no_req",S_PHASE_STATUS,S_INP,0x11,0,false},
        {"no_inp",S_PHASE_STATUS,S_REQ,0x11,0,false},
        {"no_signals",S_PHASE_STATUS,0,0x11,0,false}
    };
    for(const auto& t:cases) {
        Bus b;b.control=t.flags;b.data=0x5a;
        mb87030_device d;d.m_scsi_bus=&b;d.m_pctl=t.phase;d.m_temp=0x11;
        d.mutate_ctrl_on_step=t.change;
        auto got=d.temp_r();
        if(got!=t.expected || b.reads!=t.reads || d.steps!=1) {
            std::fprintf(stderr,"FAIL %s got=%02x expected=%02x reads=%d expected=%d steps=%d\n",
                         t.name,got,t.expected,b.reads,t.reads,d.steps);
            return 1;
        }
    }
    std::puts("P049_MB89352_TEMP_STATUS_SIX_CASES_PASS");
}
'''
    with tempfile.TemporaryDirectory(prefix="akai-p049-") as folder:
        cpp=Path(folder)/"probe.cpp"
        exe=Path(folder)/("probe.exe" if os.name=="nt" else "probe")
        cpp.write_text(prefix+method+suffix,encoding="utf-8")
        subprocess.run([compiler,"-std=c++17","-Wall","-Wextra","-Werror",
                        str(cpp),"-o",str(exe)],check=True)
        return subprocess.call([str(exe)])


def run(mame_root):
    file=mame_root/"src/devices/machine/mb87030.cpp"
    actual=extract_method(file.read_text(encoding="utf-8"))
    if compile_probe(actual)!=0:
        print("P049 FAIL: patched MAME STATUS read",file=sys.stderr)
        return 1
    bad=r'''uint8_t mb87030_device::temp_r()
{
    step(false);
    LOG("%s: %02X\n", __FUNCTION__, m_temp);
    return m_temp;
}'''
    if compile_probe(bad)==0:
        print("P049 FAIL: pre-fix mutant escaped negative test",file=sys.stderr)
        return 1
    print("P049_MB89352_NEGATIVE_CONTROL_KILLED")
    return 0


if __name__=="__main__":
    parser=argparse.ArgumentParser()
    parser.add_argument("--mame-root",type=Path,required=True)
    args=parser.parse_args()
    try:
        sys.exit(run(args.mame_root))
    except (OSError,ValueError,RuntimeError,subprocess.CalledProcessError) as exc:
        print(f"P049_SCSI_TEMP_TEST_FAILURE: {exc}",file=sys.stderr)
        sys.exit(2)
