import os
import shutil
import sys
import unittest

#i local imports
import base_test as bt

sys.path.append(bt.PATH_TO_S3CMD) # required to see S3.ExitCodes
import S3.ExitCodes as xcodes

class TestObject(bt.BaseTest):
    '''
    Test s3cmd APIs related to objects. 
    '''
    @classmethod
    def setUpClass(self):
        super().setUpClass()
        # create a clean testsuite output directory
        if os.path.isdir(bt.TESTSUITE_OUT_DIR):
            shutil.rmtree(bt.TESTSUITE_OUT_DIR)

        os.mkdir(bt.TESTSUITE_OUT_DIR)

        # create a 16M file in the testsuite data directory
        os.system(f'dd if={bt.IN_FILE} of={bt.get_16MB_fpath()} bs=1M count=16 > /dev/null 2>&1')
        # create a small file in the testsuite data directory
        os.system(f'dd if={bt.IN_FILE} of={bt.get_1KB_fpath()} bs=1K count=1 > /dev/null 2>&1')

        bucket1 = f'{bt.S3}{bt.makeBucketName(1)}'
        bucket2 = f'{bt.S3}{bt.makeBucketName(2)}'
        args = ['mb', '--recursive',  bucket1, bucket2]
        bt.executeS3cmd(args)

    def tearDown(self):
        # Bypass super().tearDown().  We don't want to remove buckets

        # Remove contents of test buckets
        bt.removeContentAllTestBuckets()

    '''===
    '  Non-public methods
    ==='''
    def _put_1KB(self):
        bucket = f'{bt.S3}{bt.makeBucketName(1)}'
        args = ['put', bt.get_1KB_fpath(), bucket]
        bt.executeS3cmd(args)
        return bucket, f'{bucket}/{bt._1KB_FN}', bt._1KB_FN

    def _put_1MB(self):
        bucket = f'{bt.S3}{bt.makeBucketName(1)}'
        args = ['put', bt.get_1MB_fpath(), bucket]
        bt.executeS3cmd(args)
        return bucket, f'{bucket}/{bt._1MB_FN}', bt._1MB_FN

    def _put_16MB(self):
        bucket = f'{bt.S3}{bt.makeBucketName(1)}'
        args = ['put', '--multipart-chunk-size-mb=5', bt.get_16MB_fpath(), bucket]
        bt.executeS3cmd(args)
        return bucket, f'{bucket}/{bt._16MB_FN}', bt._16MB_FN

    '''=== End of non-public methods ==='''

    '''===
    '  Put object section
    ==='''
    def test_put(self):
        bucket = f'{bt.S3}{bt.makeBucketName(1)}'
        args = ['put', bt.get_1KB_fpath(), bucket]
        result = bt.executeS3cmd(args)
        self.assertEqual(result.returncode, xcodes.EX_OK, msg=result.stdout)
        # verify the file was stored
        args = ['la', bucket]
        result = bt.executeS3cmd(args)
        self.assertEqual(result.returncode, xcodes.EX_OK, msg=result.stdout)
        # verify the put object seen
        fullObj = f'{bucket}/{bt._1KB_FN}'
        self.assertNotEqual(result.stdout.find(fullObj), -1, msg=bt.ERR_NOT_FOUND%fullObj) 
        
    def test_put_stdin(self):
        bucket = f'{bt.S3}{bt.makeBucketName(1)}'
        fullObj = f'{bucket}/stdin{bt._1KB_FN}'
        f = open(bt.get_1KB_fpath(), 'r')
        args = ['put', '-', fullObj]
        result = bt.executeS3cmd(args, stdin=f)
        f.close()
        self.assertEqual(result.returncode, xcodes.EX_OK, msg=result.stdout)
        # verify the file was stored
        args = ['la', bucket]
        result = bt.executeS3cmd(args)
        self.assertEqual(result.returncode, xcodes.EX_OK, msg=result.stdout)
        # verify the put object seen
        self.assertNotEqual(result.stdout.find(fullObj), -1, msg=bt.ERR_NOT_FOUND%fullObj) 
        
    def test_put_multipart(self):
        bucket = f'{bt.S3}{bt.makeBucketName(1)}'
        args = ['put', '--multipart-chunk-size-mb=5', bt.get_16MB_fpath(), bucket]
        result = bt.executeS3cmd(args)
        self.assertEqual(result.returncode, xcodes.EX_OK, msg=result.stdout)
        # verify the file was stored
        args = ['la', bucket]
        result = bt.executeS3cmd(args)
        self.assertEqual(result.returncode, xcodes.EX_OK, msg=result.stdout)
        fullObj = f'{bucket}/{bt._16MB_FN}'
        self.assertNotEqual(result.stdout.find(fullObj), -1, msg=bt.ERR_NOT_FOUND%fullObj) 

    def test_put_multipart_stdin(self):
        bucket = f'{bt.S3}{bt.makeBucketName(1)}'
        fullObj = f'{bucket}/stdin{bt._16MB_FN}'
        f = open(bt.get_16MB_fpath(), 'r')
        args = ['put', '--multipart-chunk-size-mb=5', '-', fullObj]
        result = bt.executeS3cmd(args, stdin=f)
        f.close()
        self.assertEqual(result.returncode, xcodes.EX_OK, msg=result.stdout)
        # verify the file was stored
        args = ['la', bucket]
        result = bt.executeS3cmd(args)
        self.assertEqual(result.returncode, xcodes.EX_OK, msg=result.stdout)
        self.assertNotEqual(result.stdout.find(fullObj), -1, msg=bt.ERR_NOT_FOUND%fullObj) 

    def test_put_multi(self):
        bucket = f'{bt.S3}{bt.makeBucketName(1)}'
        fpath1 = bt.get_1KB_fpath()
        fpath2 = bt.get_1MB_fpath()
        args = ['put', fpath1, fpath2, bucket]
        result = bt.executeS3cmd(args)
        self.assertEqual(result.returncode, xcodes.EX_OK, msg=result.stdout)
        # verify the file was stored
        args = ['la', bucket]
        result = bt.executeS3cmd(args)
        self.assertEqual(result.returncode, xcodes.EX_OK, msg=result.stdout)
        # verify the put object seen
        fullObj = f'{bucket}/{bt._1KB_FN}'
        self.assertNotEqual(result.stdout.find(fullObj), -1, msg=bt.ERR_NOT_FOUND%fullObj) 
        fullObj = f'{bucket}/{bt._1MB_FN}'
        self.assertNotEqual(result.stdout.find(fullObj), -1, msg=bt.ERR_NOT_FOUND%fullObj) 
        
    '''
    TODO: Minio does not work with a mix of small and large files')
    '''
    @unittest.skip
    def test_put_recursive(self):
        bucket = self._makeBucket("recursive")
        args =['put', '--recursive', '--multipart-chunk-size-mb=5', bt.TESTSUITE_DAT_DIR, bucket]
        result = bt.executeS3cmd(args)
        self.assertEqual(result.returncode, xcodes.EX_OK, msg=result.stdout)
        # verify all files were uploaded
        args = ['la', bucket]
        result = bt.executeS3cmd(args)
        self.assertEqual(result.returncode, xcodes.EX_OK, msg=result.stdout)
        files = os.listdir(bt.TESTSUITE_DAT_DIR)
        for f in files:
            self.assertNotEqual(result.stdout.find(f), -1, msg=bt.ERR_NOT_FOUND%f) 
    '''=== End of put object section ==='''

    '''===
    '  Get object section
    ==='''
    def test_get(self):
        _, fullObj, obj = self._put_1KB()
        args = ['get', fullObj, bt.TESTSUITE_OUT_DIR]
        result = bt.executeS3cmd(args)
        self.assertEqual(result.returncode, xcodes.EX_OK, msg=result.stdout)
        # verify the object was downloaded
        self.assertEqual(True, os.path.exists(f'{bt.TESTSUITE_OUT_DIR}/{obj}'),
            msg=bt.ERR_NOT_FOUND%(f'{bt.TESTSUITE_OUT_DIR}/{obj}'))

    @unittest.skip
    def test_get_multipart(self):
        _, fullObj, obj = self._put_16MB()
        args = ['get', fullObj, bt.TESTSUITE_OUT_DIR]
        result = bt.executeS3cmd(args)
        self.assertEqual(result.returncode, xcodes.EX_OK, msg=result.stdout)
        # verify the object was downloaded and its size is right
        fpath = f'{bt.TESTSUITE_OUT_DIR}/{obj}'
        self.assertEqual(True, os.path.exists(fpath), msg=bt.ERR_NOT_FOUND%fpath)
        self.assertEqual(bt._16MB, os.path.getsize(fpath))

    @unittest.skip
    def test_get_multi(self):
        _, fullSmallObj, smallObj = self._put_1KB()
        _, fullLargeObj, largeObj = self._put_16MB()
        args = ['get', fullSmallObj, fullLargeObj, bt.TESTSUITE_OUT_DIR]
        result = bt.executeS3cmd(args)
        self.assertEqual(result.returncode, xcodes.EX_OK, msg=result.stdout)
        # verify the object was downloaded and its size is right
        fpathSmall = f'{bt.TESTSUITE_OUT_DIR}/{smallObj}'
        self.assertEqual(True, os.path.exists(fpathSmall), msg=bt.ERR_NOT_FOUND%fpathSmall)
        fpathLarge = f'{bt.TESTSUITE_OUT_DIR}/{largeObj}'
        self.assertEqual(True, os.path.exists(fpathLarge), msg=bt.ERR_NOT_FOUND%fpathLarge)
        self.assertEqual(bt._16MB, os.path.getsize(fpathLarge))

    '''=== End of get object section ==='''
        
    def test_delete(self):
        _, fullObj, obj = self._put_1KB()
        args = ['del', fullObj]
        result = bt.executeS3cmd(args)
        self.assertEqual(result.returncode, xcodes.EX_OK, msg=result.stdout)
        self.assertFalse(self._isObjExist(fullObj))

    def test_delete_multipart(self):
        _, fullObj, obj = self._put_16MB()
        args = ['del', fullObj]
        result = bt.executeS3cmd(args)
        self.assertEqual(result.returncode, xcodes.EX_OK, msg=result.stdout)
        self.assertFalse(self._isObjExist(fullObj))

    def test_remove(self):
        _, fullObj, obj = self._put_1KB()
        args = ['rm', fullObj]
        result = bt.executeS3cmd(args)
        self.assertEqual(result.returncode, xcodes.EX_OK, msg=result.stdout)
        self.assertFalse(self._isObjExist(fullObj))

    def test_remove_multipart(self):
        _, fullObj, obj = self._put_16MB()
        args = ['rm', fullObj]
        result = bt.executeS3cmd(args)
        self.assertEqual(result.returncode, xcodes.EX_OK, msg=result.stdout)
        self.assertFalse(self._isObjExist(fullObj))

    def test_delete_all_recursively(self):
        bucket, _, _ = self._put_1KB()
        bucket1, _, _ = self._put_16MB()
        self.assertEqual(bucket, bucket1)
        args = ['del', '--recursive', '--force', bucket]
        result = bt.executeS3cmd(args)
        self.assertEqual(result.returncode, xcodes.EX_OK, msg=result.stdout)
        self.assertTrue(self._isBucketEmpty(bucket))

    def test_remove_all_recursively(self):
        bucket, _, _ = self._put_1KB()
        bucket1, _, _ = self._put_16MB()
        self.assertEqual(bucket, bucket1)
        args = ['rm', '--recursive', '--force', bucket]
        result = bt.executeS3cmd(args)
        self.assertEqual(result.returncode, xcodes.EX_OK, msg=result.stdout)
        self.assertTrue(self._isBucketEmpty(bucket))

    '''>>> End of delete section <<<'''

    '''<<< Copy object section >>>''' 

    def test_copy_between_buckets(self):
        bucket, fullObj, obj = self._put_1KB()
        destBucket = self._makeBucket('dest', refresh=True)
        args = ['cp', fullObj, destBucket]
        result = bt.executeS3cmd(args)
        self.assertEqual(result.returncode, xcodes.EX_OK, msg=result.stdout)
        self.assertTrue(self._isObjExist(fullObj), msg="Copied object is in destination")

    def test_copy_multipart_between_buckets(self):
        bucket, fullObj, obj = self._put_16MB()
        destBucket = self._makeBucket('dest', refresh=True)
        args = ['cp', '--multipart-chunk-size-mb=5', fullObj, destBucket]
        result = bt.executeS3cmd(args)
        self.assertEqual(result.returncode, xcodes.EX_OK, msg=result.stdout)
        self.assertTrue(self._isObjExist(f'{destBucket}/{obj}'), msg="Copied object is not in destination")

    '''>>> End of copy section <<<'''

    '''<<< Move object section >>>''' 
    def test_move(self):
        bucket, fullObj, obj = self._put_1KB()
        destBucket = self._makeBucket('dest', refresh=True)
        args = ['mv', fullObj, f'{destBucket}/{obj}']
        result = bt.executeS3cmd(args)
        self.assertEqual(result.returncode, xcodes.EX_OK, msg=result.stdout)
        self.assertTrue(self._isObjExist(f'{destBucket}/{obj}'), msg="Moved object is not in destination")
        self.assertFalse(self._isObjExist(fullObj), msg="Copied object {fullObj} is still in source")

    def test_move_multi(self):
        _, fobj_1, obj_1 = self._put_1KB()
        _, fobj_2, obj_2 = self._put_1MB()
        destBucket = self._makeBucket('dest', refresh=True)
        args = ['mv', fobj_1, fobj_2, destBucket]
        result = bt.executeS3cmd(args)
        self.assertEqual(result.returncode, xcodes.EX_OK, msg=result.stdout)
        self.assertTrue(self._isObjExist(f'{destBucket}/{obj_1}'), msg="Moved object is not in destination")
        self.assertTrue(self._isObjExist(f'{destBucket}/{obj_2}'), msg="Moved object is not in destination")
        self.assertFalse(self._isObjExist(fobj_1), msg=f'Copied object {fobj_1} is still in source')
        self.assertFalse(self._isObjExist(fobj_2), msg=f'Copied object {fobj_2} is still in source')

        
    '''>>> End of move section <<<'''

if __name__ == '__main__':
    unittest.main()
