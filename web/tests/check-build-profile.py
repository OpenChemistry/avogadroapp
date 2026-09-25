#!/usr/bin/env python3
"""Verify a freshly installed WASM web profile (no compilation required)."""
import re
import sys
from pathlib import Path

build = Path(sys.argv[1]).resolve()
libs = build / 'avogadrolibs'
app = build / 'avogadroapp'
prefix = build / 'prefix'
excluded = ['pythonscript.cpp', 'packagemanager.cpp', 'interfacescript.cpp',
            'interfacewidget.cpp', '/qtplugins/openbabel/', '/qtplugins/forcefield/',
            '/qtplugins/quantuminput/', '/qtplugins/playertool/', '/molequeue/']
compiled = '\n'.join(line for line in (libs / 'build.ninja').read_text().splitlines()
                     if line.startswith('build ') and 'CXX_COMPILER' in line)
for source in excluded:
    assert source not in compiled, f'Disabled source compiled: {source}'
plugins = re.findall(r'Q_IMPORT_PLUGIN\((\w+)Factory\)',
                    (libs / 'avogadro/qtplugins/avogadrostaticqtplugins.h').read_text())
assert set(plugins) == {'Navigator', 'Editor', 'Selection', 'BallStick'}, plugins
for name in ['pythonscript', 'packagemanager', 'interfacescript', 'interfacewidget']:
    assert not (prefix / f'include/avogadro/qtgui/{name}.h').exists(), name
config = (prefix / 'lib/cmake/avogadrolibs/AvogadroLibsConfig.cmake').read_text()
assert re.search(r'set\(AvogadroLibs_ENABLE_SUBPROCESS "OFF"\)', config)
assert re.search(r'set\(AvogadroLibs_WEB_EDITOR "ON"\)', config)
exports = (prefix / 'lib/cmake/avogadrolibs/AvogadroLibsTargets.cmake').read_text()
for name in ['Avogadro::MoleQueue', 'Avogadro::OpenBabel', 'Avogadro::ForceField', 'LibArchive']:
    assert name not in exports, name
for source in ['/avogadro/mainwindow.cpp', '/avogadro/rpc/', 'rpclistener.cpp']:
    assert source not in (app / 'build.ninja').read_text(), source
for target in ['libarchive', 'molequeue', 'jkqtplotter', 'qttesting']:
    assert not re.search(rf'^build {target}:', (build / 'build.ninja').read_text(), re.M), target
print('PASS: four plugins; no disabled sources, headers, exports, external projects or desktop RPC')
