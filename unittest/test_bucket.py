import sys
import os
import shutil
import unittest

import base_test as bt

sys.path.append(bt.PATH_TO_S3CMD) # required to see S3.ExitCodes
import S3.ExitCodes as xcodes

# define constants
_1MB = 1048576

'''
' class TestBucket
'
' Description: Test s3cmd related to bucket
''' 
class TestBucket(bt.BaseTest):

    def test_make_single(self):
        # create a single bucket
        bucket = f'{bt.S3}{bt.makeBucketName(1)}'
        args = ['mb', bucket]
        result = self._execute(args)
        self.assertEqual(result.returncode, xcodes.EX_OK)
        # verify bucket was created
        args = ['ls']
        result = self._execute(args)
        self.assertEqual(result.returncode, xcodes.EX_OK) 
        self.assertNotEqual(result.stdout.find(bucket), -1) 

    def test_make_invalid(self):
        # create a single bucket
        bucket = f'{bt.S3}{bt.makeBucketName(1)}'
        args = ['mb', '--bucket-location=EU', bucket]
        result = self._execute(args)
        self.assertEqual(result.returncode, xcodes.EX_OK)
        # create a bucket with invalid name
        args = ['mb', '--bucket-location=EU', f'{bt.S3}{bt.makeBucketName("EU")}']
        result = self._execute(args)
        self.assertEqual(result.returncode, xcodes.EX_USAGE)

    def test_make_multi(self):
        # create 2 buckets
        bucket1 = f'{bt.S3}{bt.makeBucketName(1)}'
        bucket2 = f'{bt.S3}{bt.makeBucketName(2)}'
        args = ['mb', bucket1, bucket2]
        result = self._execute(args)
        self.assertEqual(result.returncode, xcodes.EX_OK)
        # verify buckets created
        args = ['ls']
        result = self._execute(args)
        self.assertEqual(result.returncode, xcodes.EX_OK) 
        self.assertNotEqual(result.stdout.find(bucket1), -1) 
        self.assertNotEqual(result.stdout.find(bucket2), -1) 

    def test_make_exist_bucket(self):
        # make a bucket
        bucket = f'{bt.S3}{bt.makeBucketName(1)}'
        args = ['mb', bucket]
        result = self._execute(args)
        self.assertEqual(result.returncode, xcodes.EX_OK)
        # remake the same bucket
        args = ['mb', bucket]
        result = self._execute(args)
        self.assertEqual(result.returncode, xcodes.EX_CONFLICT)

    '''
    ' def test_list():
    '
    ' Description:  list all buckets (no contents)
    '''
    def test_list(self):
        # make a bucket
        bucket = f'{bt.S3}{bt.makeBucketName(1)}'
        args = ['mb', bucket]
        result = self._execute(args)
        self.assertEqual(result.returncode, xcodes.EX_OK)
        # list
        args = ['ls']
        result = self._execute(args)
        self.assertEqual(result.returncode, xcodes.EX_OK) 
        self.assertNotEqual(result.stdout.find(bucket), -1) 

    '''
    ' def test_list_all():
    '
    ' Description:  list bucket contents (no contents)
    '''
    def test_list_all(self):
        # make a bucket
        bucket = f'{bt.S3}{bt.makeBucketName(1)}'
        args = ['mb', bucket]
        result = self._execute(args)
        self.assertEqual(result.returncode, xcodes.EX_OK)
        # put an object to the bucket
        args = ['put', bt.get_1MB_fpath(), bucket]
        result = self._execute(args)
        self.assertEqual(result.returncode, xcodes.EX_OK)
        # list bucket content
        args = ['la']
        result = self._execute(args)
        self.assertEqual(result.returncode, xcodes.EX_OK)
        # verify the put object seen
        self.assertNotEqual(result.stdout.find(bucket), -1) 

    def test_disk_usage(self):
        # make a bucket
        bucket = f'{bt.S3}{bt.makeBucketName(1)}'
        args = ['mb', bucket]
        result = self._execute(args)
        self.assertEqual(result.returncode, xcodes.EX_OK)
        # put an object to the bucket
        args = ['put', bt.get_1MB_fpath(), bucket]
        result = self._execute(args)
        self.assertEqual(result.returncode, xcodes.EX_OK)
        # disk usage
        args = ['du', bucket]
        result = self._execute(args)
        self.assertEqual(result.returncode, xcodes.EX_OK)
        self.assertNotEqual(result.stdout.find(str(_1MB)), -1)

    def test_remove_single_empty(self):
        # make a bucket
        bucket = f'{bt.S3}{bt.makeBucketName(1)}'
        args = ['mb', bucket]
        result = self._execute(args)
        self.assertEqual(result.returncode, xcodes.EX_OK)
        # remove bucket
        args = ['rb', bucket]
        result = self._execute(args)
        self.assertEqual(result.returncode, xcodes.EX_OK)
        # verify bucket removed
        args = ['ls']
        result = self._execute(args)
        self.assertEqual(result.returncode, xcodes.EX_OK) 
        self.assertEqual(result.stdout.find(bucket), -1) 

    def test_remove_multi_empty(self):
        # create 2 bucket
        bucket1 = f'{bt.S3}{bt.makeBucketName(1)}'
        bucket2 = f'{bt.S3}{bt.makeBucketName(2)}'
        args = ['mb', bucket1, bucket2]
        result = self._execute(args)
        self.assertEqual(result.returncode, xcodes.EX_OK)
        # remove multiple empty buckets
        args = ['rb', bucket1, bucket2]
        result = self._execute(args)
        self.assertEqual(result.returncode, xcodes.EX_OK)
        # verify buckets removed
        args = ['ls']
        result = self._execute(args)
        self.assertEqual(result.returncode, xcodes.EX_OK) 
        self.assertEqual(result.stdout.find(bucket1), -1) 
        self.assertEqual(result.stdout.find(bucket2), -1) 

    def test_remove_single_non_empty(self):
        # create a bucket
        bucket = f'{bt.S3}{bt.makeBucketName(1)}'
        args = ['mb', bucket]
        result = self._execute(args)
        self.assertEqual(result.returncode, xcodes.EX_OK)
        # put an object to the bucket
        args = ['put', bt.get_1MB_fpath(), bucket]
        result = self._execute(args)
        self.assertEqual(result.returncode, xcodes.EX_OK)
        # remove the bucket
        args = ['rb', '--recursive', bucket]
        result = self._execute(args)
        self.assertEqual(result.returncode, xcodes.EX_OK)
        # verify bucket removed
        args = ['ls']
        result = self._execute(args)
        self.assertEqual(result.returncode, xcodes.EX_OK) 
        self.assertEqual(result.stdout.find(bucket), -1) 

    def test_remove_multi_non_empty(self):
        # create 2 bucket
        bucket1 = f'{bt.S3}{bt.makeBucketName(1)}'
        bucket2 = f'{bt.S3}{bt.makeBucketName(2)}'
        args = ['mb', bucket1, bucket2]
        result = self._execute(args)
        self.assertEqual(result.returncode, xcodes.EX_OK)
        # put an object to the buckets
        args = ['put', bt.get_1MB_fpath(), bucket1]
        result = self._execute(args)
        self.assertEqual(result.returncode, xcodes.EX_OK)

        args = ['put', bt.get_1MB_fpath(), bucket2]
        result = self._execute(args)
        self.assertEqual(result.returncode, xcodes.EX_OK)
        # remove those 2 buckets
        args = ['rb', '--recursive', bucket1, bucket2]
        result = self._execute(args)
        self.assertEqual(result.returncode, xcodes.EX_OK)
        # verify bucket removed
        args = ['ls']
        result = self._execute(args)
        self.assertEqual(result.returncode, xcodes.EX_OK) 
        self.assertEqual(result.stdout.find(bucket1), -1) 
        self.assertEqual(result.stdout.find(bucket2), -1) 

def prepareData():
    # create a clean testsuite output directory
    if os.path.isdir(bt.TESTSUITE_OUT_DIR):
        shutil.rmtree(bt.TESTSUITE_OUT_DIR)

    os.mkdir(bt.TESTSUITE_OUT_DIR)
    # create a 1M file in the testsuite directory
    os.system(f'dd if={bt.IN_FILE} of={bt.get_1MB_fpath()} bs=1M count=1 > /dev/null 2>&1')
    
if __name__ == '__main__':
    prepareData()
    unittest.main()
