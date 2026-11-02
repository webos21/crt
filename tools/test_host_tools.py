"""Host-tools assembler contracts; fixtures do not execute native programs."""
import hashlib
import json
import os
from pathlib import Path
import shutil
import struct
import sys
import tempfile
import unittest
from unittest.mock import patch
sys.path.insert(0,str(Path(__file__).parent))
import create_host_tools as producer


class HostTools(unittest.TestCase):
    def setUp(self):
        self.tmp=tempfile.TemporaryDirectory();self.addCleanup(self.tmp.cleanup)
        self.root=Path(self.tmp.name);self.sdk=self.root/'sdk';self.sdk.mkdir();self.output=self.root/'output'
        self.host={'os':'linux','arch':'x86_64'}
        mocked=patch.object(producer,'native_host',return_value=self.host);mocked.start();self.addCleanup(mocked.stop)
        (self.sdk/'manifest.json').write_text(json.dumps({'target':self.host,'source_commit':'a'*40}))
        (self.sdk/'VERSION').write_text('fixture-sdk\n');(self.sdk/'LICENSE.md').write_text('fixture notice')
        image=bytearray(64);image[:6]=b'\x7fELF\x02\x01';struct.pack_into('<HHI',image,16,2,62,1)
        for directory in ('system/bin','bin','usr/bin'):
            d=self.sdk/directory;d.mkdir(parents=True)
            for name in (['mksh','sh','toybox','make','awk',*producer.TOYBOX_APPLETS] if directory=='system/bin' else ['mksh','sh','make','awk']):
                p=d/name;p.write_bytes(image);p.chmod(0o755)
        (self.sdk/'system/etc').mkdir();(self.sdk/'system/etc/mkshrc').write_text('fixture')
        for name in ('crt-libc','llvm-runtimes','mksh','toybox','onetrue-awk'):
            for prefix in ('share/licenses','share/crt/dependencies'):
                p=self.sdk/prefix/name;p.mkdir(parents=True);(p/'NOTICE').write_text('fixture '+name)
        (self.sdk/'porting/recipes').mkdir(parents=True)
        shutil.copy2(producer.ROOT/'porting/recipes/make.json',self.sdk/'porting/recipes/make.json')

    def assemble(self):
        return producer.assemble(self.sdk,self.output,'fixture-1')

    def test_bundle_is_sdk_independent_and_inventory_binds_aliases(self):
        result=self.assemble();before=producer.payload_inventory(self.output)
        shutil.rmtree(self.sdk)
        self.assertEqual(before,producer.payload_inventory(self.output))
        self.assertEqual(result['content_sha256'],producer.canonical_hash(before))
        self.assertIn('system/bin/printf',result['executable_files'])
        self.assertFalse((self.output/'include').exists());self.assertFalse((self.output/'crt-toolchain.cmake').exists())
        alias=self.output/'system/bin/printf';alias.write_bytes(b'changed')
        self.assertNotEqual(before,producer.payload_inventory(self.output))

    def test_wrong_host_or_script_is_rejected_before_publication(self):
        p=self.sdk/'manifest.json';p.write_text(json.dumps({'target':dict(self.host,arch='aarch64')}))
        with self.assertRaisesRegex(ValueError,'execution host'):self.assemble()
        p.write_text(json.dumps({'target':self.host}))
        (self.sdk/'system/bin/mksh').write_text('#!/bin/sh\n')
        with self.assertRaisesRegex(ValueError,'native host tool'):self.assemble()
        self.assertFalse(self.output.exists())

    def test_modified_alias_or_notice_does_not_publish_partial_bundle(self):
        alias=self.sdk/'system/bin/printf';original=alias.read_bytes();alias.write_bytes(b'polluted')
        with self.assertRaisesRegex(ValueError,'dispatcher'):self.assemble()
        self.assertFalse(self.output.exists());self.assertEqual([],list(self.root.glob('.host-tools-*')))
        alias.write_bytes(original)
        shutil.rmtree(self.sdk/'share/licenses/mksh')
        with self.assertRaisesRegex(ValueError,'notices/provenance'):self.assemble()
        self.assertFalse(self.output.exists())

    def test_named_output_is_immutable(self):
        self.assemble();original=(self.output/'manifest.json').read_bytes()
        with self.assertRaisesRegex(ValueError,'already exists'):self.assemble()
        self.assertEqual(original,(self.output/'manifest.json').read_bytes())

    def test_escaped_link_is_rejected(self):
        alias=self.sdk/'system/bin/printf';alias.unlink();outside=self.root/'outside';outside.write_bytes((self.sdk/'system/bin/toybox').read_bytes())
        alias.symlink_to(outside)
        with self.assertRaises(ValueError):self.assemble()
        self.assertFalse(self.output.exists())

    def test_removed_execute_permission_is_rejected(self):
        if os.name=='nt':self.skipTest('POSIX execute bits')
        (self.sdk/'system/bin/printf').chmod(0o644)
        with self.assertRaisesRegex(ValueError,'dispatcher'):self.assemble()

    def test_notice_pin_rejects_changed_copying(self):
        source=self.root/'make';source.mkdir();(source/'COPYING').write_text('untrusted stale cache')
        with self.assertRaisesRegex(ValueError,'notice.*pin'):
            producer.assemble(self.sdk,self.output,'fixture',source)

    def test_output_parent_link_cannot_write_into_immutable_sdk(self):
        link=self.root/'alias';link.symlink_to(self.sdk,target_is_directory=True)
        with self.assertRaisesRegex(ValueError,'disjoint'):
            producer.assemble(self.sdk,link/'output','fixture')
        self.assertFalse((self.sdk/'output').exists())

    def test_elf_loader_is_declared_separately_from_needed_libraries(self):
        loader='/lib64/ld-linux-x86-64.so.2'
        with patch.object(producer,'interpreter',return_value=loader):result=self.assemble()
        self.assertEqual(set(result['interpreters'].values()),{loader})
        self.assertEqual(next(p for p in result['external_prerequisites'] if p['id']=='linux-elf-loader')['paths'],[loader])
        shutil.rmtree(self.output)
        with patch.object(producer,'interpreter',return_value='/private/foreign-loader'):
            with self.assertRaisesRegex(ValueError,'interpreter'):self.assemble()
        self.assertFalse(self.output.exists())
        self.assertFalse(self.output.exists())

if __name__=='__main__':unittest.main()
