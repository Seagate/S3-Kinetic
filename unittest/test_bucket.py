from test import *

class TestBucket(Test):
    def test_make(self):
        # create a single bucket
        args = ['mb', '--bucket-location=EU', '%s%s'%(S3, bucketName(1))]
        completed_p = self.execute(args)
        self.assertEqual(completed_p.returncode, EX_OK)

        # create a bucket with invalid name
        args = ['mb', '--bucket-location=EU', '%s%s'%(S3, bucketName('EU'))]
        completed_p = self.execute(args)
        self.assertEqual(completed_p.returncode, EX_USAGE)

        # create multiple buckets
        args = ['mb', '%s%s'%(S3, bucketName(2)), '%s%s'%(S3, bucketName(3))]
        completed_p = self.execute(args)
        self.assertEqual(completed_p.returncode, EX_OK)

        # create an exising bucket
        args = ['mb', '%s%s'%(S3, bucketName(2))]
        completed_p = self.execute(args)
        self.assertEqual(completed_p.returncode, EX_CONFLICT)

    def test_list(self):
        args = ['ls']
        completed_p = self.execute(args)
        self.assertEqual(completed_p.returncode, EX_OK)

    def test_list_all(self):
        args = ['la']
        completed_p = self.execute(args)
        self.assertEqual(completed_p.returncode, EX_OK)

    def test_disk_usage(self):
        args = ['du', '%s%s'%(S3, bucketName(1))]
        completed_p = self.execute(args)
        self.assertEqual(completed_p.returncode, EX_OK)

    def test_remove(self):
        # remove a single empty bucket
        args = ['rb', '%s%s'%(S3, bucketName(1))]
        completed_p = self.execute(args)
        self.assertEqual(completed_p.returncode, EX_OK)
        # remove multiple empty buckets
        args = ['rb', '%s%s'%(S3, bucketName(2)), '%s%s'%(S3, bucketName(3))]
        completed_p = self.execute(args)
        self.assertEqual(completed_p.returncode, EX_OK)

        ### remove non-empty bucket ###
        # 1. create a bucket
        args = ['mb', '%s%s'%(S3, bucketName(1))]
        completed_p = self.execute(args)
        self.assertEqual(completed_p.returncode, EX_OK)

        # 2. put an object into the bucket to make them non-empty then remove it
        args = ['put', '%s'%(get_1MB_fpath()), '%s%s'%(S3, bucketName(1))]
        completed_p = self.execute(args)
        self.assertEqual(completed_p.returncode, EX_OK)

        # remove a non-empty bucket
        args = ['rb', '%s%s'%(S3, bucketName(1))]
        completed_p = self.execute(args)
        self.assertEqual(completed_p.returncode, EX_CONFLICT)

        args = ['rb', '--recursive', '%s%s'%(S3, bucketName(1))]
        completed_p = self.execute(args)
        self.assertEqual(completed_p.returncode, EX_OK)

if __name__ == '__main__':
    unittest.main()

