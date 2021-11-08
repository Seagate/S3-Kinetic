#!/usr/bin/env python
# -*- coding: utf-8 -*-

import os
import time
import S3.Config
from S3.ExitCodes import *

import gvars as g
import utils as u
import fs
#import s3cmdTester as t
import tester

class RegressionTester(tester.Tester):

    def __init__(self, gVars):
        super().setName("Regression")
        super().setGVars(gVars)

    def test(self):
        super().test()
        self.testValFileSpanTwoZones()
        super().complete()

    def testValFileSpanTwoZones(self):
        util = u.Util(self.gVars())
        os.system('mkdir -p testsuite-out')
        os.system('dd if=/dev/urandom of=testsuite-out/urandom.bin bs=1M count=16 > /dev/null 2>&1')

        self.s3cmd(None, ['mb', '--bucket-location=EU', util.pbucket(1)],
            must_find = "Bucket '%s/' created" % util.pbucket(1))


        ## ====== Multipart put from stdin
        f = open('testsuite-out/urandom.bin', 'r')
        self.s3cmd("Value file spans two zones", ['put', '--multipart-chunk-size-mb=5',
            'testsuite-out/urandom.bin', '%s/urandom.bin' % util.pbucket(1)],
        must_find = ['%s/urandom.bin' % util.pbucket(1)],
        must_not_find = ['abortmp'],
        stdin = f, complete=False, first=True)
        f.close()

        for i in range(51):
            ## ====== Multipart put from stdin
            f = open('testsuite-out/urandom.bin', 'r')
            self.s3cmd("Value file spans two zones", ['put', '--multipart-chunk-size-mb=5',
                'testsuite-out/urandom.bin', '%s/urandom.bin' % util.pbucket(1)],
            must_find = ['%s/urandom.bin' % util.pbucket(1)],
            must_not_find = ['abortmp'],
            stdin = f, complete=False, first=False)
            f.close()

        f = open('testsuite-out/urandom.bin', 'r')
        self.s3cmd("Value file spans two zones", ['put', '--multipart-chunk-size-mb=5',
            'testsuite-out/urandom.bin', '%s/urandom.bin' % util.pbucket(1)],
        must_find = ['%s/urandom.bin' % util.pbucket(1)],
        must_not_find = ['abortmp'],
        stdin = f, first=False)
        f.close()
