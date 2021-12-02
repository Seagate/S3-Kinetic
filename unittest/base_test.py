import getpass
import os
import shutil
import subprocess
from subprocess import Popen, PIPE, STDOUT
import sys
import unittest

PATH_TO_S3CMD = '../s3cmd'     # place here for the next line
sys.path.append(PATH_TO_S3CMD) # required to see S3.ExitCodes
import S3.ExitCodes as xcodes

#local imports
import bucket as b

BUCKET_PREFIX = f'{getpass.getuser().lower()}-s3cmd-unittest-'
TESTSUITE_OUT_DIR = 'testsuite-out'
TESTSUITE_DAT_DIR = 'testsuite-dat'
IN_FILE = '/dev/urandom'
PYTHON = 'python'  # s3cmd does not work with python3
S3 = 's3://'
S3CMD = f'{PATH_TO_S3CMD}/s3cmd'

_1MB_FN = '_1MB.bin'
_16MB_FN = '_16MB.bin'
_1KB_FN = '_1KB.bin'

ERR_NOT_FOUND = 'ERROR: %s not found'
ERR_FOUND = 'ERROR: %s found'

def get_1KB_fpath():
    return f'{TESTSUITE_DAT_DIR}/{_1KB_FN}'

def get_1MB_fpath():
    return f'{TESTSUITE_DAT_DIR}/{_1MB_FN}'

def get_16MB_fpath():
    return f'{TESTSUITE_DAT_DIR}/{_16MB_FN}'

def makeBucketName(suffix):
    return f"{BUCKET_PREFIX}{suffix}"

def executeS3cmd(args, stdin=None):
    args.insert(0, PYTHON)
    args.insert(1, S3CMD)
    result = subprocess.run(args, stdin=stdin, stdout=PIPE, stderr=STDOUT, universal_newlines=True,
        close_fds=True)
    return result

def removeBucket(name):
    args = ['rb', '--recursive', name]
    executeS3cmd(args)

def removeAllTestBuckets():
    args = ['ls']
    result = executeS3cmd(args)
    flist = result.stdout.split(' ')
    for f in flist:
        if not f.startswith(f'{S3}{BUCKET_PREFIX}'):
            continue
        itemList = f.split('\n')
        bucket = b.Bucket(itemList[0], nameType='full')
        bucket.remove()

def removeContentAllTestBuckets():
    bucket = f'{S3}{BUCKET_PREFIX}'
    args = ['ls']
    result = executeS3cmd(args)
    flist = result.stdout.split(' ')
    for f in flist:
        if not f.startswith(bucket):
            continue
        itemList = f.split('\n')
        args = ['rm', '--recursive', '--force', itemList[0]]
        executeS3cmd(args)

class BaseTest(unittest.TestCase):
    '''
    Base class for test classes
    '''
    @classmethod
    def setUpClass(self):
        removeAllTestBuckets()
        # create a clean testsuite output directory
        if os.path.isdir(TESTSUITE_DAT_DIR):
            shutil.rmtree(TESTSUITE_DAT_DIR)

        os.mkdir(TESTSUITE_DAT_DIR)
        # create a 1M file in the testsuite directory
        os.system(f'dd if={IN_FILE} of={get_1MB_fpath()} bs=1M count=1 > /dev/null 2>&1')

    def tearDown(self):
        '''
        Clean up anything used by this class
        '''
        removeAllTestBuckets()

    def _isBucketExist(self, bucket):
        args = ['ls', bucket]
        result = executeS3cmd(args)
        return (result.returncode == xcodes.EX_OK and result.stdout.find(bucket) != -1)

    def _isObjExist(self, fullObj):
        args = ['la', fullObj]
        result = executeS3cmd(args)
        return (result.returncode == xcodes.EX_OK and result.stdout.find(fullObj) != -1)

    def _isBucketEmpty(self, bucket):
        args = ['la', bucket]
        result = executeS3cmd(args)
        return (result.returncode == xcodes.EX_OK and result.stdout.find(bucket) == -1)

    def _makeBucket(self, suffix, refresh=False):
        bucketName = makeBucketName(suffix)
        bucket = f'{S3}{bucketName}'
        if not self._isBucketExist(bucket):
            args = ['mb', bucket]
            executeS3cmd(args)
        elif refresh:
            args = ['del', '--recursive', '--force', bucket]
            executeS3cmd(args)
            
        return bucket 



