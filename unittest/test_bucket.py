import sys
import unittest

import base_test as bt # require base_test to see PATH_TO_S3CMD
if bt.PATH_TO_S3CMD not in sys.path:
    sys.path.append(bt.PATH_TO_S3CMD) # required to see S3.ExitCodes
import S3.ExitCodes as xcodes

# local imports
import file_system
import message as msg
import s3bucket
import s3object

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
        bucket = s3bucket.S3Bucket(1)
        args = ['mb', bucket.fullName()]
        result = self.execute(args)
        self.assertEqual(result.returncode, xcodes.EX_OK, msg=result.stdout)
        self.assertTrue(bucket.doesExist(), msg=msg.Message.notFound(bucket.fullName()))

    def test_make_invalid(self):
        bucket = s3bucket.S3Bucket('EU')
        args = ['mb', '--bucket-location=EU', bucket.fullName()]
        result = self.execute(args)
        self.assertEqual(result.returncode, xcodes.EX_USAGE, msg=result.stdout)
        self.assertFalse(bucket.doesExist(), msg=msg.Message.found(bucket.fullName()))

    def test_make_multi(self):
        # create 2 buckets
        bucket1 = s3bucket.S3Bucket(1)
        bucket2 = s3bucket.S3Bucket(2)
        args = ['mb', bucket1.fullName(), bucket2.fullName()]
        result = self.execute(args)
        self.assertEqual(result.returncode, xcodes.EX_OK, msg=result.stdout)
        self.assertTrue(bucket1.doesExist(), msg=msg.Message.notFound(bucket1.fullName()))
        self.assertTrue(bucket2.doesExist(), msg=msg.Message.notFound(bucket2.fullName()))

    def test_make_exist_bucket(self):
        bucket = s3bucket.S3Bucket(1)
        bucket.make()
        # remake the same bucket
        args = ['mb', bucket.fullName()]
        result = self.execute(args)
        self.assertEqual(result.returncode, xcodes.EX_CONFLICT, msg=result.stdout)

    def test_list_buckets(self):
        ''' List all buckets (no contents) '''
        # make a bucket
        bucket = s3bucket.S3Bucket(1)
        bucket.make()
        # list
        args = ['ls']
        result = self.execute(args)
        self.assertEqual(result.returncode, xcodes.EX_OK, msg=result.stdout)
        self.assertTrue(result.stdout.find(bucket.fullName()) != -1, msg=msg.Message.notFound(bucket.fullName()))

    def test_list_bucket_contents(self):
        ''' List contents of a single bucket '''
        # make a bucket
        bucket = s3bucket.S3Bucket(1)
        bucket.make()
        # put an object to the bucket
        obj = s3object.S3Object(file_system.Size._1MB)
        bucket.put(obj)

        # execute, assert that the object is listed
        args = ['ls', bucket.fullName()]
        result = self.execute(args)
        self.assertEqual(result.returncode, xcodes.EX_OK, msg=result.stdout)
        self.assertTrue(result.stdout.find(obj.fullName()) != -1, msg=msg.Message.notFound(obj.name(), bucket.fullName()))

    def test_list_all_objects(self):
        ''' List all objects in all buckets '''
        # make two buckets
        bucket1 = s3bucket.S3Bucket(1)
        bucket1.make()
        bucket2 = s3bucket.S3Bucket(2)
        bucket2.make()

        # put an object to each bucket
        obj1 = s3object.S3Object(file_system.Size._1MB)
        bucket1.put(obj1)
        obj2 = s3object.S3Object(file_system.Size._1MB)
        bucket2.put(obj2)

        # execute command, assert that both objects are listed by la
        args = ['la']
        result = self.execute(args)
        self.assertEqual(result.returncode, xcodes.EX_OK, msg=result.stdout)
        self.assertTrue(result.stdout.find(obj1.fullName()) != -1, msg=msg.Message.notFound(obj1.name(), bucket1.fullName()))
        self.assertTrue(result.stdout.find(obj2.fullName()) != -1, msg=msg.Message.notFound(obj2.name(), bucket2.fullName()))

    def test_disk_usage(self):
        ''' Report disk usage of a bucket '''
        # make a bucket
        bucket = s3bucket.S3Bucket(1)
        bucket.make()
        # put an object to the bucket
        obj1 = s3object.S3Object(file_system.Size._1MB)
        obj2 = s3object.S3Object(file_system.Size._1MB)
        bucket.put(obj1, f'{obj1.name()}_1')
        bucket.put(obj2, f'{obj2.name()}_2')
        # disk usage
        args = ['du', bucket.fullName()]
        result = self.execute(args)
        self.assertEqual(result.returncode, xcodes.EX_OK, msg=result.stdout)
        self.assertNotEqual(result.stdout.find(str(2*file_system.Size._1MB)), -1, msg=msg.Message.wrongDiskUsage())

    def test_remove_single_empty(self):
        # make a bucket
        bucket = s3bucket.S3Bucket(1)
        bucket.make()
        # remove bucket
        args = ['rb', bucket.fullName()]
        result = self.execute(args)
        self.assertEqual(result.returncode, xcodes.EX_OK, msg=result.stdout)
        # verify bucket removed
        self.assertFalse(bucket.doesExist(), msg=msg.Message.found(bucket.fullName()))

    def test_remove_multi_empty(self):
        # create 2 bucket
        bucket1 = s3bucket.S3Bucket(1)
        bucket2 = s3bucket.S3Bucket(2)
        bucket1.make()
        bucket2.make()
        # remove multiple empty buckets
        args = ['rb', bucket1.fullName(), bucket2.fullName()]
        result = self.execute(args)
        self.assertEqual(result.returncode, xcodes.EX_OK, msg=result.stdout)
        # verify buckets removed
        self.assertFalse(bucket1.doesExist(), msg=msg.Message.found(bucket1.fullName()))
        self.assertFalse(bucket2.doesExist(), msg=msg.Message.found(bucket2.fullName()))

    def test_remove_single_non_empty(self):
        # create a bucket
        bucket = s3bucket.S3Bucket(1)
        bucket.make()
        # put an object to the bucket
        obj = s3object.S3Object(file_system.Size._1MB)
        bucket.put(obj)
        # remove the bucket
        args = ['rb', '--recursive', bucket.fullName()]
        result = self.execute(args)
        self.assertEqual(result.returncode, xcodes.EX_OK, msg=result.stdout)
        # verify bucket removed
        self.assertFalse(bucket.doesExist(), msg=msg.Message.notFound(bucket.fullName()))

    def test_remove_multi_non_empty(self):
        # create 2 bucket
        bucket1 = s3bucket.S3Bucket(1)
        bucket2 = s3bucket.S3Bucket(2)
        bucket1.make()
        bucket2.make()
        # put an object to the buckets
        obj1 = s3object.S3Object(file_system.Size._1MB)
        obj2 = s3object.S3Object(file_system.Size._1MB)
        bucket1.put(obj1)
        bucket2.put(obj2)
        # remove those 2 buckets
        args = ['rb', '--recursive', bucket1.fullName(), bucket2.fullName()]
        result = self.execute(args)
        self.assertEqual(result.returncode, xcodes.EX_OK, msg=result.stdout)
        # verify bucket removed
        self.assertFalse(bucket1.doesExist(), msg=msg.Message.found(bucket1.fullName()))
        self.assertFalse(bucket2.doesExist(), msg=msg.Message.found(bucket2.fullName()))

if __name__ == '__main__':
    unittest.main()
