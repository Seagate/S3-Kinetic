import os
import random
import shutil

class Size:
    """Sizes of objects/files."""
    _1KB = 1024
    _1MB = 1024*_1KB
    _5MB = 5*_1MB
    _6MB = 6*_1MB
    _16MB = 16*_1MB
    _32MB = 32*_1MB

# Dictionary stores information to create input file with dd command: size, name , blockSize, blockCount
# Format: ((blockSize, numberOfBlocks), ....)
DATA_SIZE = (('1KB', 1), ('1MB', 1), ('1MB', 5), ('1MB', 6), ('1MB', 16), ('1MB', 32))
DATA_DIR = './test-data'
DOWNLOAD_DIR = 'test-download'
DATA_FILES = {}

def getRandFileSize():
    """Randomly get an input file size:  1KB, 1MB, 5MB, 6MB, 16MB, 32MB"""

    return list(DATA_FILES)[random.randint(0, len(DATA_FILES) - 1)]

def getFileName(size):
    """Get file name given file size"""

    fileTuple = DATA_FILES.get(size)
    if fileTuple == None:
        raise Exception(f'Invalid file size: {size}')
    fname = fileTuple[1]

    return fname

def _makeDir(aDir):
    if os.path.isdir(aDir):
        shutil.rmtree(aDir)

    os.mkdir(aDir)

def makeDownloadDir():
    """Create a clean download directory"""

    _makeDir(DOWNLOAD_DIR)

def makeDataDir():
    """Create input data directory"""

    _makeDir(DATA_DIR)

class FileProducer:
    """Class that creates input files"""
    
    DEV_IN_FILE = '/dev/urandom'

    def __init__(self):
        if len(DATA_FILES) > 0:
            # DATA_FILES dictionary was populated
            return

        for sizes in DATA_SIZE:
            [unit, blockSize, numBlocks] = [sizes[0][-2:], int(sizes[0][:-2]), sizes[1]]
            sizeName = blockSize * numBlocks
            if unit == 'KB':
                size = numBlocks * blockSize * (1 << 10)
            elif unit == 'MB':
                size = numBlocks * blockSize * (1 << 20)
            else:
                raise Exception('Invalid unit: {unit}')

            DATA_FILES[size] = (size, f'_{sizeName}{unit}.bin', f'{blockSize}{unit[0]}', numBlocks)

    def makeAll(self):
        """Make all input files"""

        # create a clean upload data directory
        _makeDir(DATA_DIR)

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
        fullFileName = f'{DATA_DIR}/{name}'
        os.system(f'dd if={self.DEV_IN_FILE} of={fullFileName} bs={blockSize} count={count} > /dev/null 2>&1')
