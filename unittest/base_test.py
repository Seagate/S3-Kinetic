import getpass
import os
import shutil
import subprocess
from subprocess import Popen, PIPE, STDOUT
import unittest

#--- define constants ---
BUCKET_PREFIX = f'{getpass.getuser().lower()}-s3cmd-unittest-'
IN_FILE = '/dev/urandom'
PYTHON = 'python'  # s3cmd does not work with python3
PATH_TO_S3CMD = '../s3cmd'
S3 = 's3://'
S3CMD = f'{PATH_TO_S3CMD}/s3cmd'
TESTSUITE_OUT_DIR = 'testsuite-out'
_1MB_FN = '_1MB.bin'

#--- define methods ---
def makeBucketName(suffix):
    return f"{BUCKET_PREFIX}{suffix}"

def get_1MB_fpath():
    return f'{TESTSUITE_OUT_DIR}/{_1MB_FN}'

'''
'  Description:  Base class for test classes
'''
class BaseTest(unittest.TestCase):
    '''
    ' def tearDown()
    ' Description: Clean up anything used by this class
    '''
    def tearDown(self):
        self._removeAllTestBuckets()

    def _execute(self, args):
        args.insert(0, PYTHON)
        args.insert(1, S3CMD)
        result = subprocess.run(args, stdout=PIPE, stderr=STDOUT, universal_newlines=True,
            close_fds=True)
        return result

    def _removeBucket(self, name):
        args = ['rb', '--recursive', name]
        self._execute(args)

    def _removeAllTestBuckets(self):
        bucket = f'{S3}{BUCKET_PREFIX}'
        args = ['ls']
        result = self._execute(args)
        flist = result.stdout.split(' ')
        for f in flist:
            if not f.startswith(bucket):
                continue
            itemList = f.split('\n')
            self._removeBucket(itemList[0])
