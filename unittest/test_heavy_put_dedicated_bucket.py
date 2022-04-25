import sys
import unittest

# local imports
import base_test
import cmd_operator
import s3bucket

class TestHeavyPutDedicatedBucket(base_test.BaseTest):
    """Test S3-Kinetic with heavy puts"""

    NUM_THREADS = 90
    NUM_PUTS = 50
    
    def test_heavy_put(self):
        """Execute heavy load test"""

        # Launch PUT threads
        putters = [] 
        print(f'Starting {self.NUM_THREADS} PUT threads, {self.NUM_PUTS} PUTS per thread...')
        for i in range(0, self.NUM_THREADS):
            putter = cmd_operator.CmdOperator(f'thread-{i}', cmd_operator.Type.PUT, self.NUM_PUTS)
            bucket = s3bucket.S3Bucket(putter.getName())
            bucket.make()
            putters.append(putter)
            putter.start()

        # Wait for all PUT threads complete
        print(f'Waiting for completion.  Be patient...')
        for putter in putters:
            putter.join()
        print(f'All threads complete')

if __name__ == '__main__':
    unittest.main()
