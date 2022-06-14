import sys
import unittest

# local imports
import base_test as bt
import cmd_operator as co
import file_system
import s3bucket
import s3object

class TestHeavyGetOneBucket(bt.BaseTest):
    """Test S3-Kinetic with heavy gets with one bucket

        This test generates many GET commands from many threads
    """

    NUM_THREADS = 100
    NUM_OPS = 200
    
    def setUp(self):
        """Setup input files for the test.

            Creates a collection of files with different sizes and puts all of them into one bucket
        """

        # Create upload files       
        fileProducer = file_system.FileProducer()
        fileProducer.makeAll()
        # Upload files to one bucket
        self.__bucket = s3bucket.S3Bucket(1)
        self.__bucket.make()
        for size in file_system.DATA_FILES.keys():
            obj = s3object.S3Object(size)
            self.__bucket.put(obj)
         
    def test_heavy_get(self):
        """Execute heavy gets with one bucket

            Launches a fixed number of threads (NUM_THREADS), each thread makes n PUT
            operations in the given bucket. The test stops when all threads perform 
            successfully or on the first error.
        """

        file_system.makeDownloadDir()
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
                if error != None:
                    print(f'Error: {error}')

        print(f'All threads complete')
        self.assertEqual(error, None, msg=f'{error}')


if __name__ == '__main__':
    unittest.main()
