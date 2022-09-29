import sys
import os

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
        return result

    def get(self, newName=None):
        """Get/download this object from dbase """

        if newName == None:
            args = ['get', '--force', self.fullName(), file_system.DOWNLOAD_DIR]
        else:
            args = ['get', '--force', self.fullName(), os.path.join(file_system.DOWNLOAD_DIR, newName)]
        result = bt.executeS3cmd(args)
        assert result.returncode == xcodes.EX_OK, result.stdout
        return result

    # Performs a GET on an object following a PUT, compares it bytewise to its original source
    # Renamed objects must have their __name member updated for this function to work
    def verifyPut(self):
        args = ['get', '--force', self.fullName()]
        result = bt.executeS3cmd(args)
        assert result.returncode == xcodes.EX_OK, result.stdout

        downloadPath = f'{os.getcwd()}/{self.name()}'
        args = ['cmp', '-b', self.fullFileName(), downloadPath]
        comp = bt.executeLinuxCmd(args)

        # clean up by deleting downloaded object
        if(os.path.exists(self.name())):
            os.remove(self.name())

        # return code 0 means files are equivalent; return true/false accordingly
        if(comp.returncode == 0):
            return True
        else:
            return False

    # Compares the resulting object from a GET to its original source (bytewise)
    # Takes in a "downloadPath" argument to determine where the newly downloaded object resides
    def verifyGet(self, downloadPath):
        args = ['cmp', '-b', self.fullFileName(), downloadPath]
        comp = bt.executeLinuxCmd(args)

        # return code 0 means files are equivalent; return true/false accordingly
        if(comp.returncode == 0):
            return True
        else:
            return False

    # Compares a copied object to its source object
    def verifyCopy(self, srcBucket, destBucket):
        srcObject = f'{srcBucket.fullName()}/{self.name()}'
        destObject = f'{destBucket.fullName()}/{self.name()}'

        # get objects; rename source object to "source", dest object to "dest"
        srcArgs = ['get', '--force', srcObject, "source"]
        destArgs = ['get', '--force', destObject, "dest"]
        srcResult = bt.executeS3cmd(srcArgs)
        destResult = bt.executeS3cmd(destArgs)
        assert srcResult.returncode == xcodes.EX_OK, srcResult.stdout
        assert destResult.returncode == xcodes.EX_OK, destResult.stdout

        srcPath = f'{os.getcwd()}/source'
        destPath = f'{os.getcwd()}/dest'

        compArgs = ['cmp', '-b', srcPath, destPath]
        comp = bt.executeLinuxCmd(compArgs)

        # clean up downloaded objects
        if(os.path.exists(srcPath)):
            os.remove(srcPath)
        if(os.path.exists(destPath)):
            os.remove(destPath)

        # return code 0 means files are equivalent; return true/false accordingly
        if(comp.returncode == 0):
            return True
        else:
            return False

