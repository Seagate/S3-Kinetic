#!/usr/bin/env python
# -*- coding: utf-8 -*-

from __future__ import absolute_import, print_function

import sys
import os
import re
import time
from subprocess import Popen, PIPE, STDOUT
import locale
import getpass

import S3.Exceptions
from S3.ExitCodes import *

import gvars as g
import utils as u
import tester

class FSTester(tester.Tester):
    def __init__(self, gVars):
        super().setName("Standard File System")
        super().setGVars(gVars)


    def mkdir(self, label, dir_name):
        if os.name in ("posix", "nt"):
            cmd = ['mkdir', '-p']
        else:
            print("Unknown platform: %s" % os.name)
            sys.exit(1)
        cmd.append(dir_name)
        return tester.execute(self.gVars(), label, cmd)

    def rmdir(self, label, dir_name):
        if os.path.isdir(dir_name):
            if os.name == "posix":
                cmd = ['rm', '-rf']
            elif os.name == "nt":
                cmd = ['rmdir', '/s/q']
            else:
                print("Unknown platform: %s" % os.name)
                sys.exit(1)
            cmd.append(dir_name)
            return tester.execute(self.gVars(), label, cmd)
        else:
            return tester.execute(self.gVars(), label, [])

    def copy(self, label, src_file, dst_file):
        if os.name == "posix":
            cmd = ['cp', '-f']
        elif os.name == "nt":
            cmd = ['copy']
        else:
            print("Unknown platform: %s" % os.name)
            sys.exit(1)
        cmd.append(src_file)
        cmd.append(dst_file)
        return tester.execute(self.gVars(), label, cmd)

    def _test(self):
        ## ====== Make destination dir for get
        self.mkdir("Make dst dir for get", "testsuite-out")

        self.mkdir("Create cache dir", "testsuite/cachetest/content")
        with open("testsuite/cachetest/content/testfile", "w"):
            pass

        ## ====== Create dir with name of a file
        self.mkdir("Create file-dir dir", "testsuite-out/xyz/dir-test/file-dir")
