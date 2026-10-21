#!/usr/bin/env python3
"""Package a locally verified release with corresponding source and notices."""
import hashlib
import json
import pathlib
import shutil
import tarfile
root=pathlib.Path(__file__).resolve().parents[1]
version=(root/'VERSION').read_text().strip()
acceptance=json.loads((root/'build/acceptance.json').read_text())
if acceptance.get('version')!=version or not acceptance['pass'] or len(acceptance['matrix'])!=9 or acceptance.get('graphics_panics')!=2:
    raise SystemExit('Complete tools/verify.py successfully before packaging')
for row in acceptance['matrix']:
    if not row.get('no_leaks') or row.get('stress_seconds',0)<60:
        raise SystemExit('GUI stress/leak gate is incomplete')
    build=root/'build'/row['preset']
    if hashlib.sha256((build/'LamOS.iso').read_bytes()).hexdigest()!=row['iso_sha256'] or hashlib.sha256((build/'kernel.elf').read_bytes()).hexdigest()!=row['kernel_sha256']:
        raise SystemExit('Build artifacts changed after acceptance: '+row['preset'])
release=root/'build/release'
source=release/f'LamOS-{version}-source.tar.gz'
entries=['VERSION','AGENTS.md','README.md','ARCHITECTURE.md','CHANGELOG.md','LICENSE','THIRD_PARTY.md',
         'CMakeLists.txt','CMakePresets.json','.clang-format','.clang-tidy','.clangd','.gitignore',
         '.vscode','boot','cmake','docs','include','kernel','lib','tests','tools']
with tarfile.open(source,'w:gz') as archive:
    for name in entries:
        for path in ([root/name] if (root/name).is_file() else sorted((root/name).rglob('*'))):
            if path.is_file() and '__pycache__' not in path.parts and path.suffix!='.pyc':
                archive.add(path,arcname=str(path.relative_to(root)),recursive=False)
for name in ['LICENSE','THIRD_PARTY.md']:
    shutil.copyfile(root/name,release/name)
shutil.copyfile(root/'build/acceptance.json',release/'acceptance.json')
shutil.copyfile(root/'docs/VALIDATION.md',release/'validation-summary.md')
manifest={}
for name in ['LamOS.iso','kernel.elf','BOOTX64.EFI',f'LamOS-{version}-source.tar.gz','acceptance.json','validation-summary.md','LICENSE','THIRD_PARTY.md']:
    manifest[name]=hashlib.sha256((release/name).read_bytes()).hexdigest()
(release/'SHA256SUMS').write_text(''.join(f'{digest}  {name}\n' for name,digest in manifest.items()))
(release/'release.json').write_text(json.dumps({'version':version,'sha256':manifest,'acceptance':True},indent=2)+'\n')
print('Release packaged:',release)
