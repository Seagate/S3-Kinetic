#!/usr/bin/env python
# -*- coding: utf-8 -*-

from __future__ import absolute_import, print_function

import argparse
import sys
import os

sys.path.append('../s3cmd')

from gvars import GlobalVars 
from fsTester import FSTester
from s3cmdTester import S3cmdTester
from regressionTester import RegressionTester

gAllTests = ("fs", "s3cmd", "regression") 

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

    return True 

def setArgs():
    global gAllTests
    tests = list(gAllTests)
    tests.append('all')
    parser = argparse.ArgumentParser()
    parser.add_argument('-t', '--test', default='all',
        choices=tests, nargs='*',
        help='tests to be performed: ' + str(tests) + ' (default: all)')
    parser.add_argument('-c', '--config', help='s3cmd config file path (default: user s3cmd config)')
    parser.add_argument('-v', '--verbose', action="store_true", default=False,
                        help='turn on verbose')
    parser.add_argument('-b', "--bucketprefix", help='bucket prefix')
    return parser
    
def main():
    parser = setArgs()
    args = parser.parse_args()
    if 'all' in args.test:
        #args.test.clear()
        args.test = gAllTests

    print("%s\n" % (args))
    gVars = GlobalVars()
    gVars.setConfig(args.config)
    gVars.setBucketPrefix(args.bucketprefix)
    gVars.setVerbose(args.verbose)
    print(gVars)
    
    if not prepareTestSuite():
        sys.exit(1)

    for test in args.test:
        if test == "fs":
            tester = FSTester(gVars)
        elif test == 's3cmd':
            tester = S3cmdTester(gVars)
        elif test == 'regression':
            tester = RegressionTester(gVars)
        else:
            print("Invalid test: ", test)
            continue

        tester.test()
 
if __name__ == "__main__":
    main()


     
