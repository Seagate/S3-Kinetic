import unittest
import sys
import os
import getpass
from subprocess import Popen, PIPE, STDOUT
import subprocess
import shutil

from constants import *

sys.path.append(PATH_TO_S3CMD)

from S3.ExitCodes import *

unittest.TestLoader.sortTestMethodsUsing = None

class TestBucket(unittest.TestCase):
    def setUp(self):
        # create testsuite output directory
        if not os.path.isdir(TESTSUITE_OUT_DIR):
            os.mkdir(TESTSUITE_OUT_DIR)
        
        # create a 1M file
        os.system('dd if=%s of=%s/%s bs=1M count=1 > /dev/null 2>&1' % \
            (IN_FILE, TESTSUITE_OUT_DIR, ONE_MB_FN))

    def tearDown(self):
        if os.path.isdir(TESTSUITE_OUT_DIR):
            shutil.rmtree(TESTSUITE_OUT_DIR)

    def execute(self, args):
        args.insert(0, PYTHON)
        args.insert(1, S3CMD)
        completed_p = subprocess.run(args, stdout=PIPE, stderr=STDOUT, universal_newlines=True,
            close_fds=True)
        return completed_p

    def test_make(self):
        # create a single bucket
        args = ['mb', '--bucket-location=EU', '%s%s'%(S3, makeBucketName(1))]
        completed_p = self.execute(args)
        self.assertEqual(completed_p.returncode, EX_OK)

        # create a bucket with invalid name
        args = ['mb', '--bucket-location=EU', '%s%s'%(S3, makeBucketName('EU'))]
        completed_p = self.execute(args)
        self.assertEqual(completed_p.returncode, EX_USAGE)

        # create multiple buckets
        args = ['mb', '%s%s'%(S3, makeBucketName(2)), '%s%s'%(S3, makeBucketName(3))]
        completed_p = self.execute(args)
        self.assertEqual(completed_p.returncode, EX_OK)

        # create an exising bucket
        args = ['mb', '%s%s'%(S3, makeBucketName(2))]
        completed_p = self.execute(args)
        self.assertEqual(completed_p.returncode, EX_CONFLICT)

    def test_list(self):
        args = ['ls']
        completed_p = self.execute(args)
        self.assertEqual(completed_p.returncode, EX_OK)

    def test_list_all(self):
        args = ['la']
        completed_p = self.execute(args)
        self.assertEqual(completed_p.returncode, EX_OK)

    def test_disk_usage(self):
        args = ['du', '%s%s'%(S3, makeBucketName(1))]
        completed_p = self.execute(args)
        self.assertEqual(completed_p.returncode, EX_OK)

    def test_remove(self):
        # remove a single empty bucket
        args = ['rb', '%s%s'%(S3, makeBucketName(1))]
        completed_p = self.execute(args)
        self.assertEqual(completed_p.returncode, EX_OK)
        # remove multiple empty buckets
        args = ['rb', '%s%s'%(S3, makeBucketName(2)), '%s%s'%(S3, makeBucketName(3))]
        completed_p = self.execute(args)
        self.assertEqual(completed_p.returncode, EX_OK)

        ### remove non-empty bucket ###
        # 1. create a bucket
        args = ['mb', '%s%s'%(S3, makeBucketName(1))]
        completed_p = self.execute(args)
        self.assertEqual(completed_p.returncode, EX_OK)

        # 2. put an object into the bucket to make them non-empty then remove it
        args = ['put', '%s/%s'%(TESTSUITE_OUT_DIR, ONE_MB_FN), '%s%s'%(S3, makeBucketName(1))]
        completed_p = self.execute(args)
        self.assertEqual(completed_p.returncode, EX_OK)

        # remove a non-empty bucket
        args = ['rb', '%s%s'%(S3, makeBucketName(1))]
        completed_p = self.execute(args)
        self.assertEqual(completed_p.returncode, EX_CONFLICT)

        args = ['rb', '--recursive', '%s%s'%(S3, makeBucketName(1))]
        completed_p = self.execute(args)
        self.assertEqual(completed_p.returncode, EX_OK)

if __name__ == '__main__':
    unittest.main()

