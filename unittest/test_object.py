import os
import shutil
import sys
import unittest

# local imports
import base_test as bt
import object as o
import bucket as b
import message as msg

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

        # create a small file in the testsuite data directory
        obj = o.Object(o.Size._1KB)
        os.system(f'dd if={bt.IN_FILE} of={obj.fullFileName()} bs=1K count=1 > /dev/null 2>&1')
        # create a small file in the testsuite data directory
        obj = o.Object(o.Size._1MB)
        os.system(f'dd if={bt.IN_FILE} of={obj.fullFileName()} bs=1M count=1 > /dev/null 2>&1')
        # create a 16M file in the testsuite data directory
        obj = o.Object(o.Size._16MB)
        os.system(f'dd if={bt.IN_FILE} of={obj.fullFileName()} bs=1M count=16 > /dev/null 2>&1')

        bucket1 = b.Bucket(1)
        bucket2 = b.Bucket(2)
        bucket1.make()
        bucket2.make()

    def tearDown(self):
        # Bypass super().tearDown().  We don't want to remove buckets

        # Remove contents of test buckets
        self.removeContentAllTestBuckets()

    @classmethod
    def removeContentAllTestBuckets(cls):
        args = ['ls']
        result = cls.execute(args)
        flist = result.stdout.split(' ')
        for f in flist:
            if not f.startswith(f'{bt.S3}{bt.BUCKET_PREFIX}'):
                continue
            itemList = f.split('\n')
            bucket = b.Bucket(itemList[0], nameType='full')
            bucket.remove()

    '''===
    '  Put object section
    ==='''
    def test_put(self):
        bucket = b.Bucket(1)
        bucket.make()
        obj = o.Object(o.Size._1KB)
        args = ['put', obj.fullFileName(), bucket.fullName()]
        result = self.execute(args)
        self.assertEqual(result.returncode, xcodes.EX_OK, msg=result.stdout)
        # verify the file was stored
        self.assertTrue(bucket.isContain(obj.name()), msg=msg.Message.notFound(obj.name(), bucket.fullName())) 
        
    def test_put_stdin(self):
        bucket = b.Bucket(1)
        bucket.make()
        obj = o.Object(o.Size._1KB)
        objFullName = f'{bucket.fullName()}/{obj.name()}'
        f = open(obj.fullFileName(), 'r')
        args = ['put', '-', objFullName]
        result = self.execute(args, stdin=f)
        f.close()
        self.assertEqual(result.returncode, xcodes.EX_OK, msg=result.stdout)
        # verify the file was stored
        self.assertTrue(bucket.isContain(obj.name()), msg=msg.Message.notFound(obj.name(), bucket.fullName()))
        
    def test_put_multipart(self):
        bucket = b.Bucket(1)
        bucket.make()
        obj = o.Object(o.Size._16MB)
        args = ['put', '--multipart-chunk-size-mb=5', obj.fullFileName(), bucket.fullName()]
        result = self.execute(args)
        self.assertEqual(result.returncode, xcodes.EX_OK, msg=result.stdout)
        # verify the file was stored
        obj.setBucket(bucket)
        self.assertTrue(bucket.isContain(obj.name()), msg=msg.Message.notFound(obj.name(), bucket.fullName()))

    def test_put_multipart_stdin(self):
        bucket = b.Bucket(1)
        bucket.make()
        obj = o.Object(o.Size._16MB)
        objFullName = f'{bucket.fullName()}/{obj.name()}'
        f = open(obj.fullFileName(), 'r')
        args = ['put', '--multipart-chunk-size-mb=5', '-', objFullName]
        result = self.execute(args, stdin=f)
        f.close()
        self.assertEqual(result.returncode, xcodes.EX_OK, msg=result.stdout)
        # verify the file was stored
        self.assertTrue(bucket.isContain(obj.name()), msg=msg.Message.notFound(obj.name(), bucket.fullName()))

    def test_put_multi(self):
        bucket = b.Bucket(1)
        bucket.make()
        obj1 = o.Object(o.Size._1KB)
        obj2 = o.Object(o.Size._1MB)
        args = ['put', obj1.fullFileName(), obj2.fullFileName(), bucket.fullName()]
        result = self.execute(args)
        self.assertEqual(result.returncode, xcodes.EX_OK, msg=result.stdout)
        obj1.setBucket(bucket)
        obj2.setBucket(bucket)
        # verify the file was stored
        self.assertTrue(bucket.isContain(obj1.name()), msg=msg.Message.notFound(obj1.name(), bucket.fullName()))
        self.assertTrue(bucket.isContain(obj2.name()), msg=msg.Message.notFound(obj2.name(), bucket.fullName()))
        
    '''
    TODO: Minio does not work with a mix of small and large files')
    '''
    @unittest.skip
    def test_put_recursive(self):
        bucket = b.Bucket("recursive")
        bucket.make()
        args =['put', '--recursive', '--multipart-chunk-size-mb=5', \
            bt.TESTSUITE_DAT_DIR, bucket.fullName()]
        result = self.execute(args)
        self.assertEqual(result.returncode, xcodes.EX_OK, msg=result.stdout)
        # verify all files were uploaded
        args = ['la', bucket]
        result = self.execute(args)
        self.assertEqual(result.returncode, xcodes.EX_OK, msg=result.stdout)
        files = os.listdir(bt.TESTSUITE_DAT_DIR)
        for f in files:
            self.assertNotEqual(result.stdout.find(f), -1, msg=msg.Message.notFound(f, bt.TESTSUITE_DAT_DIR)) 

    '''=== End of put object section ==='''

    '''===
    '  Get object section
    ==='''
    def test_get(self):
        bucket = b.Bucket(1)
        bucket.make()
        obj = o.Object(o.Size._1KB)
        bucket.put(obj)
        args = ['get', obj.fullName(), bt.TESTSUITE_OUT_DIR]
        result = self.execute(args)
        self.assertEqual(result.returncode, xcodes.EX_OK, msg=result.stdout)
        # verify the object was downloaded
        self.assertTrue(os.path.exists(f'{bt.TESTSUITE_OUT_DIR}/{obj.name()}'),
            msg=msg.Message.notFound(obj.name(), bt.TESTSUITE_OUT_DIR))

    @unittest.skip
    def test_get_multipart(self):
        bucket = b.Bucket(1)
        bucket.make()
        obj = o.Object(o.Size._16MB)
        bucket.put(obj)
        args = ['get', obj.fullName(), bt.TESTSUITE_OUT_DIR]
        result = self.execute(args)
        self.assertEqual(result.returncode, xcodes.EX_OK, msg=result.stdout)
        # verify the object was downloaded and its size is right
        fpath = f'{bt.TESTSUITE_OUT_DIR}/{obj.name()}'
        self.assertTrue(os.path.exists(fpath), msg=msg.Message.notFound(obj.name(), bt.TESTSUITE_OUT_DIR)) 
        self.assertEqual(bt._16MB, os.path.getsize(fpath), msg=msg.Message.mismatchSize(fpath))

    @unittest.skip
    def test_get_multi(self):
        bucket = b.Bucket(1)
        bucket.make()
        smallObj = o.Object(o.Size._1KB)
        largeObj = o.Object(o.Size._16MB)
        bucket.put(smallObj)
        bucket.put(largeObj)
        args = ['get', smallObj.fullName(), largeObj.fullName(), bt.TESTSUITE_OUT_DIR]
        result = self.execute(args)
        self.assertEqual(result.returncode, xcodes.EX_OK, msg=result.stdout)
        # verify the object was downloaded and its size is right
        fpathSmall = f'{bt.TESTSUITE_OUT_DIR}/{smallObj.name()}'
        self.assertTrue(os.path.exists(fpathSmall), msg=msg.Message.notFound(smallObj.name(), bt.TESTSUITE_OUT_DIR))
        fpathLarge = f'{bt.TESTSUITE_OUT_DIR}/{largeObj.name()}'
        self.assertTrue(os.path.exists(fpathLarge), msg=msg.Message.notFound(largObj.name(), bt.TESTSUITE_OUT_DIR))

        self.assertEqual(bt._1KB, os.path.getsize(fpathSmall), msg=msg.Message.mismatchSize(fpathSmall))
        self.assertEqual(bt._16MB, os.path.getsize(fpathLarge), msg=msg.Message.mismatchSize(fpathLarge))

    '''=== End of get object section ==='''
        
    def test_delete(self):
        bucket = b.Bucket(1)
        bucket.make()
        obj = o.Object(o.Size._1KB)
        bucket.put(obj)
        args = ['del', obj.fullName()]
        result = self.execute(args)
        self.assertEqual(result.returncode, xcodes.EX_OK, msg=result.stdout)
        self.assertFalse(bucket.isContain(obj.name()), msg=msg.Message.found(obj.name(), bucket.fullName()))

    @unittest.skip
    def test_delete_multipart(self):
        bucket = b.Bucket(1)
        bucket.make()
        obj = o.Object(o.Size._16MB)
        bucket.put(obj)
        args = ['del', obj.fullName()]
        result = self.execute(args)
        self.assertEqual(result.returncode, xcodes.EX_OK, msg=result.stdout)
        self.assertFalse(bucket.isContain(obj.name()), msg=msg.Message.found(obj.name(), bucket.fullName()))

    def test_remove(self):
        bucket = b.Bucket(1)
        bucket.make()
        obj = o.Object(o.Size._1KB)
        bucket.put(obj)
        args = ['rm', obj.fullName()]
        result = self.execute(args)
        self.assertEqual(result.returncode, xcodes.EX_OK, msg=result.stdout)
        self.assertFalse(bucket.isContain(obj.name()), msg=msg.Message.found(obj.name(), bucket.fullName()))

    @unittest.skip
    def test_remove_multipart(self):
        bucket = b.Bucket(1)
        bucket.make()
        obj = o.Object(o.Size._16MB)
        bucket.put(obj)
        args = ['rm', obj.fullName()]
        result = self.execute(args)
        self.assertEqual(result.returncode, xcodes.EX_OK, msg=result.stdout)
        self.assertFalse(bucket.isContain(obj.name()), msg=msg.Message.found(obj.name(), bucket.fullName()))

    def test_delete_all_recursively(self):
        bucket = b.Bucket(1)
        bucket.make()
        obj1 = o.Object(o.Size._1KB)
        obj2 = o.Object(o.Size._16MB)
        bucket.put(obj1)
        bucket.put(obj2)
        args = ['del', '--recursive', '--force', bucket.fullName()]
        result = self.execute(args)
        self.assertEqual(result.returncode, xcodes.EX_OK, msg=result.stdout)
        self.assertTrue(bucket.isEmpty(), msg=msg.Message.notEmpty(bucket.fullName()))

    def test_remove_all_recursively(self):
        bucket = b.Bucket(1)
        obj1 = o.Object(o.Size._1KB)
        bucket.put(obj1)
        obj2 = o.Object(o.Size._16MB)
        bucket.put(obj2)
        args = ['rm', '--recursive', '--force', bucket.fullName()]
        result = self.execute(args)
        self.assertEqual(result.returncode, xcodes.EX_OK, msg=result.stdout)
        self.assertTrue(bucket.isEmpty(), msg=msg.Message.notEmpty(bucket.fullName()))

    '''>>> End of delete section <<<'''

    '''<<< Copy object section >>>''' 

    def test_copy_between_buckets(self):
        srcBucket = b.Bucket(1)
        srcBucket.make()
        destBucket = b.Bucket(2)
        destBucket.make()
        obj = o.Object(o.Size._1KB)
        srcBucket.put(obj)
        args = ['cp', obj.fullName(), destBucket.fullName()]
        result = self.execute(args)
        self.assertEqual(result.returncode, xcodes.EX_OK, msg=result.stdout)
        obj.setBucket(destBucket)
        self.assertTrue(destBucket.isContain(obj.name()),
            msg=msg.Message.notInDest(obj.name(), destBucket.fullName()))

    @unittest.skip
    def test_copy_multipart_between_buckets(self):
        srcBucket = b.Bucket(1)
        srcBucket.make()
        destBucket = b.Bucket(2)
        destBucket.make()
        obj = o.Object(o.Size._16MB)
        srcBucket.put(obj)

        args = ['cp', '--multipart-chunk-size-mb=5', obj.fullName(), destBucket.fullName()]
        result = self.execute(args)
        self.assertEqual(result.returncode, xcodes.EX_OK, msg=result.stdout)
        obj.setBucket(destBucket)
        self.assertTrue(destBucket.isContain(obj.name()),
            msg=msg.Message.notInDest(obj.name(), destBucket.fullName()))

    '''>>> End of copy section <<<'''

    '''<<< Move object section >>>''' 
    def test_move(self):
        srcBucket = b.Bucket(1)
        srcBucket.make()
        destBucket = b.Bucket(2)
        destBucket.make()
        obj = o.Object(o.Size._1KB)
        srcBucket.put(obj)

        args = ['mv', obj.fullName(), destBucket.fullName()]
        result = self.execute(args)
        self.assertEqual(result.returncode, xcodes.EX_OK, msg=result.stdout)
        self.assertTrue(destBucket.isContain(obj.name()),
            msg=msg.Message.notInDest(obj.name(), destBucket.fullName()))
        self.assertFalse(srcBucket.isContain(obj.name()),
            msg=msg.Message.inSource(obj.name(), srcBucket.fullName()))

    def test_move_multi(self):
        srcBucket = b.Bucket(1)
        srcBucket.make()
        destBucket = b.Bucket(2)
        destBucket.make()
        obj1 = o.Object(o.Size._1KB)
        obj2 = o.Object(o.Size._1MB)
        srcBucket.put(obj1)
        srcBucket.put(obj2)

        args = ['mv', obj1.fullName(), obj2.fullName(), destBucket.fullName()]
        result = self.execute(args)
        self.assertEqual(result.returncode, xcodes.EX_OK, msg=result.stdout)

        self.assertTrue(destBucket.isContain(obj1.name()), msg=msg.Message.notInDest(obj1.name(), destBucket.fullName()))
        self.assertTrue(destBucket.isContain(obj2.name()), msg=msg.Message.notInDest(obj2.name(), destBucket.fullName()))
        self.assertFalse(srcBucket.isContain(obj1.name()), msg=msg.Message.inSource(obj1.name(), srcBucket.fullName()))
        self.assertFalse(srcBucket.isContain(obj2.name()), msg=msg.Message.inSource(obj2.name(), srcBucket.fullName()))

    '''>>> End of move section <<<'''

if __name__ == '__main__':
    unittest.main()
