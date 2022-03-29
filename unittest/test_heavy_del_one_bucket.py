import sys
import threading
import unittest

# local imports
import base_test as bt
import bucket as b
import cmd_operator as co
#import file_system
import object as o
import obj_factory as of

class TestHeavyDelOneBucket(bt.BaseTest):
    """Test S3-Kinetic with heavy deletes with one bucket"""

    NUM_THREADS = 5
    NUM_OPS = 10 
    NUM_SUFFIX = 10

    def setUp(self):
        """setup data for the test"""

        #self.__nxtName = None 
        self.__mutex = threading.Lock()

        # Create objects
        self.__bucket = b.Bucket(1)
        self.__bucket.make()
        self.__objFac = of.ObjFactory([self.__bucket])
        self.__objFac.makeAll(self.NUM_OPS*self.NUM_THREADS)
        
    def test_heavy_del(self):
        """Execute heavy load deletes with one bucket test"""

        operatorList = [] 

        # Launch DELETE threads
        print(f'Starting {self.NUM_THREADS} DELETE threads, {self.NUM_OPS} DELETEs per thread...')
        bucketList = [self.__bucket]
        for i in range(0, self.NUM_THREADS):
            operator = co.CmdOperator(f'thread-{i}', co.Type.DEL, self.NUM_OPS)
            operator.setBuckets(bucketList)
            operator.setObjFactory(self.__objFac)
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
