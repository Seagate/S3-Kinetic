import unittest
import getpass
import sys
from subprocess import Popen, PIPE, STDOUT
import subprocess

from constants import *

sys.path.append(PATH_TO_S3CMD)

from S3.ExitCodes import *

unittest.TestLoader.sortTestMethodsUsing = None

class TestBucket(unittest.TestCase):
    def setUp(self):
        pass

    def tearDown(self):
        pass

    def execute(self, args):
        args.insert(0, PYTHON)
        args.insert(1, S3CMD)
        completed_p = subprocess.run(args, stdout=PIPE, stderr=STDOUT, universal_newlines=True)
        self.assertEqual(completed_p.returncode, EX_OK)

    def test_make(self):
        args = ['mb', '%s%s'%(S3, makeBucketName(1))]
        self.execute(args)

    def test_list(self):
        args = ['ls']
        self.execute(args)

    def test_list_all(self):
        args = ['la']
        self.execute(args)

    def test_remove(self):
        args = ['rb', '%s%s'%(S3, makeBucketName(1))]
        self.execute(args)

if __name__ == '__main__':
    unittest.main()

