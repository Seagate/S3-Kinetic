import os
import sys
import unittest

# local imports
import base_test as bt # required to see PATH_TO_S3CMD used in the next instruction
if bt.PATH_TO_S3CMD not in sys.path:
    sys.path.append(bt.PATH_TO_S3CMD) # required to see S3.ExitCodes

import S3.ExitCodes as xcodes

# local imports
import base_test as bt
import bucket as b
import file_system
import message as msg
import object as o

import S3.ExitCodes as xcodes

class TestObject(bt.BaseTest):
    """Test s3cmd APIs related to objects.

       This class contains several test functions which
       verify object-related s3cmd operations, including:

        * Putting object(s) into a bucket
        * Getting (downloading) object(s) from a bucket
        * Moving object(s) between different buckets
        * Deleting object(s)
        * Renaming object(s)

       The tests within this class aim to cover all possible
       combinations of the operations listed above, on both
       simple and larger (multipart) objects.

       Each test function does the following:

        1. Create temporary bucket(s) and object(s) and arrange them
           according to the test's preconditions.
             Ex: test_put() begins by creating one bucket and one object

        2. Execute a single s3cmd command.
             Ex: test_put() executes "s3cmd put <OBJECT> s3://<BUCKET>"

        3. Perform one or more assertions according to expected
           postconditions.
             Ex: test_put() asserts that <OBJECT> exists in <BUCKET>

        4. Delete all buckets/objects created in step 1.
             Note: This step does not directly appear in the test
                   functions in this file; it is an inherited method
                   (tearDown) which is automatically called after
                   every test function executes.
    """

    @classmethod
    def setUpClass(self):
        super().setUpClass()
        file_system.makeDownloadDir()

    def test_put_rename(self):
        """ Put an object to a bucket, while renaming the object """
        bucket = b.Bucket(1)
        bucket.make()
        obj = o.Object(file_system.Size._1KB)
        objNewName = "test.bin"
        objOldName = obj.name()

        # call command "put <OBJECT> s3://bucket/test.bin"
        args = ['put', obj.fullFileName(),
                 bucket.fullName() + "/" + objNewName]
        result = self.execute(args)
        self.assertEqual(result.returncode, xcodes.EX_OK, msg=result.stdout)

        # assert object can be found in bucket and is renamed
        self.assertTrue(bucket.doesContain(objNewName),
                        msg=msg.Message.notFound(objNewName,
                        bucket.fullName()))
        self.assertFalse(bucket.doesContain(objOldName),
                         msg="Object put but not renamed")

    def test_get_wd(self):
        """ Download an object to the current working directory """
        bucket = b.Bucket(1)
        bucket.make()
        obj = o.Object(file_system.Size._1KB)
        bucket.put(obj)

        # call command "get --force s3://bucket/<OBJECT>"
        args = ['get', '--force', obj.fullName()]
        result = self.execute(args)
        self.assertEqual(result.returncode, xcodes.EX_OK, msg=result.stdout)

        # assert object is present in working directory, delete for cleanup
        self.assertTrue(os.path.exists(obj.name()),
                        msg=msg.Message.notFound(obj.name(), os.getcwd()))
        if(os.path.exists(obj.name())):
            os.remove(obj.name())

    def test_get_wd_rename(self):
        """ Download an object to the current working directory
            while renaming the downloaded copy """
        bucket = b.Bucket(1)
        bucket.make()
        obj = o.Object(file_system.Size._1KB)
        bucket.put(obj)
        objNewName = "test.bin"
        objOldName = obj.name()

        # call command "get --force s3://bucket/<OBJECT> test.bin"
        args = ['get', '--force', obj.fullName(), objNewName]
        result = self.execute(args)
        self.assertEqual(result.returncode, xcodes.EX_OK, msg=result.stdout)

        # assert object was downloaded to working directory and renamed
        self.assertTrue(os.path.exists(objNewName),
                        msg=msg.Message.notFound(objNewName, os.getcwd()))
        self.assertFalse(os.path.exists(objOldName),
                         msg="Object downloaded but not renamed")
        # clean up by deleting downloaded object
        if(os.path.exists(objNewName)):
            os.remove(objNewName)

    def test_move_rename(self):
        """Move object from one bucket to another, while also renaming it."""
        srcBucket = b.Bucket(1)
        srcBucket.make()
        destBucket = b.Bucket(2)
        destBucket.make()
        obj = o.Object(file_system.Size._1KB)
        srcBucket.put(obj)
        objNewName = "test.bin"
        objOldName = obj.name()

        # call command "mv s3://srcBucket/<OBJECT> s3://destBucket/test.bin"
        args = ['mv', obj.fullName(), destBucket.fullName() + "/test.bin"]
        result = self.execute(args)
        self.assertEqual(result.returncode, xcodes.EX_OK, msg=result.stdout)

        # assert that object is renamed and only present in destination bucket
        self.assertTrue(destBucket.doesContain(objNewName),
                        msg=msg.Message.notInDest(objNewName,
                        destBucket.fullName()))
        self.assertFalse(srcBucket.doesContain(objNewName),
                         msg=msg.Message.inSource(objNewName,
                         srcBucket.fullName()))
        # assert that object's old name is not present in either bucket
        self.assertFalse(srcBucket.doesContain(objOldName),
                         msg=msg.Message.inSource(objOldName,
                         srcBucket.fullName()))
        self.assertFalse(destBucket.doesContain(obj.name()),
                         msg="Object moved but not renamed")

    def test_put_rename_multipart(self):
        """ Put a multipart object to a bucket, while also renaming it """
        bucket = b.Bucket(1)
        bucket.make()
        obj = o.Object(file_system.Size._16MB)
        objOldName = obj.name()
        objNewName = "test.bin"

        # call command "put --multipart ... <OBJECT> s3://<BUCKET>/test.bin"
        args = ['put', '--multipart-chunk-size-mb=5', obj.fullFileName(),
                bucket.fullName() + "/" + objNewName]
        result = self.execute(args)
        self.assertEqual(result.returncode, xcodes.EX_OK, msg=result.stdout)

        # verify the file was stored and renamed
        obj.setBucket(bucket)
        self.assertTrue(bucket.doesContain(objNewName),
                        msg=msg.Message.notFound(obj.name(),
                        bucket.fullName()))
        self.assertFalse(bucket.doesContain(objOldName),
                         msg="Object not renamed")

    def test_get_wd_multipart(self):
        """ Download a multipart object to the working directory """
        bucket = b.Bucket(1)
        bucket.make()
        obj = o.Object(file_system.Size._16MB)
        bucket.put(obj)

        # execute command "get --force s3://<BUCKET>/<MULTIPART_OBJECT>"
        args = ['get', '--force', obj.fullName()]
        result = self.execute(args)
        self.assertEqual(result.returncode, xcodes.EX_OK, msg=result.stdout)

        # verify the object was downloaded to working dir + size is correct
        self.assertTrue(os.path.exists(obj.name()),
                        msg=msg.Message.notFound(obj.name(), os.getcwd()))
        self.assertEqual(file_system.Size._16MB, os.path.getsize(obj.name()),
                         msg=msg.Message.sizeMismatch(os.getcwd()))

        # TODO: add file comparison assertion(s)

        # clean up
        if(os.path.exists(obj.name())):
            os.remove(obj.name())

    def test_get_wd_rename_multipart(self):
        """ Download a multipart object to the working directory, while
            specifying a new name for the downloaded obejct  """
        bucket = b.Bucket(1)
        bucket.make()
        obj = o.Object(file_system.Size._16MB)
        bucket.put(obj)
        objNewName = "test.bin"
        objOldName = obj.name()

        # execute command "get --force s3://<BUCKET>/<OBJECT> <OBJECT_RENAMED>"
        args = ['get', '--force', obj.fullName(), objNewName]
        result = self.execute(args)
        self.assertEqual(result.returncode, xcodes.EX_OK, msg=result.stdout)

        # assert object was downloaded to working directory and renamed
        self.assertTrue(os.path.exists(objNewName),
                        msg=msg.Message.notFound(objNewName, os.getcwd()))
        self.assertFalse(os.path.exists(objOldName),
                         msg="Object downloaded but not renamed")

        # TODO: add file comparison assertion(s)

        # clean up by deleting downloaded object
        if(os.path.exists(objNewName)):
            os.remove(objNewName)

    def test_rename_multipart(self):
        """ Rename a multipart object (without moving it to another bucket)"""
        bucket = b.Bucket(1)
        bucket.make()
        obj = o.Object(file_system.Size._16MB)
        bucket.put(obj)
        objNewName = "test.bin"
        objOldName = obj.name()

        # call command "mv s3://bucket/<OBJECT> s3://bucket/test.bin"
        args = ['mv', obj.fullName(), bucket.fullName() + "/test.bin"]
        result = self.execute(args)
        self.assertEqual(result.returncode, xcodes.EX_OK, msg=result.stdout)

        # assert that object is renamed and old name no longer present
        self.assertTrue(bucket.doesContain(objNewName),
                        msg=msg.Message.notInDest(objNewName,
                        bucket.fullName()))
        self.assertFalse(bucket.doesContain(objOldName),
                         msg=msg.Message.inSource(objOldName,
                         bucket.fullName()))

    def test_move_rename_multipart(self):
        """ Move a multipart object from one bucket to another,
            while also renaming it  """
        srcBucket = b.Bucket(1)
        srcBucket.make()
        destBucket = b.Bucket(2)
        destBucket.make()
        obj = o.Object(file_system.Size._16MB)
        srcBucket.put(obj)
        objNewName = "test.bin"
        objOldName = obj.name()

        # call command "mv s3://srcBucket/<OBJECT> s3://destBucket/test.bin"
        args = ['mv', obj.fullName(), destBucket.fullName() + "/test.bin"]
        result = self.execute(args)
        self.assertEqual(result.returncode, xcodes.EX_OK, msg=result.stdout)

        # assert that object is renamed and only present in destination bucket
        self.assertTrue(destBucket.doesContain(objNewName),
                        msg=msg.Message.notInDest(objNewName,
                        destBucket.fullName()))
        self.assertFalse(srcBucket.doesContain(objNewName),
                         msg=msg.Message.inSource(objNewName,
                         srcBucket.fullName()))
        # assert that object's old name is not present in either bucket
        self.assertFalse(srcBucket.doesContain(objOldName),
                         msg=msg.Message.inSource(objOldName,
                         srcBucket.fullName()))
        self.assertFalse(destBucket.doesContain(obj.name()),
                         msg="Object moved but not renamed")

    def test_put(self):
        bucket = b.Bucket(1)
        bucket.make()
        obj = o.Object(file_system.Size._1KB)
        args = ['put', obj.fullFileName(), bucket.fullName()]
        result = self.execute(args)
        self.assertEqual(result.returncode, xcodes.EX_OK, msg=result.stdout)
        # verify the file was stored
        self.assertTrue(bucket.doesContain(obj.name()), msg=msg.Message.notFound(obj.name(),
            bucket.fullName()))

    def test_put_stdin(self):
        """Put an object into a bucket by reading the object from stdin."""
        bucket = b.Bucket(1)
        bucket.make()
        obj = o.Object(file_system.Size._1KB)
        objFullName = f'{bucket.fullName()}/{obj.name()}'
        f = open(obj.fullFileName(), 'r')
        args = ['put', '-', objFullName]
        result = self.execute(args, stdin=f)
        f.close()
        self.assertEqual(result.returncode, xcodes.EX_OK, msg=result.stdout)
        # verify the file was stored
        self.assertTrue(bucket.doesContain(obj.name()), msg=msg.Message.notFound(obj.name(),
            bucket.fullName()))

    def test_put_multipart(self):
        bucket = b.Bucket(1)
        bucket.make()
        obj = o.Object(file_system.Size._16MB)
        args = ['put', '--multipart-chunk-size-mb=5', obj.fullFileName(), bucket.fullName()]
        result = self.execute(args)
        self.assertEqual(result.returncode, xcodes.EX_OK, msg=result.stdout)
        # verify the file was stored
        obj.setBucket(bucket)
        self.assertTrue(bucket.doesContain(obj.name()), msg=msg.Message.notFound(obj.name(),
            bucket.fullName()))

    def test_put_multipart_stdin(self):
        """Put a multipart object into a bucket by reading the object from stdin."""
        bucket = b.Bucket(1)
        bucket.make()
        obj = o.Object(file_system.Size._16MB)
        objFullName = f'{bucket.fullName()}/{obj.name()}'
        f = open(obj.fullFileName(), 'r')
        args = ['put', '--multipart-chunk-size-mb=5', '-', objFullName]
        result = self.execute(args, stdin=f)
        f.close()
        self.assertEqual(result.returncode, xcodes.EX_OK, msg=result.stdout)
        # verify the file was stored
        self.assertTrue(bucket.doesContain(obj.name()), msg=msg.Message.notFound(obj.name(),
            bucket.fullName()))

    def test_put_multi(self):
        bucket = b.Bucket(1)
        bucket.make()
        obj1 = o.Object(file_system.Size._1KB)
        obj2 = o.Object(file_system.Size._1MB)
        args = ['put', obj1.fullFileName(), obj2.fullFileName(), bucket.fullName()]
        result = self.execute(args)
        self.assertEqual(result.returncode, xcodes.EX_OK, msg=result.stdout)
        obj1.setBucket(bucket)
        obj2.setBucket(bucket)
        # verify the file was stored
        self.assertTrue(bucket.doesContain(obj1.name()), msg=msg.Message.notFound(obj1.name(),
            bucket.fullName()))
        self.assertTrue(bucket.doesContain(obj2.name()), msg=msg.Message.notFound(obj2.name(),
            bucket.fullName()))

    @unittest.skip
    def test_put_recursive(self):
        """Recursively put all objects in a file directory."""
        bucket = b.Bucket("recursive")
        bucket.make()
        args =['put', '--recursive', '--multipart-chunk-size-mb=5', \
            bt.DAT_DIR, bucket.fullName()]
        result = self.execute(args)
        self.assertEqual(result.returncode, xcodes.EX_OK, msg=result.stdout)
        # verify all files were uploaded
        args = ['la', bucket]
        result = self.execute(args)
        self.assertEqual(result.returncode, xcodes.EX_OK, msg=result.stdout)
        files = os.listdir(bt.DAT_DIR)
        for f in files:
            self.assertNotEqual(result.stdout.find(f), -1,
                msg=msg.Message.notFound(f, bt.DAT_DIR))

    def test_get(self):
        bucket = b.Bucket(1)
        bucket.make()
        obj = o.Object(file_system.Size._1KB)
        bucket.put(obj)
        args = ['get', '--force', obj.fullName(), file_system.DOWNLOAD_DIR]
        result = self.execute(args)
        self.assertEqual(result.returncode, xcodes.EX_OK, msg=result.stdout)
        # verify the object was downloaded
        self.assertTrue(os.path.exists(f'{file_system.DOWNLOAD_DIR}/{obj.name()}'),
            msg=msg.Message.notFound(obj.name(), file_system.DOWNLOAD_DIR))

    def test_get_multipart(self):
        bucket = b.Bucket(1)
        bucket.make()
        obj = o.Object(file_system.Size._16MB)
        bucket.put(obj)
        args = ['get', '--force', obj.fullName(), file_system.DOWNLOAD_DIR]
        result = self.execute(args)
        self.assertEqual(result.returncode, xcodes.EX_OK, msg=result.stdout)
        # verify the object was downloaded and its size is right
        fpath = f'{file_system.DOWNLOAD_DIR}/{obj.name()}'
        self.assertTrue(os.path.exists(fpath),
        msg=msg.Message.notFound(obj.name(), file_system.DOWNLOAD_DIR)) 
        self.assertEqual(file_system.Size._16MB, os.path.getsize(fpath), msg=msg.Message.sizeMismatch(fpath))

    def test_get_multi(self):
        """Get multiple objects from a bucket."""
        bucket = b.Bucket(1)
        bucket.make()
        smallObj = o.Object(file_system.Size._1KB)
        largeObj = o.Object(file_system.Size._16MB)
        bucket.put(smallObj)
        bucket.put(largeObj)
        args = ['get', '--force', smallObj.fullName(), largeObj.fullName(), file_system.DOWNLOAD_DIR]
        result = self.execute(args)
        self.assertEqual(result.returncode, xcodes.EX_OK, msg=result.stdout)
        # verify the object was downloaded and its size is right
        fpathSmall = f'{file_system.DOWNLOAD_DIR}/{smallObj.name()}'
        self.assertTrue(os.path.exists(fpathSmall),
            msg=msg.Message.notFound(smallObj.name(), file_system.DOWNLOAD_DIR))
        fpathLarge = f'{file_system.DOWNLOAD_DIR}/{largeObj.name()}'
        self.assertTrue(os.path.exists(fpathLarge),
            msg=msg.Message.notFound(largeObj.name(), file_system.DOWNLOAD_DIR))

        self.assertEqual(file_system.Size._1KB, os.path.getsize(fpathSmall),
            msg=msg.Message.sizeMismatch(fpathSmall))
        self.assertEqual(file_system.Size._16MB, os.path.getsize(fpathLarge),
            msg=msg.Message.sizeMismatch(fpathLarge))

    def test_delete(self):
        bucket = b.Bucket(1)
        bucket.make()
        obj = o.Object(file_system.Size._1KB)
        bucket.put(obj)
        args = ['del', obj.fullName()]
        result = self.execute(args)
        self.assertEqual(result.returncode, xcodes.EX_OK, msg=result.stdout)
        self.assertFalse(bucket.doesContain(obj.name()),
            msg=msg.Message.found(obj.name(), bucket.fullName()))

    def test_delete_multipart(self):
        bucket = b.Bucket(1)
        bucket.make()
        obj = o.Object(file_system.Size._16MB)
        bucket.put(obj)
        args = ['del', obj.fullName()]
        result = self.execute(args)
        self.assertEqual(result.returncode, xcodes.EX_OK, msg=result.stdout)
        self.assertFalse(bucket.doesContain(obj.name()),
            msg=msg.Message.found(obj.name(), bucket.fullName()))

    def test_remove(self):
        bucket = b.Bucket(1)
        bucket.make()
        obj = o.Object(file_system.Size._1KB)
        bucket.put(obj)
        args = ['rm', obj.fullName()]
        result = self.execute(args)
        self.assertEqual(result.returncode, xcodes.EX_OK, msg=result.stdout)
        self.assertFalse(bucket.doesContain(obj.name()),
            msg=msg.Message.found(obj.name(), bucket.fullName()))

    def test_remove_multipart(self):
        bucket = b.Bucket(1)
        bucket.make()
        obj = o.Object(file_system.Size._16MB)
        bucket.put(obj)
        args = ['rm', obj.fullName()]
        result = self.execute(args)
        self.assertEqual(result.returncode, xcodes.EX_OK, msg=result.stdout)
        self.assertFalse(bucket.doesContain(obj.name()),
            msg=msg.Message.found(obj.name(), bucket.fullName()))

    def test_delete_all_recursive(self):
        """Recursively delete all objects from a bucket."""
        bucket = b.Bucket(1)
        bucket.make()
        obj1 = o.Object(file_system.Size._1KB)
        obj2 = o.Object(file_system.Size._16MB)
        bucket.put(obj1)
        bucket.put(obj2)
        args = ['del', '--recursive', '--force', bucket.fullName()]
        result = self.execute(args)
        self.assertEqual(result.returncode, xcodes.EX_OK, msg=result.stdout)
        self.assertTrue(bucket.isEmpty(), msg=msg.Message.notEmpty(bucket.fullName()))

    def test_remove_all_recursive(self):
        """Recursively remove (the same as delete) all objects from a bucket."""
        bucket = b.Bucket(1)
        bucket.make()
        obj1 = o.Object(file_system.Size._1KB)
        bucket.put(obj1)
        obj2 = o.Object(file_system.Size._16MB)
        bucket.put(obj2)
        args = ['rm', '--recursive', '--force', bucket.fullName()]
        result = self.execute(args)
        self.assertEqual(result.returncode, xcodes.EX_OK, msg=result.stdout)
        self.assertTrue(bucket.isEmpty(), msg=msg.Message.notEmpty(bucket.fullName()))

    def test_copy_between_buckets(self):
        """Copy an object from a bucket to another."""
        srcBucket = b.Bucket(1)
        srcBucket.make()
        destBucket = b.Bucket(2)
        destBucket.make()
        obj = o.Object(file_system.Size._1KB)
        srcBucket.put(obj)
        args = ['cp', obj.fullName(), destBucket.fullName()]
        result = self.execute(args)
        self.assertEqual(result.returncode, xcodes.EX_OK, msg=result.stdout)
        obj.setBucket(destBucket)
        self.assertTrue(destBucket.doesContain(obj.name()),
            msg=msg.Message.notInDest(obj.name(), destBucket.fullName()))

    def test_copy_multipart_between_buckets(self):
        """Copy a multipart object from a bucket to another."""
        srcBucket = b.Bucket(1)
        srcBucket.make()
        destBucket = b.Bucket(2)
        destBucket.make()
        obj = o.Object(file_system.Size._16MB)
        srcBucket.put(obj)

        args = ['cp', '--multipart-chunk-size-mb=5', obj.fullName(), destBucket.fullName()]
        result = self.execute(args)
        self.assertEqual(result.returncode, xcodes.EX_OK, msg=result.stdout)
        obj.setBucket(destBucket)
        self.assertTrue(destBucket.doesContain(obj.name()),
            msg=msg.Message.notInDest(obj.name(), destBucket.fullName()))

    def test_move(self):
        """Move an object from a bucket to another."""
        srcBucket = b.Bucket(1)
        srcBucket.make()
        destBucket = b.Bucket(2)
        destBucket.make()
        obj = o.Object(file_system.Size._1KB)
        srcBucket.put(obj)

        args = ['mv', obj.fullName(), destBucket.fullName()]
        result = self.execute(args)
        self.assertEqual(result.returncode, xcodes.EX_OK, msg=result.stdout)
        self.assertTrue(destBucket.doesContain(obj.name()),
            msg=msg.Message.notInDest(obj.name(), destBucket.fullName()))
        self.assertFalse(srcBucket.doesContain(obj.name()),
            msg=msg.Message.inSource(obj.name(), srcBucket.fullName()))

    def test_move_multi(self):
        """Move multiple objects from a bucket to another."""
        srcBucket = b.Bucket(1)
        srcBucket.make()
        destBucket = b.Bucket(2)
        destBucket.make()
        obj1 = o.Object(file_system.Size._1KB)
        obj2 = o.Object(file_system.Size._1MB)
        srcBucket.put(obj1)
        srcBucket.put(obj2)

        args = ['mv', obj1.fullName(), obj2.fullName(), destBucket.fullName()]
        result = self.execute(args)
        self.assertEqual(result.returncode, xcodes.EX_OK, msg=result.stdout)

        self.assertTrue(destBucket.doesContain(obj1.name()), msg=msg.Message.notInDest(obj1.name(), destBucket.fullName()))
        self.assertTrue(destBucket.doesContain(obj2.name()), msg=msg.Message.notInDest(obj2.name(), destBucket.fullName()))
        self.assertFalse(srcBucket.doesContain(obj1.name()), msg=msg.Message.inSource(obj1.name(), srcBucket.fullName()))
        self.assertFalse(srcBucket.doesContain(obj2.name()), msg=msg.Message.inSource(obj2.name(), srcBucket.fullName()))

if __name__ == '__main__':
    unittest.main()
