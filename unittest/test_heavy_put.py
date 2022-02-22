import sys
import unittest

import base_test as bt # require base_test to see PATH_TO_S3CMD
if bt.PATH_TO_S3CMD not in sys.path:
    sys.path.append(bt.PATH_TO_S3CMD) # required to see S3.ExitCodes

# local imports
import cmd_operator as co
import in_file_factory as ff 

class TestHeavyPut(bt.BaseTest):
    """Test S3-Kinetic with heavy puts"""

    NUM_THREADS = 5 #25 
    NUM_PUTS = 5 #50
    
    def tearDown(self):
        pass

    def test_heavy_put(self):
        """Execute heavy load test"""

        # Launch PUT threads
        putters = [] 
        print(f'Starting {self.NUM_THREADS} PUT threads, {self.NUM_PUTS} PUTS per thread...')
        for i in range(0, self.NUM_THREADS):
            putter = co.CmdOperator(f'thread-{i}', co.Type.PUT, self.NUM_PUTS)
            putters.append(putter)
            putter.start()

        # Wait for all PUT threads complete
        print(f'Waiting for completion.  Be patient...')
        for putter in putters:
            putter.join()
        print(f'All threads complete')

if __name__ == '__main__':
    unittest.main()
