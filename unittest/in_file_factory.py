import os
import random

class Size:
    """Sizes of objects/files."""
    _1KB = 1024
    _1MB = 1024*_1KB
    _5MB = 5*_1MB
    _6MB = 6*_1MB
    _16MB = 16*_1MB
    _32MB = 32*_1MB

# Dictionary stores information to create input file with dd command: size, name , blockSize, blockCount
DATA_FILES = {Size._1KB:(Size._1KB, '_1KB.bin', '1K', 1),
              Size._1MB:(Size._1MB, '_1MB.bin', '1M', 1),
              Size._5MB:(Size._5MB, '_5MB.bin', '1M', 5),
              Size._6MB:(Size._6MB, '_6MB.bin', '1M', 6),
              Size._16MB:(Size._16MB, '_16MB.bin', '1M', 16),
              Size._32MB:(Size._32MB, '_32MB.bin', '1M', 32)}

def getRandFileSize():
    """Randomly get an input file size:  1KB, 1MB, 5MB, 6MB, 16MB, 32MB"""

    return list(DATA_FILES)[random.randint(0, len(DATA_FILES) - 1)]

def getRandFileName():
    """Randomly get an input file name:  _1KB.bin, _1MB.bin, _5MB,..."""
    
    return list(DATA_FILES.values())[random.randint(0, len(DATA_FILES) - 1)][1]

def getFileName(size):
    """Get file name given file size"""

    fileTuple = DATA_FILES.get(size)
    if fileTuple == None:
        raise(f'Invalid file size: {size}')
    fname = fileTuple[1]
    return fname

class InFileFactory:
    """Class that creates input files"""
    
    DAT_DIR = './test-dat'
    DEV_IN_FILE = '/dev/urandom'

    def makeAll(self):
        """Make all input files"""

        for fileTuple in DATA_FILES.values():
           self.__make(fileTuple) 

    def __make(self, dataFileTuple):
        """Make an input file
        Arguments
        --------
        dataFileTuple : tuple, format = (size, fname, blockSize, blockCount)
        """

        name  = dataFileTuple[1]
        blockSize = dataFileTuple[2]
        count = dataFileTuple[3]
        fullFileName = f'{self.DAT_DIR}/{name}'
        os.system(f'dd if={self.DEV_IN_FILE} of={fullFileName} bs={blockSize} count={count} > /dev/null 2>&1')
