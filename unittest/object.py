import in_file_factory as ff

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
        fname = ff.getFileName(size)
        if suffix != "":
            suffix = f'_{suffix}'
        self.__name = fname + suffix 
        self.__size = size
        self.__fullFileName = f'{ff.DAT_DIR}/{self.__name}'
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
        return (self.__size > ff.Size._5MB)
