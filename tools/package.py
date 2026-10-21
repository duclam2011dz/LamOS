#!/usr/bin/env python3
"""Package a locally verified release with corresponding source and notices."""
import hashlib
import json
import pathlib
import shutil
import tarfile
root=pathlib.Path(__file__).resolve().parents[1]
acceptance=json.loads((root/'build/acceptance.json').read_text())
if not acceptance['pass'] or len(acceptance['matrix'])!=9:
    raise SystemExit('Complete tools/verify.py successfully before packaging')
for row in acceptance['matrix']:
    build=root/'build'/row['preset']
    if hashlib.sha256((build/'LamOS.iso').read_bytes()).hexdigest()!=row['iso_sha256'] or hashlib.sha256((build/'kernel.elf').read_bytes()).hexdigest()!=row['kernel_sha256']:
        raise SystemExit('Build artifacts changed after acceptance: '+row['preset'])
release=root/'build/release'
source=release/'LamOS-1.0.0-source.tar.gz'
entries=['AGENTS.md','README.md','ARCHITECTURE.md','CHANGELOG.md','LICENSE','THIRD_PARTY.md',
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
manifest={}
for name in ['LamOS.iso','kernel.elf','BOOTX64.EFI','LamOS-1.0.0-source.tar.gz']:
    manifest[name]=hashlib.sha256((release/name).read_bytes()).hexdigest()
(release/'SHA256SUMS').write_text(''.join(f'{digest}  {name}\n' for name,digest in manifest.items()))
(release/'release.json').write_text(json.dumps({'version':'1.0.0','sha256':manifest,'acceptance':True},indent=2)+'\n')
print('Release packaged:',release)
