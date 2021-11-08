#!/usr/bin/env python
# -*- coding: utf-8 -*-

import os
import time
import S3.Config
from S3.ExitCodes import *

import gvars as g
import utils as u
import tester
import fs

class CurlTester(tester.Tester):

    def __init__(self, gVars):
        super().setName("Curl")
        super().setGVars(gVars)

    def testCurlHEAD(self, label, src_file, **kwargs):
        cmd = ['curl', '--silent', '--head', '-include', '--location']
        cmd.append(src_file)
        return u.execute(self.__gVars, label, cmd, **kwargs)

    def test(self):
        if not self.gVars().haveCurl():
            return

        util = u.Util(self.gVars())
        fSys = fs.FS(self.gVars())

        cfg = S3.Config.Config(self.gVars().configFile())
        '''
        ## ====== Retrieve from URL
        u.s3cmd(self.__gVars, None, ['put', '--guess-mime-type', '--acl-public',
            'testsuite/etc/logo.png', '%s/xyz/etc/logo.png' % util.pbucket(1)],
            must_find = [ "-> '%s/xyz/etc/logo.png'" % util.pbucket(1) ])
        #skip
        self.testCurlHEAD("Retrieve from URL", 'http://%s.%s/xyz/etc/logo.png' % (util.bucket(1), cfg.host_base),
            must_find_re = ['Content-Length: 22059'])
        ## ====== Verify Private ACL
        #skip
        self.testCurlHEAD("Verify Private ACL", 'http://%s.%s/xyz/etc/logo.png' % (util.bucket(1), cfg.host_base),
            must_find_re = [ '403 Forbidden' ])

        ## ====== Verify Public ACL
        #skip
        self.testCurlHEAD("Verify Public ACL", 'http://%s.%s/xyz/etc/logo.png' % (util.bucket(1), cfg.host_base),
            must_find_re = [ '200 OK',
                                            'Content-Length: 22059'])
        self.testCurlHEAD("HEAD check Cache-Control present", 'http://%s.%s/copy/etc2/Logo.PNG' % (util.bucket(2), cfg.host_base),
                           must_find_re = [ "Cache-Control: max-age=3600" ])

        self.testCurlHEAD("HEAD check Cache-Control not present", 'http://%s.%s/copy/etc2/Logo.PNG' % (util.bucket(2), cfg.host_base),
                           must_not_find_re = [ "Cache-Control: max-age=3600" ])
        '''
        '''
        ## ====== Get current expiration setting
        u.s3cmd(self.__gVars, "Get current expiration setting", ['info', util.pbucket(1)],
            must_find = [ "Expiration Rule: all objects in this bucket will expire in '2020-12-31T00:00:00.000Z'"])
    
        ## ====== set Requester Pays flag
        u.s3cmd(self.__gVars, "Set requester pays", ['payer', '--requester-pays', util.pbucket(2)])

        ## ====== get Requester Pays flag
        u.s3cmd(self.__gVars, "Get requester pays flag", ['info', util.pbucket(2)],
            must_find = [ "Payer:     Requester"])

        ## ====== ls using Requester Pays flag
        u.s3cmd(self.__gVars, "ls using requester pays flag", ['ls', '--requester-pays', util.pbucket(2)])

        ## ====== clear Requester Pays flag
        u.s3cmd(self.__gVars, "Clear requester pays", ['payer', util.pbucket(2)])

        ## ====== get Requester Pays flag
        u.s3cmd(self.__gVars, "Get requester pays flag", ['info', util.pbucket(2)],
            must_find = [ "Payer:     BucketOwner"])

        ## ====== Change ACL to Private
        u.s3cmd(self.__gVars, "Change ACL to Private", ['setacl', '--acl-private', '%s/xyz/etc/l*.png' % util.pbucket(1)],
            must_find = [ "logo.png: ACL set to Private" ])

        ## ====== Change ACL to Public
        u.s3cmd(self.__gVars, "Change ACL to Public", ['setacl', '--acl-public', '--recursive',
            '%s/xyz/etc/' % util.pbucket(1) , '-v'],
            must_find = [ "logo.png: ACL set to Public" ])
        ## ====== Verify ACL and MIME type
        u.s3cmd(self.__gVars, "Verify ACL and MIME type", ['info', '%s/copy/etc2/Logo.PNG' % util.pbucket(2) ],
            must_find_re = [ "MIME type:.*image/png",
                             "ACL:.*\*anon\*: READ",
                             "URL:.*https?://%s.%s/copy/etc2/Logo.PNG" % (bucket(2), cfg.host_base) ])
        u.s3cmd(self.__gVars, "Verify ACL and MIME type", ['info', '%s/copy/etc2/Logo.PNG' % util.pbucket(2) ],
            must_find_re = [ "MIME type:.*binary/octet-stream",
                             "ACL:.*\*anon\*: READ",
                             "URL:.*https?://%s.%s/copy/etc2/Logo.PNG" % (util.bucket(2), cfg.host_base) ])

        #u.s3cmd(self.__gVars, "Modify MIME type back", ['modify', '--mime-type=image/png', '%s/copy/etc2/Logo.PNG' % util.pbucket(2) ])
        u.s3cmd(self.__gVars, "Verify ACL and MIME type", ['info', '%s/copy/etc2/Logo.PNG' % util.pbucket(2) ],
            must_find_re = [ "MIME type:.*image/png",
                             "ACL:.*\*anon\*: READ",
                             "URL:.*https?://%s.%s/copy/etc2/Logo.PNG" % (util.bucket(2), cfg.host_base) ])
        '''
