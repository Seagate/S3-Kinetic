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
import S3.Config
from S3.ExitCodes import *

import utils as u
import tester as tst 

class FS:
    def __init__(self, gvars):
        self.__gvars = gvars

    def gvars(self, gvars):
        self.__gvars = gvars

    def mkdir(self, dir_name, label=None):
        if os.name in ("posix", "nt"):
            cmd = ['mkdir', '-p']
        else:
            print("Unknown platform: %s" % os.name)
            return False
        cmd.append(dir_name)
        return tst.execute(self.__gvars, label, cmd)

    def rmdir(self, dir_name, label=None):
        if os.path.isdir(dir_name):
            if os.name == "posix":
                cmd = ['rm', '-rf']
            elif os.name == "nt":
                cmd = ['rmdir', '/s/q']
            else:
                print("Unknown platform: %s" % os.name)
                return False
            cmd.append(dir_name)
            return tst.execute(self.__gvars, label, cmd)
        else:
            return tst.execute(self.__gvars, label, [])

    def copy(self, src_file, dst_file, label = None):
        if os.name == "posix":
            cmd = ['cp', '-f']
        elif os.name == "nt":
            cmd = ['copy']
        else:
            print("Unknown platform: %s" % os.name)
            return False
        cmd.append(src_file)
        cmd.append(dst_file)
        return tst.execute(self.__gvars, label, cmd)

