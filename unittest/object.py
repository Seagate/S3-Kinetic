import base_test as bt

_1KB_FN = "_1KB.bin"
_1MB_FN = "_1MB.bin"
_16MB_FN = "_16MB.bin"

class Size:
    _1KB = 1024
    _1MB = 1024*_1KB
    _5MB = 5*_1MB
    _16MB = 16*_1MB

class Object:
    def __init__(self, size, suffix=""):
        self.__size = size
        if suffix != "":
            suffix = f'_{suffix}'
        if size == Size._1KB:
            self.__name = _1KB_FN + suffix 
        elif size == Size._1MB:
            self.__name = _1MB_FN + suffix
        elif size == Size._16MB:
            self.__name = _16MB_FN + suffix
        else:
            raise Exception(f'Invalid file name {name}.' + \
                             ' Choices: Size._1KB, Size._1MB, Size._16MB')

        self.__fullFileName = f'{bt.TESTSUITE_DAT_DIR}/{self.__name}'
        self.__bucket = None

    def name(self):
        return self.__name

    def fullFileName(self):
        return self.__fullFileName

    def fullName(self):
        if self.bucket()== None:
            return None
        else:
            return f'{self.__bucket.fullName()}/{self.name()}'

    def bucket(self):
        return self.__bucket

    def setBucket(self, bucket):
        self.__bucket = bucket
        
    def mustBeInMultiPart(self):
        return (self.__size > Size._5MB)
