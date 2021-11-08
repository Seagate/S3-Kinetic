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
import gvars as g
import fs

class Tester:
    __name = "Tester"
    __gVars = None

    def __init__(self, gVars):
        self.__gVars = gVars

    def setGVars(self, gVars):
        self.__gVars = gVars 

    def gVars(self):
        return self.__gVars

    def setName(self, name):
        self.__name = name

    def name(self):
        return self.__name

    def prepare(self):
        print()
        util = u.Util(self.gVars())
        fSys = fs.FS(self.gVars())

        self.gVars().setTestFailed(0)
        self.gVars().setTestSucceeded(0)
        ##========  Test prepration
        fillChar = '='
        width = 40
        label = self.name() + " Test"
        sFill = fillChar*int(((width - len(label) - 2)/2))
        print("\n%s %s %s" % (sFill, label, sFill)) 
        self.clearAll()
        self.init()

    def complete(self):
        width = 40
        label = self.name() + " Test"
        label = "End of " + self.name() + " test" 
        sFill = '='*int(((width - len(label) - 2)/2)) 
        print("%s %s %s" % (sFill, label, sFill)) 


    def clearAll(self):
        success = True
        fSystem = fs.FS(self.gVars())
        util = u.Util(self.gVars())
        #print("\x1b[32;1mClear all...\x1b[0m")
        print(("%s " % ("Clear all")).ljust(30, "."), end=' ')
        # Remove test output directory
        fSystem.rmdir(self.gVars().outDir())

        # Remove all buckets
        self.s3cmd(None, ['rb', '-r', '--force', util.pbucket(1), util.pbucket(2), util.pbucket(3)])
        # Verify they were removed
        self.s3cmd(None, ['ls'],
               must_not_find = [util.pbucket(1), util.pbucket(2), util.pbucket(3)])


        message = ""
        #print("\x1b[31;1mClear all...OK\x1b[0m")
        print("\x1b[32;1mOK\x1b[0m%s" % (message))

    def init(self):
        success = True
        fSystem = fs.FS(self.gVars())
        print(("%s " % ("Initialize")).ljust(30, "."), end=' ')
        success = fSystem.mkdir(self.gVars().outDir())

        message = ""
        print("\x1b[32;1mOK\x1b[0m%s" % (message))
        return success
        
    def s3cmd(self, label, cmd_args = [], **kwargs):
        if not cmd_args[0].endswith("s3cmd"):
            cmd_args.insert(0, "python")
            cmd_args.insert(1, "../s3cmd/s3cmd")
        if self.gVars().configFile():
            cmd_args.insert(2, "-c")
            cmd_args.insert(3, self.gVars().configFile())

        return execute(self.gVars(), label, cmd_args, **kwargs)

def execute(gVars, label, cmd_args = [], retcode = 0, must_find = [],
    must_not_find = [], must_find_re = [], must_not_find_re = [], stdin = None,
    complete=True, first=True):

    def command_output():
        print("----")
        print(" ".join([" " in arg and "'%s'" % arg or arg for arg in cmd_args]))
        print("----")
        print(stdout)
        print("----")

    def failure(message = ""):
        global count_fail
        if message:
            message = u"  (%r)" % message
        print(u"\x1b[31;1mFAIL%s\x1b[0m" % (message))
        gVars.incTestFailed()
        command_output()
        #return 1
        sys.exit(1)

    def success(message = ""):
        global count_pass
        if label and complete:
            gVars.incTestSucceeded()
            if message:
                message = "  (%r)" % message
            print("\x1b[32;1mOK\x1b[0m%s" % (message))

        if gVars.verbose():
            command_output()
        return 0

    def skip(message = ""):
        global count_skip
        if message != None:
            if message:
                message = "  (%r)" % message
            print("\x1b[33;1mSKIP\x1b[0m%s" % (message))
        #count_skip += 1
        return 0

    def compile_list(_list, regexps = False):
        if regexps == False:
            _list = [re.escape(item) for item in _list]

        return [re.compile(item, re.MULTILINE) for item in _list]

    if label and first:
        print(("%3d  %s " % (gVars.numTests()+1, label)).ljust(30, "."), end=' ')
        sys.stdout.flush()

    if False: #run_tests.count(gVars.testCount()) == 0 or exclude_tests.count(gVars.testCount()) > 0:
        return skip()

    if not cmd_args:
        return skip(label)

    p = Popen(cmd_args, stdin = stdin, stdout = PIPE, stderr = STDOUT,
              universal_newlines = True, close_fds = True)
    stdout, stderr = p.communicate()

    if type(retcode) not in [list, tuple]: retcode = [retcode]
    if p.returncode not in retcode:
        return failure("retcode: %d, expected one of: %s" % (p.returncode, retcode))

    if type(must_find) not in [ list, tuple ]: must_find = [must_find]
    if type(must_find_re) not in [ list, tuple ]: must_find_re = [must_find_re]
    if type(must_not_find) not in [ list, tuple ]: must_not_find = [must_not_find]
    if type(must_not_find_re) not in [ list, tuple ]: must_not_find_re = [must_not_find_re]

    find_list = []
    find_list.extend(compile_list(must_find))
    find_list.extend(compile_list(must_find_re, regexps = True))
    find_list_patterns = []
    find_list_patterns.extend(must_find)
    find_list_patterns.extend(must_find_re)

    not_find_list = []
    not_find_list.extend(compile_list(must_not_find))
    not_find_list.extend(compile_list(must_not_find_re, regexps = True))
    not_find_list_patterns = []
    not_find_list_patterns.extend(must_not_find)
    not_find_list_patterns.extend(must_not_find_re)
    for index in range(len(find_list)):
        stdout = u.unicodise(stdout)
        match = find_list[index].search(stdout)
        if not match:
            return failure("pattern not found: %s" % find_list_patterns[index])
    for index in range(len(not_find_list)):
        match = not_find_list[index].search(stdout)
        if match:
            return failure("pattern found: %s (match: %s)" % (not_find_list_patterns[index], match.group(0)))

    return success()

