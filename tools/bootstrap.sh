#!/usr/bin/env bash
# Ubuntu 22.04 package bootstrap; downloads/extracts locally, never invokes sudo.
set -euo pipefail
project_root=$(cd "$(dirname "$0")/.." && pwd)
mkdir -p "$project_root/.tools"
cd "$project_root/.tools"
python3 - "$project_root/tools/packages.lock.json" <<'PY'
import hashlib,json,pathlib,subprocess,sys
for package in json.load(open(sys.argv[1])):
    path=pathlib.Path(package['file'])
    if not path.exists():
        subprocess.run(['apt-get','download',package['package']+'='+package['version']],check=True)
    if hashlib.sha256(path.read_bytes()).hexdigest()!=package['sha256']:
        raise SystemExit('Package checksum mismatch: '+str(path))
    subprocess.run(['dpkg-deb','-x',str(path),'root'],check=True)
PY
ln -sf clangd-14 root/usr/bin/clangd
if [[ ${1:-} == --bochs ]]; then
    if [[ ! -d bochs-src ]]; then
        git clone --depth 1 --branch REL_2_7_FINAL https://github.com/bochs-emu/Bochs.git bochs-src
    fi
    [[ $(git -C bochs-src rev-parse HEAD) == 0e45b736ef9792eb9b752b0a35db49eaf2faea47 ]]
    if git -C bochs-src apply --check "$project_root/tools/bochs-rfb-depth.patch"; then
        git -C bochs-src apply "$project_root/tools/bochs-rfb-depth.patch"
    else
        git -C bochs-src apply --reverse --check "$project_root/tools/bochs-rfb-depth.patch"
    fi
    cd bochs-src/bochs
    ./configure --with-rfb --without-x11 --enable-x86-64 --enable-cpu-level=6 \
        --enable-debugger --disable-docbook --disable-plugins
    make -j"${JOBS:-4}"
fi
printf '%s\n' 'Local tools ready. From the project root: . tools/env.sh'
