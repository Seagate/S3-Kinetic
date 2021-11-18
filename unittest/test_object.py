import base_test as bt

class TestObject(bt.BaseTest):
    def setUp(self):
        super().setUp()
        # Do setup for test object 
        # ...

    def tearDown(self):
        # Do teardown for test object 
        # ...
        super().tearDown()

    def test_put(self):
        pass

    def test_put_multipart(self):
        pass

