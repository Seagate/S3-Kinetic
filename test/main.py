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

sys.path.append('../s3cmd')

import S3.Exceptions
from S3.ExitCodes import *

import gvars as g
#import utils as u
import tester as t
import fsTester as ft
import s3cmdTester as s3t
import regressionTester as rt


count_pass = 0
count_fail = 0
count_skip = 0

def prepareTestSuite():
    # Unpack testsuite/ directory
    if not os.path.isdir('testsuite') and os.path.isfile('testsuite.tar.gz'):
        os.system("tar -xz -f testsuite.tar.gz")

    # Verify existence of testsuite directory
    if not os.path.isdir('testsuite'):
        print("Something went wrong while unpacking testsuite.tar.gz")
        return False 

    os.system("tar -xf testsuite/checksum.tar -C testsuite")
    if not os.path.isfile('testsuite/checksum/cksum33.txt'):
        print("Something went wrong while unpacking testsuite/checkum.tar")
        return False 

    ## Fix up permissions for permission-denied tests
    os.chmod("testsuite/permission-tests/permission-denied-dir", 0o444)
    os.chmod("testsuite/permission-tests/permission-denied.txt", 0o000)

    if not os.path.isdir('testsuite/crappy-file-name'):
        os.system("tar xvz -C testsuite -f testsuite/crappy-file-name.tar.gz")
        # TODO: also unpack if the tarball is newer than the directory timestamp
        #       for instance when a new version was pulled from SVN.

    return True 

def usage():
    print("Usage:")
    print("-h, --help\n\t\tPrint this usage information")
    print("-t, --test list of test\n\t\tfs,regression,s3,all.  Default is all")
    print("-c, --config configFile\n\t\tSet config file")
    print("-p, --bucket-prefix bucketPrefix\n\t\tSet bucket prefix")
    print("-v, --vebose\n\t\tSet verbose on/off")
    print("\nExamples:")
    print("To test only fs and regression\n\t\t%s -t fs,regression" % (sys.argv[0]))

def parseArgs(gVars):
    argv = sys.argv[1:]
    while argv:
        arg = argv.pop(0)
        if arg in ("-t", "--test"):
            tests = argv.pop(0)
            print(type(tests))
            print("tests: ", tests)
            gVars.setTest(tests.split(','))
            continue

        if arg.startswith('--bucket-prefix='):
            print("Usage: '--bucket-prefix PREFIX', not '--bucket-prefix=PREFIX'")
            sys.exit(0)
        if arg in ("-h", "--help"):
            usage()
            sys.exit(0)
        if arg in ("-c", "--config"):
            config_file = argv.pop(0)
            gVars.setConfigFile(config_file)
            continue
        if arg in ("-v", "--verbose"):
            gVars.setVerbose(True)
            continue
        if arg in ("-p", "--bucket-prefix"):
            try:
                bucket_prefix = argv.pop(0)
                gVars.setBucketPrefix(bucket_prefix)
            except IndexError:
                print("Bucket prefix option must explicitly supply a bucket name prefix")
                sys.exit(0)
            continue
        if arg in ("-l", "--list"):
            exclude_tests = range(0, 999)
            break
        '''
        if ".." in arg:
            range_idx = arg.find("..")
            range_start = arg[:range_idx] or 0
            range_end = arg[range_idx+2:] or 999
            run_tests.extend(range(int(range_start), int(range_end) + 1))
        elif False: #arg.startswith("-"):
            exclude_tests.append(int(arg[1:]))
        else:
            pass #run_tests.append(int(arg))
        '''

def main():
    if not prepareTestSuite():
        sys.exit(1)

    gVars = g.GlobalVars()
    parseArgs(gVars)

    for test in gVars.tests():
        if test == "fs":
            tester = ft.FSTester(gVars)
        elif test == 's3cmd':
            tester = s3t.S3cmdTester(gVars)
        elif test == 'regression':
            tester = rt.RegressionTester(gVars)
        else:
            print("Invalid test: ", test)
            continue

        tester.test()
 
if __name__ == "__main__":
    main()


     
