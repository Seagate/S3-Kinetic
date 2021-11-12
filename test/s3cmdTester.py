#!/usr/bin/env python
# -*- coding: utf-8 -*-

import os
import time
import S3.Config
from S3.ExitCodes import *

from utils import Util
from fs import FS
from tester import Tester

class S3cmdTester(Tester):
    def __init__(self, gVars):
        super().setGVars(gVars)
        super().setName("S3cmd")

    def _test(self):
        util = Util(self.gVars())
        fSys = FS(self.gVars())

        ## ====== Create one bucket (EU)
        self.s3cmd("Create one bucket (EU)", ['mb', '--bucket-location=EU', util.pbucket(1)],
            must_find = "Bucket '%s/' created" % util.pbucket(1))

        ## ====== Create multiple buckets
        self.s3cmd("Create multiple buckets", ['mb', util.pbucket(2), util.pbucket(3)],
            must_find = [ "Bucket '%s/' created" % util.pbucket(2), "Bucket '%s/' created" % util.pbucket(3)])

        ## ====== Invalid bucket name
        self.s3cmd("Invalid bucket name", ["mb", "--bucket-location=EU", util.pbucket('EU')],
            retcode = EX_USAGE,
            must_find_re = "ERROR:(.*)Parameter problem: Bucket name '%s' contains disallowed character" % util.bucket('EU'),
            must_not_find_re = "Bucket.*created")

        ## ====== Buckets list
        self.s3cmd("Buckets list", ["ls"],
            must_find = [ util.pbucket(1), util.pbucket(2), util.pbucket(3) ], must_not_find_re = util.pbucket('EU'))

        fSys.rmdir("testsuite/cachetest")
        fSys.mkdir("testsuite/cachetest")

        ## ====== Sync to S3
        self.s3cmd("Sync to S3", ['sync', 'testsuite/', util.pbucket(1) + '/xyz/', '--exclude',
            'demo/*', '--exclude', '*.png', '--no-encrypt', '--exclude-from', 'testsuite/exclude.encodings',
            '--exclude', 'testsuite/cachetester..s3cmdcache', '--cache-file', 'testsuite/cachetest/.s3cmdcache'],
            must_find_re = ["ERROR:(.*:)Upload of 'testsuite/permission-tests/permission-denied.txt' is not possible \(Reason: Permission denied\)",
                            "WARNING:(.*):32 non-printable characters replaced in: crappy-file-name/non-printables",],
            must_not_find_re = ["demo/", "^(?!WARNING:(.*):Skipping).*\.png$", "permission-denied-dir"],
            retcode = EX_PARTIAL)

        ## ====== Create new file and sync with caching enabled
        fSys.mkdir("testsuite/cachetester.content")
        with open("testsuite/cachetester.content/tester.ile", "w"):
            pass

        self.s3cmd("Sync to S3 with caching", ['sync', 'testsuite/', util.pbucket(1) + '/xyz/',
            '--exclude', 'demo/*', '--exclude', '*.png', '--no-encrypt', '--exclude-from',
            'testsuite/exclude.encodings', '--exclude', 'cachetester..s3cmdcache', '--cache-file', 'testsuite/cachetester..s3cmdcache' ],
            must_find = "upload: 'testsuite/cachetester.content/tester.ile' -> '%s/xyz/cachetester.content/tester.ile'" % util.pbucket(1),
            must_not_find = "upload 'testsuite/cachetester..s3cmdcache'",
            retcode = EX_PARTIAL)

        ## ====== Remove content and retry cached sync with --delete-removed
        fSys.rmdir("testsuite/cachetester.content")

        self.s3cmd("Sync to S3 and delete removed with caching", ['sync', 'testsuite/',
            util.pbucket(1) + '/xyz/', '--exclude', 'demo/*', '--exclude', '*.png', '--no-encrypt', '--exclude-from',
            'testsuite/exclude.encodings', '--exclude', 'testsuite/cachetester..s3cmdcache', '--cache-file',
            'testsuite/cachetester..s3cmdcache', '--delete-removed'],
            must_find = "delete: '%s/xyz/cachetester.content/tester.ile'" % util.pbucket(1),
            must_not_find = "dictionary changed size during iteration",
            retcode = EX_PARTIAL)

        encoding = self.gVars().encoding()
        enc_pattern = self.gVars().encodedPattern()

        ## ====== Remove cache directory and file
        fSys.rmdir("testsuite/cachetest")
        if self.gVars().haveEncoding():
            ## ====== Sync UTF-8 / GBK / ... to S3
            self.s3cmd("Sync %s to S3" % encoding, ['sync', 'testsuite/encodings/' + encoding,
            '%s/xyz/encodings/' % util.pbucket(1), '--exclude', 'demo/*', '--no-encrypt' ],
            must_find = [ u"'testsuite/encodings/%(encoding)s/%(pattern)s' -> '%(pbucket)s/xyz/encodings/%(encoding)s/%(pattern)s'" %
                { 'encoding' : encoding, 'pattern' : enc_pattern , 'pbucket' : util.pbucket(1)} ])

        ## ====== List bucket content
        self.s3cmd("List bucket content", ['ls', '%s/xyz/' % util.pbucket(1) ],
            must_find_re = [ u"DIR +%s/xyz/binary/$" % util.pbucket(1) , u"DIR +%s/xyz/etc/$" % util.pbucket(1) ],
            must_not_find = [ u"random-crap.md5", u"/demo" ])

        ## ====== List bucket recursive
        must_find = [ u"%s/xyz/binary/random-crap.md5" % util.pbucket(1) ]
        if self.gVars().haveEncoding():
            must_find.append(u"%(pbucket)s/xyz/encodings/%(encoding)s/%(pattern)s" %
                { 'encoding' : encoding, 'pattern' : enc_pattern, 'pbucket' : util.pbucket(1) })

        self.s3cmd("List bucket recursive", ['ls', '--recursive', util.pbucket(1)],
            must_find = must_find,
            must_not_find = [ "logo.png" ])

        ## ====== FIXME
        self.s3cmd("Recursive put", ['put', '--recursive', 'testsuite/etc', '%s/xyz/' % util.pbucket(1) ])

        ## ====== Put from stdin
        f = open('testsuite/single-file/single-file.txt', 'r')
        self.s3cmd("Put from stdin", ['put', '-', '%s/single-file/single-file.txt' % util.pbucket(1)],
                   must_find = ["'<stdin>' -> '%s/single-file/single-file.txt'" % util.pbucket(1)],
                   stdin = f)
        f.close()

        ## ====== Multipart put
        self.putMultiPartObj("Put multipart")

        ## ====== Multipart put from stdin
        self.putMultiPartObj("Multipart large put from stdin", stdin=True)

        ## ====== Moving things without trailing '/'
        os.system('dd if=/dev/urandom of=testsuite-out/urandom1.bin bs=1k count=1 > /dev/null 2>&1')
        os.system('dd if=/dev/urandom of=testsuite-out/urandom2.bin bs=1k count=1 > /dev/null 2>&1')
        self.s3cmd("Put multiple files",
            ['put', 'testsuite-out/urandom1.bin', 'testsuite-out/urandom2.bin', '%s/' % util.pbucket(1)],
            must_find = ["%s/urandom1.bin" % util.pbucket(1), "%s/urandom2.bin" % util.pbucket(1)])

        self.s3cmd("Move without '/'",
            ['mv', '%s/urandom1.bin' % util.pbucket(1), '%s/urandom2.bin' % util.pbucket(1), '%s/dir' % util.pbucket(1)],
            retcode = 64,
            must_find = ['Destination must be a directory'])

        self.s3cmd("Move recursive w/a '/'",
            ['-r', 'mv', '%s/dir1' % util.pbucket(1), '%s/dir2' % util.pbucket(1)],
            retcode = 64,
            must_find = ['Destination must be a directory'])

        ## ====== Moving multiple files into directory with trailing '/'
        must_find = ["'%s/urandom1.bin' -> '%s/dir/urandom1.bin'" % (util.pbucket(1),util.pbucket(1)),
            "'%s/urandom2.bin' -> '%s/dir/urandom2.bin'" % (util.pbucket(1),util.pbucket(1))]
        must_not_find = ["'%s/urandom1.bin' -> '%s/dir'" % (util.pbucket(1),util.pbucket(1)),
            "'%s/urandom2.bin' -> '%s/dir'" % (util.pbucket(1),util.pbucket(1))]

        self.s3cmd("Move multiple files",
            ['mv', '%s/urandom1.bin' % util.pbucket(1), '%s/urandom2.bin' % util.pbucket(1), '%s/dir/' % util.pbucket(1)],
            must_find = must_find,
            must_not_find = must_not_find)

        ## ====== Sync from S3
        must_find = [ "'%s/xyz/binary/random-crap.md5' -> 'testsuite-out/xyz/binary/random-crap.md5'" % util.pbucket(1) ]

        if self.gVars().haveEncoding():
            must_find.append(u"'%(pbucket)s/xyz/encodings/%(encoding)s/%(pattern)s' -> 'testsuite-out/xyz/encodings/%(encoding)s/%(pattern)s' " %
                { 'encoding' : encoding, 'pattern' : enc_pattern, 'pbucket' : util.pbucket(1) })

        self.s3cmd("Sync from S3", ['sync', '%s/xyz' % util.pbucket(1), 'testsuite-out'],
            must_find = must_find)

        ## ====== Remove 'demo' directory
        fSys.rmdir("testsuite-out/xyz/dir-test/")

        ## ====== Create dir with name of a file
        fSys.mkdir("testsuite-out/xyz/dir-test/file-dir")

        ## ====== Skip dst dirs
        self.s3cmd("Skip over dir", ['sync', '%s/xyz' % util.pbucket(1), 'testsuite-out'],
            must_find = "Download of 'xyz/dir-test/file-dir' failed (Reason: testsuite-out/xyz/dir-test/file-dir is a directory)",
            retcode = EX_PARTIAL)

        ## ====== Put public, guess MIME
        self.s3cmd("Put public, guess MIME", ['put', '--guess-mime-type', '--acl-public',
            'testsuite/etc/logo.png', '%s/xyz/etc/logo.png' % util.pbucket(1)],
            must_find = [ "-> '%s/xyz/etc/logo.png'" % util.pbucket(1) ])

        ## ====== Change ACL to Public
        self.s3cmd("Change ACL to Public", ['setacl', '--acl-public', '--recursive',
            '%s/xyz/etc/' % util.pbucket(1) , '-v'],
            must_find = [ "logo.png: ACL set to Public" ])

        ## ====== Sync more to S3
        self.s3cmd("Sync more to S3", ['sync', 'testsuite/', 's3://%s/xyz/' % util.bucket(1), '--no-encrypt' ],
                   must_find = [ "'testsuite/demo/some-file.xml' -> '%s/xyz/demo/some-file.xml' " % util.pbucket(1) ],
                   must_not_find = [ "'testsuite/etc/linked.png' -> '%s/xyz/etc/linked.png'" % util.pbucket(1) ],
                   retcode = EX_PARTIAL)

        ## ====== Don't check MD5 sum on Sync
        # Keep
        fSys.copy("testsuite/checksum/cksum2.txt", "testsuite/checksum/cksum1.txt")
        fSys.copy("testsuite/checksum/cksum2.txt", "testsuite/checksum/cksum33.txt")

        self.s3cmd("Don't check MD5", ['sync', 'testsuite/', 's3://%s/xyz/' % util.bucket(1),
            '--no-encrypt', '--no-check-md5'],
            must_find = [ "cksum33.txt" ],
            must_not_find = [ "cksum1.txt" ],
            retcode = EX_PARTIAL)

        ## ====== Check MD5 sum on Sync
        self.s3cmd("Check MD5", ['sync', 'testsuite/', 's3://%s/xyz/' % util.bucket(1),
            '--no-encrypt', '--check-md5'],
            must_find = [ "cksum1.txt" ],
            retcode = EX_PARTIAL)

        ## ====== Rename within S3
        self.s3cmd("Rename within S3", ['mv', '%s/xyz/etc/logo.png' % util.pbucket(1),
            '%s/xyz/etc2/Logo.PNG' % util.pbucket(1)],
            must_find = [ "move: '%s/xyz/etc/logo.png' -> '%s/xyz/etc2/Logo.PNG'" % (util.pbucket(1), util.pbucket(1))])

        ## ====== Rename (NoSuchKey)
        self.s3cmd("Rename (NoSuchKey)", ['mv', '%s/xyz/etc/logo.png' % util.pbucket(1),
            '%s/xyz/etc2/Logo.PNG' % util.pbucket(1)],
            retcode = EX_NOTFOUND,
            must_find_re = [ 'Key not found' ],
            must_not_find = [ "move: '%s/xyz/etc/logo.png' -> '%s/xyz/etc2/Logo.PNG'" % (util.pbucket(1), util.pbucket(1)) ])

        ## ====== Sync more from S3 (invalid src)
        self.s3cmd("Sync more from S3 (invalid src)", ['sync', '--delete-removed',
            '%s/xyz/DOESNOTEXIST' % util.pbucket(1), 'testsuite-out'],
            must_not_find = [ "delete: 'testsuite-out/logo.png'" ])

        ## ====== Sync more from S3
        fSys.rmdir("testsuite-out")
        fSys.mkdir("testsuite-out")
        self.s3cmd("Sync more from S3", ['sync', '--delete-removed', '%s/xyz' % util.pbucket(1), 'testsuite-out'],
            must_find = [ "'%s/xyz/etc2/Logo.PNG' -> 'testsuite-out/xyz/etc2/Logo.PNG'" % util.pbucket(1),
                          "'%s/xyz/demo/some-file.xml' -> 'testsuite-out/xyz/demo/some-file.xml'" % util.pbucket(1) ],
            must_not_find_re = [ "not-deleted.*etc/logo.png", "delete: 'testsuite-out/logo.png'" ])

        ## ====== Get multiple files
        fSys.rmdir("testsuite-out")
        self.s3cmd("Get multiple files", ['get', '%s/xyz/etc2/Logo.PNG' % util.pbucket(1),
            '%s/xyz/etc/AtomicClockRadio.ttf' % util.pbucket(1), 'testsuite-out'],
            retcode = EX_USAGE,
            must_find = [ 'Destination must be a directory or stdout when downloading multiple sources.' ])

        ## ====== put/get non-ASCII filenames
        self.s3cmd("Put unicode filenames", ['put', u'testsuite/encodings/UTF-8/ŪņЇЌœđЗ/Žůžo',
            u'%s/xyz/encodings/UTF-8/ŪņЇЌœđЗ/Žůžo' % util.pbucket(1)],
            retcode = 0,
            must_find = [ '->' ])

        ## ====== put/get non-ASCII filenames
        fSys.mkdir("testsuite-out")
        self.s3cmd("Get unicode filenames", ['get', u'%s/xyz/encodings/UTF-8/ŪņЇЌœđЗ/Žůžo' % util.pbucket(1), 'testsuite-out'],
                   retcode = 0,
                   must_find = [ '->' ])

        ## ====== Get multiple files
        self.s3cmd("Get multiple files", ['get', '%s/xyz/etc2/Logo.PNG' % util.pbucket(1), '%s/xyz/etc/AtomicClockRadio.ttf' % util.pbucket(1), 'testsuite-out'],
            must_find = [ u"-> 'testsuite-out/Logo.PNG'",
                          u"-> 'testsuite-out/AtomicClockRadio.ttf'" ])

        ## ====== Upload files differing in capitalisation
        self.s3cmd("blah.txt / Blah.txt", ['put', '-r', 'testsuite/blahBlah', util.pbucket(1)],
            must_find = [ '%s/blahBlah/Blah.txt' % util.pbucket(1), '%s/blahBlah/blah.txt' % util.pbucket(1)])

        ## ====== Copy between buckets
        self.s3cmd("Copy between buckets", ['cp', '%s/xyz/etc2/Logo.PNG' % util.pbucket(1), '%s/xyz/etc2/logo.png' % util.pbucket(3)],
            must_find = [ "remote copy: '%s/xyz/etc2/Logo.PNG' -> '%s/xyz/etc2/logo.png'" % (util.pbucket(1), util.pbucket(3)) ])

        ## ====== Recursive copy
        self.s3cmd("Recursive copy, set ACL", ['cp', '-r', '--acl-public', '%s/xyz/' % util.pbucket(1), '%s/copy/' % util.pbucket(2), '--exclude', 'demo/dir?/*.txt', '--exclude', 'non-printables*'],
            must_find = [ "remote copy: '%s/xyz/etc2/Logo.PNG' -> '%s/copy/etc2/Logo.PNG'" % (util.pbucket(1), util.pbucket(2)),
                          "remote copy: '%s/xyz/blahBlah/Blah.txt' -> '%s/copy/blahBlah/Blah.txt'" % (util.pbucket(1), util.pbucket(2)),
                          "remote copy: '%s/xyz/blahBlah/blah.txt' -> '%s/copy/blahBlah/blah.txt'" % (util.pbucket(1), util.pbucket(2)) ],
            must_not_find = [ "demo/dir1/file1-1.txt" ])

        ## ====== modify MIME type
        self.s3cmd("Modify MIME type", ['modify', '--mime-type=binary/octet-stream', '%s/copy/etc2/Logo.PNG' % util.pbucket(2) ])


        self.s3cmd("Modify MIME type back", ['modify', '--mime-type=image/png', '%s/copy/etc2/Logo.PNG' % util.pbucket(2) ])

        self.s3cmd("Add cache-control header", ['modify', '--add-header=cache-control: max-age=3600, public', '%s/copy/etc2/Logo.PNG' % util.pbucket(2) ],
            must_find_re = [ "modify: .*" ])

        self.s3cmd("Remove cache-control header", ['modify', '--remove-header=cache-control', '%s/copy/etc2/Logo.PNG' % util.pbucket(2) ],
                   must_find_re = [ "modify: .*" ])

        ## ====== sign
        self.s3cmd("sign string", ['sign', 's3cmd'], must_find_re = ["Signature:"])
        self.s3cmd("signurl time", ['signurl', '%s/copy/etc2/Logo.PNG' % util.pbucket(2), str(int(time.time()) + 60)], must_find_re = ["http://"])
        self.s3cmd("signurl time offset", ['signurl', '%s/copy/etc2/Logo.PNG' % util.pbucket(2), '+60'], must_find_re = ["https?://"])
        self.s3cmd("signurl content disposition and type", ['signurl', '%s/copy/etc2/Logo.PNG' % util.pbucket(2), '+60', '--content-disposition=inline; filename=video.mp4', '--content-type=video/mp4'], must_find_re = [ 'response-content-disposition', 'response-content-type' ] )

        ## ====== Rename within S3
        self.s3cmd("Rename within S3", ['mv', '%s/copy/etc2/Logo.PNG' % util.pbucket(2), '%s/copy/etc/logo.png' % util.pbucket(2)],
            must_find = [ "move: '%s/copy/etc2/Logo.PNG' -> '%s/copy/etc/logo.png'" % (util.pbucket(2), util.pbucket(2))])

        ## ====== Sync between buckets
        self.s3cmd("Sync remote2remote", ['sync', '%s/xyz/' % util.pbucket(1), '%s/copy/' % util.pbucket(2), '--delete-removed', '--exclude', 'non-printables*'],
            must_find = [ "remote copy: '%s/xyz/demo/dir1/file1-1.txt' -> '%s/copy/demo/dir1/file1-1.txt'" % (util.pbucket(1), util.pbucket(2)),
                          "remote copy: 'etc/logo.png' -> 'etc2/Logo.PNG'",
                          "delete: '%s/copy/etc/logo.png'" % util.pbucket(2) ],
            must_not_find = [ "blah.txt" ])
        ## ====== Don't Put symbolic link
        self.s3cmd("Don't put symbolic links", ['put', 'testsuite/etc/linked1.png', 's3://%s/xyz/' % util.bucket(1),],
                   retcode = EX_USAGE,
                   must_find_re = ["WARNING:(.*):Skipping over symbolic link: testsuite/etc/linked1.png"],
                   must_not_find_re = ["^(?!WARNING:(.*):Skipping).*linked1.png"])

        ## ====== Put symbolic link
        self.s3cmd("Put symbolic links", ['put', 'testsuite/etc/linked1.png', 's3://%s/xyz/' % util.bucket(1),'--follow-symlinks' ],
                   must_find = [ "'testsuite/etc/linked1.png' -> '%s/xyz/linked1.png'" % util.pbucket(1)])

        ## ====== Sync symbolic links
        self.s3cmd("Sync symbolic links", ['sync', 'testsuite/', 's3://%s/xyz/' % util.bucket(1), '--no-encrypt', '--follow-symlinks' ],
            must_find = ["remote copy: 'etc2/Logo.PNG' -> 'etc/linked.png'"],
                   # Don't want to recursively copy linked directories!
                   must_not_find_re = ["etc/more/linked-dir/more/give-me-more.txt",
                                       "etc/brokenlink.png"],
                   retcode = EX_PARTIAL)

        ## ====== Multi source move
        self.s3cmd("Multi-source move", ['mv', '-r', '%s/copy/blahBlah/Blah.txt' % util.pbucket(2), '%s/copy/etc/' % util.pbucket(2), '%s/moved/' % util.pbucket(2)],
            must_find = [ "move: '%s/copy/blahBlah/Blah.txt' -> '%s/moved/Blah.txt'" % (util.pbucket(2), util.pbucket(2)),
                          "move: '%s/copy/etc/AtomicClockRadio.ttf' -> '%s/moved/AtomicClockRadio.ttf'" % (util.pbucket(2), util.pbucket(2)),
                          "move: '%s/copy/etc/TypeRa.ttf' -> '%s/moved/TypeRa.ttf'" % (util.pbucket(2), util.pbucket(2)) ],
            must_not_find = [ "blah.txt" ])

        ## ====== Verify move
        self.s3cmd("Verify move", ['ls', '-r', util.pbucket(2)],
            must_find = [ "%s/moved/Blah.txt" % util.pbucket(2),
                          "%s/moved/AtomicClockRadio.ttf" % util.pbucket(2),
                          "%s/moved/TypeRa.ttf" % util.pbucket(2),
                          "%s/copy/blahBlah/blah.txt" % util.pbucket(2) ],
            must_not_find = [ "%s/copy/blahBlah/Blah.txt" % util.pbucket(2),
                              "%s/copy/etc/AtomicClockRadio.ttf" % util.pbucket(2),
                              "%s/copy/etc/TypeRa.ttf" % util.pbucket(2) ])

        ## ====== List all
        self.s3cmd("List all", ['la'],
                   must_find = [ "%s/urandom.bin" % util.pbucket(1)])

        ## ====== Simple delete
        self.s3cmd("Simple delete", ['del', '%s/xyz/etc2/Logo.PNG' % util.pbucket(1)],
            must_find = [ "delete: '%s/xyz/etc2/Logo.PNG'" % util.pbucket(1) ])

        ## ====== Simple delete with rm
        self.s3cmd("Simple delete with rm", ['rm', '%s/xyz/test_rm/TypeRa.ttf' % util.pbucket(1)],
            must_find = [ "delete: '%s/xyz/test_rm/TypeRa.ttf'" % util.pbucket(1) ])

        ## ====== Create expiration rule with days and prefix
        # This command randomly fails with low rate. Once happens, it happens consistently
        self.s3cmd("Create expiration rule with days and prefix", ['expire', util.pbucket(1), '--expiry-days=365', '--expiry-prefix=log/'],
            must_find = [ "Bucket '%s/': expiration configuration is set." % util.pbucket(1)])

        ## ====== Create expiration rule with date and prefix
        # This command randomly fails with low rate. Once happens, it happens consistently
        self.s3cmd("Create expiration rule with date and prefix", ['expire', util.pbucket(1), '--expiry-date=2020-12-31T00:00:00.000Z', '--expiry-prefix=log/'],
            must_find = [ "Bucket '%s/': expiration configuration is set." % util.pbucket(1)])

        ## ====== Create expiration rule with days only
        self.s3cmd("Create expiration rule with days only", ['expire', util.pbucket(1), '--expiry-days=365'],
            must_find = [ "Bucket '%s/': expiration configuration is set." % util.pbucket(1)])

        ## ====== Create expiration rule with date only
        self.s3cmd("Create expiration rule with date only", ['expire', util.pbucket(1), '--expiry-date=2020-12-31T00:00:00.000Z'],
            must_find = [ "Bucket '%s/': expiration configuration is set." % util.pbucket(1)])

        ## ====== Delete expiration rule
        self.s3cmd("Delete expiration rule", ['expire', util.pbucket(1)],
            must_find = [ "Bucket '%s/': expiration configuration is deleted." % util.pbucket(1)])
        ## ====== Recursive delete maximum exceeed
        self.s3cmd("Recursive delete maximum exceeded", ['del', '--recursive', '--max-delete=1', '--exclude', 'Atomic*', '%s/xyz/etc' % util.pbucket(1)],
            must_not_find = [ "delete: '%s/xyz/etc/TypeRa.ttf'" % util.pbucket(1) ])

        ## ====== Recursive delete
        self.s3cmd("Recursive delete", ['del', '--recursive', '--exclude', 'Atomic*', '%s/xyz/etc' % util.pbucket(1)],
            must_find = [ "delete: '%s/xyz/etc/TypeRa.ttf'" % util.pbucket(1) ],
            must_find_re = [ "delete: '.*/etc/logo.png'" ],
            must_not_find = [ "AtomicClockRadio.ttf" ])

        ## ====== Recursive delete with rm
        self.s3cmd("Recursive delete with rm", ['rm', '--recursive', '--exclude', 'Atomic*', '%s/xyz/test_rm' % util.pbucket(1)],
            must_find = [ "delete: '%s/xyz/test_rm/more/give-me-more.txt'" % util.pbucket(1) ],
            must_find_re = [ "delete: '.*/test_rm/logo.png'" ],
            must_not_find = [ "AtomicClockRadio.ttf" ])

        ## ====== Recursive delete all
        self.s3cmd("Recursive delete all", ['del', '--recursive', '--force', util.pbucket(1)],
            must_find_re = [ "delete: '.*binary/random-crap'" ])

        ## ====== Remove empty bucket
        self.s3cmd("Remove empty bucket", ['rb', util.pbucket(1)],
            must_find = [ "Bucket '%s/' removed" % util.pbucket(1) ])

        ## ====== Remove remaining buckets
        self.s3cmd("Remove remaining buckets", ['rb', '--recursive', util.pbucket(2), util.pbucket(3)],
            must_find = [ "Bucket '%s/' removed" % util.pbucket(2),
                      "Bucket '%s/' removed" % util.pbucket(3) ])
