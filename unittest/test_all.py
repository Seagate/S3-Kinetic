import unittest

from test_bucket import TestBucket
from test_object import TestObject
from test_policy import TestPolicy

if __name__ == '__main__':
    suite = unittest.TestSuite()
    result = unittest.TestResult()
    suite.addTest(unittest.makeSuite(TestBucket))
    suite.addTest(unittest.makeSuite(TestObject))
    suite.addTest(unittest.makeSuite(TestPolicy))
    runner = unittest.TextTestRunner()
    runner.run(suite)
