import getpass
import os
import shutil
import subprocess
from subprocess import Popen, PIPE, STDOUT
import sys
import unittest

PATH_TO_S3CMD = '../s3cmd' # required for the next instruction
if PATH_TO_S3CMD not in sys.path:
    sys.path.append(PATH_TO_S3CMD) # required to see S3.ExitCodes

#local imports
import file_system
import s3bucket

# Constants
BUCKET_PREFIX = f'{getpass.getuser().lower()}-s3cmd-unittest-'
PYTHON = 'python'  # s3cmd does not work with python3
S3 = 's3://'
S3CMD = f'{PATH_TO_S3CMD}/s3cmd'

def executeS3cmd(args, stdin=None):
    args.insert(0, PYTHON)
    args.insert(1, S3CMD)
    result = subprocess.run(args, stdin=stdin, stdout=PIPE, stderr=STDOUT, universal_newlines=True,
        close_fds=True)
    return result

# Similar to executeS3cmd(), but for generic (non-s3cmd) Linux commands
def executeLinuxCmd(args, stdin=None):
    result = subprocess.run(args, stdin=stdin, stdout=PIPE, stderr=STDOUT, universal_newlines=True,
        close_fds=True)
    return result

class BaseTest(unittest.TestCase):
    """Base class for test classes."""

    @classmethod
    def setUpClass(cls):
        """Do class setup: Create all input files"""

        fileProducer = file_system.FileProducer()
        fileProducer.makeAll()

    @classmethod
    def removeAllTestBuckets(cls):
        args = ['ls']
        result = cls.execute(args)
        flist = result.stdout.split(' ')
        for f in flist:
            if not f.startswith(f'{S3}{BUCKET_PREFIX}'):
                continue
            itemList = f.split('\n')
            bucket = s3bucket.S3Bucket(itemList[0], nameType='full')
            bucket.remove()

    @classmethod
    def execute(cls, args, stdin=None):
        """Execute s3cmd."""
        return executeS3cmd(args, stdin)

    def tearDown(self):
        self.removeAllTestBuckets()
