import unittest
import sys
import os
import getpass
from subprocess import Popen, PIPE, STDOUT
import subprocess
import shutil

from constants import *

sys.path.append(PATH_TO_S3CMD)

from S3.ExitCodes import *

unittest.TestLoader.sortTestMethodsUsing = None

class Test(unittest.TestCase):
    def setUp(self):
        # create testsuite output directory
        if not os.path.isdir(TESTSUITE_OUT_DIR):
            os.mkdir(TESTSUITE_OUT_DIR)
        
        # create a 1M file
        os.system('dd if=%s of=%s bs=1M count=1 > /dev/null 2>&1' % (IN_FILE, get_1MB_fpath()))

    def tearDown(self):
        if os.path.isdir(TESTSUITE_OUT_DIR):
            shutil.rmtree(TESTSUITE_OUT_DIR)

    def execute(self, args):
        args.insert(0, PYTHON)
        args.insert(1, S3CMD)
        completed_p = subprocess.run(args, stdout=PIPE, stderr=STDOUT, universal_newlines=True,
            close_fds=True)
        return completed_p

