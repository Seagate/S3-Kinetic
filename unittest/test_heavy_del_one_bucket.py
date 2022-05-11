import sys
import threading
import unittest

# local imports
import base_test as bt
import cmd_operator as co
import object_producer
import s3bucket

class TestHeavyDelOneBucket(bt.BaseTest):
    """Test S3-Kinetic by having many threads to delete s3objects from only ONE bucket"""

    NUM_THREADS = 100
    NUM_OPS = 50

    def setUp(self):
        """Setup data for the test

        This method creates multiple objects within only one bucket.
        """

        self.__mutex = threading.Lock()

        # Create a bucket and its objects
        self.__bucket = s3bucket.S3Bucket(1)
        self.__bucket.make()
        self.__objectProducer = object_producer.ObjectProducer([self.__bucket])
        self.__objectProducer.makeAll(self.NUM_OPS*self.NUM_THREADS)
        
    def test_heavy_del(self):
        """Parallel delete all created objects within a bucket"""

        operatorList = [] 

        # Launch DELETE threads
        print(f'Starting {self.NUM_THREADS} DELETE threads, {self.NUM_OPS} DELETEs per thread...')
        bucketList = [self.__bucket]
        for i in range(0, self.NUM_THREADS):
            name = f'thread-{i}'
            operator = co.CmdOperator(name, co.Type.DEL, self.NUM_OPS)
            operator.setBuckets(bucketList)
            operator.setObjectProducer(self.__objectProducer)
            operatorList.append(operator)
            operator.start()

        # Wait for all threads complete
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
