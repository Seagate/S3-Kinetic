import sys

import base_test as bt

if bt.PATH_TO_S3CMD not in sys.path:
    sys.path.append(bt.PATH_TO_S3CMD) # required to see S3.ExitCodes

import file_system
import S3.ExitCodes as xcodes

class S3Object:
    """A class used to represent object in a bucket or file on disk."""

    def __init__(self, size=None):
        """Initialize this object.

        Parameters:
            size -- size of the object, choices: Size.<_1KB, _1MB, _5MB, _16M>
            suffix -- suffix of the object (default is blank)

        Raises:
            Exception
                If size is not valid.
        """

        if size == None:
            self.__name = None
            self.__fullFileName = None
            self.__size = None
            self.__bucket = None
        else:
            fname = file_system.getFileName(size)
            self.__name = fname
            self.__size = size
            self.__fullFileName = f'{file_system.DATA_DIR}/{self.__name}'
            self.__bucket = None

    def name(self):
        """Return name without bucket name of this object."""
        return self.__name

    def setName(self, name):
        self.__name = name

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
        return (self.__size > file_system.Size._5MB)

    def delete(self):
        """Delete this object from dbase"""

        args = ['del', self.fullName()]
        result = bt.executeS3cmd(args)
        assert result.returncode == xcodes.EX_OK, result.stdout

    def get(self, newName=None):
        """Get/download this object from dbase """

        if newName == None:
            args = ['get', '--force', self.fullName(), file_system.DOWNLOAD_DIR]
        else:
            args = ['get', '--force', self.fullName(), os.path.join(file_system.DOWNLOAD_DIR, newName)]
        result = bt.executeS3cmd(args)
        assert result.returncode == xcodes.EX_OK, result.stdout
        return (self.__size > file_system.Size._5MB)
