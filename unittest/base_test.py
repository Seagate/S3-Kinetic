import unittest
from subprocess import Popen, PIPE, STDOUT
import subprocess

# define constants
PYTHON = 'python3'
PATH_TO_S3CMD = '../s3cmd'
S3CMD = '%s/s3cmd'%PATH_TO_S3CMD

class BaseTest(unittest.TestCase):
    def setUp(self):
        pass

    def tearDown(self):
        pass

    def _execute(self, args):
        args.insert(0, PYTHON)
        args.insert(1, S3CMD)
        completed_p = subprocess.run(args, stdout=PIPE, stderr=STDOUT, universal_newlines=True,
            close_fds=True)
        return completed_p

