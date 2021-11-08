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
#import fs

try:
    unicode
except NameError:
    # python 3 support
    # In python 3, unicode -> str, and str -> bytes
    unicode = str

class Util:
    __gVars = None

    def __init__(self, gVars):
        self.__gVars = gVars

    # helper functions for generating bucket names
    def bucket(self, tail):
        '''Test bucket name'''
        label = 'autotest'
        if str(tail) == '3':
                label = 'autotest'
        return '%ss3cmd-%s-%s' % (self.__gVars.bucketPrefix(), label, tail)

    def pbucket(self, tail):
        '''Like bucket(), but prepends "s3://" for you'''
        return 's3://' + self.bucket(tail)

def unicodise(string, encoding = "utf-8", errors = "replace"):
    '''
    Convert 'string' to Unicode or raise an exception.
    Config can't use toolbox from Utils that is itself using Config
    ''' 
    if type(string) == unicode:
        return string

    try:
        return unicode(string, encoding, errors)
    except UnicodeDecodeError:
        raise UnicodeDecodeError("Conversion to unicode failed: %r" % string)

def which(program):
    def is_exe(fpath):
        return os.path.isfile(fpath) and os.access(fpath, os.X_OK)

    fpath, fname = os.path.split(program)
    if fpath:
        if is_exe(program):
            return program
    else:
        for path in os.environ["PATH"].split(os.pathsep):
            path = path.strip('"')
            exe_file = os.path.join(path, program)
            if is_exe(exe_file):
                return exe_file

    return None
