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
import bucket as b
import object as o

# Constants
BUCKET_PREFIX = f'{getpass.getuser().lower()}-s3cmd-unittest-'
TESTSUITE_OUT_DIR = 'testsuite-out'
TESTSUITE_DAT_DIR = 'testsuite-dat'
IN_FILE = '/dev/urandom'
PYTHON = 'python'  # s3cmd does not work with python3
S3 = 's3://'
S3CMD = f'{PATH_TO_S3CMD}/s3cmd'

def executeS3cmd(args, stdin=None):
    args.insert(0, PYTHON)
    args.insert(1, S3CMD)
    result = subprocess.run(args, stdin=stdin, stdout=PIPE, stderr=STDOUT, universal_newlines=True,
        close_fds=True)
    return result

class BaseTest(unittest.TestCase):
    """Base class for test classes."""
    '''
    def __init__(self, *args, **kwargs):
        super().__init__(*args, **kwargs)
    '''
    @classmethod
    def setUpClass(cls):
        cls.removeAllTestBuckets()
        # create a clean testsuite output directory
        if os.path.isdir(TESTSUITE_DAT_DIR):
            shutil.rmtree(TESTSUITE_DAT_DIR)

        os.mkdir(TESTSUITE_DAT_DIR)
        # create a 1M file in the testsuite directory
        obj = o.Object(o.Size._1MB)
        os.system(f'dd if={IN_FILE} of={obj.fullFileName()} bs=1M count=1 > /dev/null 2>&1')

    @classmethod
    def removeAllTestBuckets(cls):
        args = ['ls']
        result = cls.execute(args)
        flist = result.stdout.split(' ')
        for f in flist:
            if not f.startswith(f'{S3}{BUCKET_PREFIX}'):
                continue
            itemList = f.split('\n')
            bucket = b.Bucket(itemList[0], nameType='full')
            bucket.remove()

    @classmethod
    def execute(cls, args, stdin=None):
        """Execute s3cmd."""
        return executeS3cmd(args, stdin)

    def tearDown(self):
        self.removeAllTestBuckets()
