import base_test as bt

_1KB_FN = "_1KB.bin"
_1MB_FN = "_1MB.bin"
_5MB_FN = "_5MB.bin"
_16MB_FN = "_16MB.bin"

class Size:
    """Sizes of objects/files."""
    _1KB = 1024
    _1MB = 1024*_1KB
    _5MB = 5*_1MB
    _16MB = 16*_1MB

class Object:
    """A class used to represent object in a bucket or file on disk."""

    def __init__(self, size, suffix=""):
        """Initialize this object.

        Parameters:
            size -- size of the object, choices: Size.<_1KB, _1MB, _5MB, _16M>
            suffix -- suffix of the object (default is blank)

        Raises:
            Exception
                If size is not valid.
        """
        self.__size = size
        if suffix != "":
            suffix = f'_{suffix}'
        if size == Size._1KB:
            self.__name = _1KB_FN + suffix 
        elif size == Size._1MB:
            self.__name = _1MB_FN + suffix
        elif size == Size._5MB:
            self.__name = _5MB_FN + suffix
        elif size == Size._16MB:
            self.__name = _16MB_FN + suffix
        else:
            raise Exception(f'Invalid file name {name}.' + \
                             ' Choices: Size._1KB, Size._1MB, Size._16MB')

        self.__fullFileName = f'{bt.TESTSUITE_DAT_DIR}/{self.__name}'
        self.__bucket = None

    def name(self):
        """Return name without bucket name of this object."""
        return self.__name

    def fullFileName(self):
        """Return full file path name of this object."""
        return self.__fullFileName

    def fullName(self):
        """Return full name, <bucketName>/<objectName>, of this object."""
        if self.bucket()== None:
            return None
        else:
            return f'{self.__bucket.fullName()}/{self.name()}'

    def bucket(self):
        """Return the bucket which contains this object if known."""
        return self.__bucket

    def setBucket(self, bucket):
        """Set the bucket which contains this object."""
        self.__bucket = bucket
        
    def mustBeInMultiPart(self):
        """Return boolean to indicate if this object must be chunked for put operation."""
        return (self.__size > Size._5MB)
