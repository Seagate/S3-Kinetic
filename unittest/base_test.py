import unittest
import os
import getpass
from subprocess import Popen, PIPE, STDOUT
import subprocess
import shutil

#--- define constants ---
BUCKET_PREFIX = '%s-s3cmd-unittest-' % getpass.getuser().lower()
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
    return '%s/%s'%(TESTSUITE_OUT_DIR, _1MB_FN)

'''
'  Description:  Base class for test classes
'''
class BaseTest(unittest.TestCase):
    '''
    ' def tearDown()
    ' Description: Clean up anything used by this class
    '''
    def tearDown(self):
        # remove all buckets
        self._removeBucket(f'{S3}{makeBucketName(1)}')
        self._removeBucket(f'{S3}{makeBucketName(2)}')
        

    def _execute(self, args):
        args.insert(0, PYTHON)
        args.insert(1, S3CMD)
        result = subprocess.run(args, stdout=PIPE, stderr=STDOUT, universal_newlines=True,
            close_fds=True)
        return result

    def _removeBucket(self, name):
        args = ['rb', '--recursive', name]
        self._execute(args)
            
