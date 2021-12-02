import sys
import unittest

import base_test as bt
import bucket as b

sys.path.append(bt.PATH_TO_S3CMD) # required to see S3.ExitCodes
import S3.ExitCodes as xcodes

# define constants
_1MB = 1048576

class TestBucket(bt.BaseTest):
    '''
    Test s3cmd APIs related to buckets. 
    
    This class verifies different common operations like:
       * create a bucket
       * delete a bucket
       * list buckets
       * delete buckets
       
    A bucket could be seen as a folder in which objects can be stored
    ''' 
    def test_make_single(self):
        bucket = b.Bucket(1)
        args = ['mb', bucket.fullName()]
        result = bt.executeS3cmd(args)
        self.assertEqual(result.returncode, xcodes.EX_OK, msg=result.stdout)
        self.assertTrue(bucket.isExist(), msg=bt.ERR_NOT_FOUND%(bucket.fullName()))

    def test_make_invalid(self):
        bucket = b.Bucket('EU')
        args = ['mb', '--bucket-location=EU', bucket.fullName()]
        result = bt.executeS3cmd(args)
        self.assertEqual(result.returncode, xcodes.EX_USAGE, msg=result.stdout)
        self.assertFalse(bucket.isExist(), msg=bt.ERR_FOUND%(bucket.fullName()))

    def test_make_multi(self):
        # create 2 buckets
        bucket_1 = b.Bucket(1)
        bucket_2 = b.Bucket(2)
        args = ['mb', bucket_1.fullName(), bucket_2.fullName()]
        result = bt.executeS3cmd(args)
        self.assertEqual(result.returncode, xcodes.EX_OK, msg=result.stdout)
        self.assertTrue(bucket_1.isExist(), msg=bt.ERR_NOT_FOUND%(bucket_1.fullName()))
        self.assertTrue(bucket_2.isExist(), msg=bt.ERR_NOT_FOUND%(bucket_2.fullName()))

    def test_make_exist_bucket(self):
        bucket = b.Bucket(1)
        bucket.make()
        # remake the same bucket
        args = ['mb', bucket.fullName()]
        result = bt.executeS3cmd(args)
        self.assertEqual(result.returncode, xcodes.EX_CONFLICT, msg=result.stdout)

    '''
    List all buckets (no contents)
    '''
    def test_list(self):
        # make a bucket
        bucket = b.Bucket(1)
        bucket.make()
        # list
        args = ['ls']
        result = bt.executeS3cmd(args)
        self.assertEqual(result.returncode, xcodes.EX_OK, msg=result.stdout)
        self.assertTrue(bucket.isExist(), msg=bt.ERR_NOT_FOUND%(bucket.fullName()))

    '''
    List bucket contents
    '''
    def test_list_all(self):
        # make a bucket
        bucket = b.Bucket(1)
        bucket.make()
        # put an object to the bucket
        putFile = bt.get_1MB_fpath()
        bucket.put(putFile)
        # checkout if the putfile is the bucket
        args = ['la', bucket.fullName()]
        result = bt.executeS3cmd(args)
        self.assertEqual(result.returncode, xcodes.EX_OK, msg=result.stdout)
        objName = f'{bucket.fullName()}/{bt._1MB_FN}'
        self.assertTrue(result.stdout.find(objName) != -1, msg=bt.ERR_NOT_FOUND%(objName))

    '''
    Ensure disk usage of a bucket equals to sum of sizes of all objects in the bucket
    '''
    def test_disk_usage(self):
        # make a bucket
        bucket = b.Bucket(1)
        bucket.make()
        # put an object to the bucket
        putFile1 = f'{bt.get_1MB_fpath()}'
        bucket.put(putFile1, f'{putFile1}_1')
        putFile2 = f'{bt.get_1MB_fpath()}'
        bucket.put(putFile2, f'{putFile2}_2')
        # disk usage
        args = ['du', bucket.fullName()]
        result = bt.executeS3cmd(args)
        self.assertEqual(result.returncode, xcodes.EX_OK, msg=result.stdout)
        self.assertNotEqual(result.stdout.find(str(2*_1MB)), -1, msg=f'bt.ERROR: Incorrect disk usage size')

    def test_remove_single_empty(self):
        # make a bucket
        bucket = b.Bucket(1)
        bucket.make()
        # remove bucket
        args = ['rb', bucket.fullName()]
        result = bt.executeS3cmd(args)
        self.assertEqual(result.returncode, xcodes.EX_OK, msg=result.stdout)
        # verify bucket removed
        self.assertFalse(bucket.isExist(), msg=bt.ERR_FOUND%(bucket.fullName())) 

    def test_remove_multi_empty(self):
        # create 2 bucket
        bucket1 = b.Bucket(1)
        bucket2 = b.Bucket(2)
        bucket1.make()
        bucket2.make()
        # remove multiple empty buckets
        args = ['rb', bucket1.fullName(), bucket2.fullName()]
        result = bt.executeS3cmd(args)
        self.assertEqual(result.returncode, xcodes.EX_OK, msg=result.stdout)
        # verify buckets removed
        self.assertFalse(bucket1.isExist(), msg=bt.ERR_FOUND%(bucket1.fullName())) 
        self.assertFalse(bucket2.isExist(), msg=bt.ERR_FOUND%(bucket2.fullName())) 

    def test_remove_single_non_empty(self):
        # create a bucket
        bucket = b.Bucket(1)
        bucket.make()
        # put an object to the bucket
        putFile = bt.get_1MB_fpath()
        bucket.put(putFile)
        # remove the bucket
        args = ['rb', '--recursive', bucket.fullName()]
        result = bt.executeS3cmd(args)
        self.assertEqual(result.returncode, xcodes.EX_OK, msg=result.stdout)
        # verify bucket removed
        self.assertFalse(bucket.isExist(), msg=bt.ERR_FOUND%(bucket.fullName())) 

    def test_remove_multi_non_empty(self):
        # create 2 bucket
        bucket1 = b.Bucket(1)
        bucket2 = b.Bucket(2)
        bucket1.make()
        bucket2.make()
        # put an object to the buckets
        putFile = bt.get_1MB_fpath()
        bucket1.put(putFile)
        bucket2.put(putFile)
        # remove those 2 buckets
        args = ['rb', '--recursive', bucket1.fullName(), bucket2.fullName()]
        result = bt.executeS3cmd(args)
        self.assertEqual(result.returncode, xcodes.EX_OK, msg=result.stdout)
        # verify bucket removed
        self.assertFalse(bucket1.isExist(), msg=bt.ERR_FOUND%(bucket1.fullName())) 
        self.assertFalse(bucket2.isExist(), msg=bt.ERR_FOUND%(bucket2.fullName())) 

if __name__ == '__main__':
    unittest.main()
