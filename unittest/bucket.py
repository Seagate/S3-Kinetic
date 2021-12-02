import getpass
import sys

# import locals
import base_test as bt

sys.path.append(bt.PATH_TO_S3CMD) # required to see S3.ExitCodes
import S3.ExitCodes as xcodes

BUCKET_PREFIX = f'{getpass.getuser().lower()}-s3cmd-unittest-'
S3 = 's3://'

class Bucket:
    def __init__(self, name, nameType='suffix'):
        if nameType == 'suffix':
            self.__suffix = name 
            self.__name = f"{BUCKET_PREFIX}{self.__suffix}"
            self.__fullName = f'{S3}{self.__name}'
        elif nameType == 'full':
            self.__fullName = name
            self.__name = name.replace(S3, '')
            self.__suffix = name.replace(BUCKET_PREFIX, '') 
        else:
            raise Exception(f'Invalid name type: {nameType}.  Choices: "suffix" and "full"')

    def isExist(self):
        args = ['ls']
        result = bt.executeS3cmd(args)
        return (result.returncode == xcodes.EX_OK and result.stdout.find(self.fullName()) != -1)

    def isEmpty(self):
        args = ['la', self.fullName()]
        result = bt.executeS3cmd(args)
        return (result.returncode == xcodes.EX_OK and result.stdout.find(self.fullName()) == -1)

    def isContain(self, obj):
        args = ['la', obj]
        result = bt.executeS3cmd(args)
        return (result.returncode == xcodes.EX_OK and result.stdout.find(obj) != -1)

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
            args = ['put', obj, self.fullName()]
        else:
            args = ['put', obj, f'{self.fullName()}/{newName}']
        bt.executeS3cmd(args)

    def remove(self):
        args = ['rb', '--recursive', self.fullName()]
        bt.executeS3cmd(args)


