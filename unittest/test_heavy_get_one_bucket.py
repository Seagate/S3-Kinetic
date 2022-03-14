import sys
import unittest

# local imports
import base_test as bt
import bucket as b
import cmd_operator as co
import in_file_factory as ff 
import object as o

class TestHeavyGetOneBucket(bt.BaseTest):
    """Test S3-Kinetic with heavy gets with one bucket"""

    NUM_THREADS = 1
    NUM_OPS = 10 
    
    def setUp(self):
        """Setup input files for the test.

            Creates a collection of files with different sizes and puts all of them into one bucket
        """

        # Create upload files       
        factory = ff.InFileFactory()
        factory.makeAll()
        # Upload files to one bucket
        self.__bucket = b.Bucket(1)
        self.__bucket.make()
        for size in ff.DATA_FILES.keys():
            obj = o.Object(size)
            self.__bucket.put(obj)
         
    def test_heavy_get(self):
        """Execute heavy gets with one bucket

            Launches a fixed number of threads (NUM_THREADS), each thread makes n PUT
            operations in the given bucket. The test stops when all threads perform 
            successfully or on the first error.
        """

        bt.makeDownloadDir()
        operatorList = [] 

        # Launch threads
        print(f'Starting {self.NUM_THREADS} GET threads, {self.NUM_OPS} GETs per thread...')
        bucketList = [self.__bucket]
        for i in range(0, self.NUM_THREADS):
            operator = co.CmdOperator(f'thread-{i}', co.Type.GET, self.NUM_OPS)
            operator.setBuckets(bucketList)
            operatorList.append(operator)
            operator.start()

        # Wait for all PUT threads complete
        print(f'Waiting for completion.  Be patient...')
        error = None

        for operator in operatorList:
            operator.join()
            # check for error
            if error == None:
                error = operator.getError()

        print(f'All threads complete')
        self.assertEqual(error, None, msg=f'{error}')


if __name__ == '__main__':
    unittest.main()
