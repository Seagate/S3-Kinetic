#!/usr/bin/env python
# -*- coding: utf-8 -*-

import os
import time
import S3.Config
from S3.ExitCodes import *

from utils import Util
from tester import Tester

class RegressionTester(Tester):

    def __init__(self, gVars):
        super().setName("Regression")
        super().setGVars(gVars)

    def test(self):
        super().test()
        self.testValFileSpanTwoZones()
        super().complete()

    def testValFileSpanTwoZones(self):
        util = Util(self.gVars())
        bucket = util.pbucket(1)
        self.s3cmd(None, ['mb', '--bucket-location=EU', bucket],
            must_find = "Bucket '%s/' created" % bucket)
        label = "Value file spans two zones"
        self.putMultiPartObj(label, complete=False)
        for i in range(51):
            self.putMultiPartObj(label, complete=False, first=False)
        self.putMultiPartObj(label, first=False)
