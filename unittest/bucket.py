import getpass
import sys

# local imports
import base_test as bt # required to see PATH_TO_S3CMD used in the next instruction

if bt.PATH_TO_S3CMD not in sys.path:
    sys.path.append(bt.PATH_TO_S3CMD) # required to see S3.ExitCodes

import S3.ExitCodes as xcodes

class Bucket:
    """A class used to represent bucket."""

    def __init__(self, name, nameType='suffix'):
        """Initialize this bucket.

        Parameters:
            name     -- bucket name suffix or fullname
            nameType --- 'suffix' or 'full' bucket name (prefix with s3://) (default is 'suffix')
        """
        if nameType == 'suffix':
            self.__suffix = name 
            self.__name = f"{bt.BUCKET_PREFIX}{self.__suffix}"
            self.__fullName = f'{bt.S3}{self.__name}'
        elif nameType == 'full':
            self.__fullName = name
            self.__name = name.replace(bt.S3, '')
            self.__suffix = name.replace(bt.BUCKET_PREFIX, '') 
        else:
            raise Exception(f'Invalid name type: {nameType}.  Choices: "suffix" and "full"')

    def isEmpty(self):
        args = ['la', self.fullName()]
        result = bt.executeS3cmd(args)
        return (result.returncode == xcodes.EX_OK and result.stdout.find(self.fullName()) == -1)

    def isContain(self, aName):
        """Return True if this bucket contain the specified object, False otherwise."""
        objFullName = f'{self.fullName()}/{aName}'
        args = ['la', objFullName]
        result = bt.executeS3cmd(args)
        return (result.returncode == xcodes.EX_OK and result.stdout.find(objFullName) != -1)

    def isExist(self):
        args = ['ls']
        result = bt.executeS3cmd(args)
        return (result.returncode == xcodes.EX_OK and result.stdout.find(self.fullName()) != -1)

    def name(self):
        return self.__name

    def fullName(self):
        return self.__fullName

    def make(self, refresh=True):
        if not self.isExist():
            args = ['mb', self.fullName()]
            bt.executeS3cmd(args)
        elif refresh:
            args = ['del', '--recursive', '--force', self.fullName()]
            bt.executeS3cmd(args)
        return self.isExist()            

    def put(self, obj, newName=None):

        if newName == None:
            args = ['put', obj.fullFileName(), self.fullName()]
        else:
            args = ['put', obj.fullFileName(), f'{self.fullName()}/{newName}']

        if obj.mustBeInMultiPart():
            args.insert(1, 'multipart_chunk_size_mb=5')

        result =bt.executeS3cmd(args)
        obj.setBucket(self)

    def remove(self):
        args = ['rb', '--recursive', self.fullName()]
        bt.executeS3cmd(args)


