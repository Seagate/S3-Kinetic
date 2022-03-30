from enum import Enum
import random
import threading

#Local imports
import file_system
import s3bucket
import s3object

class Type(Enum):
    """Enumeration of operation types:  PUT, GET, DEL"""

    PUT = 0
    GET = 1
    DEL = 2

    @classmethod
    def isValid(cls, aType):
        """Validate operation type (PUT, GET, DEL)"""

        return (aType == cls.PUT or aType == cls.GET or aType == cls.DEL)


class CmdOperator(threading.Thread):
    """A thread to repetitively execute an S3cmd

    Attributes:
    __type : Type
        Type of operation, ex. PUT, GET, DEL, ...
    __n    : int
        Number of operations to perform.  Default to 1
    __buckets : list, default = []
        A list of made buckets 
    __error : AssertionError
        Assertion error used to decide to stop command operator (thread)

    """

    stopAllOperators = False

    def __init__(self, name, aType, n = 1):
        threading.Thread.__init__(self)
        self.setName(name)
        if not Type.isValid(aType): 
            raise("Invalid function type: %d" % (aType))
        self.__type = aType 
        self.__n = n
        self.__buckets = []
        self.__error = None
        self.__objectProducer = None

    def setBuckets(self, buckets):
        self.__buckets = buckets

    def __put(self):
        """Put file with random size"""

        fileSize = file_system.getRandFileSize()
        bucket = s3bucket.S3Bucket(self.getName())
        obj = s3object.S3Object(fileSize)
        bucket.put(obj)

    def __get(self):
        if len(self.__buckets) == 0:
            return
        # Get random bucket
        bucket = self.__buckets[random.randint(0, len(self.__buckets) - 1)]
        size = file_system.getRandFileSize()
        obj = s3object.S3Object(size)
        obj.setBucket(bucket)
        result = obj.get()
        return result

    def setObjectProducer(self, objectProducer):
        self.__objectProducer = objectProducer

    def __del(self):
        if len(self.__buckets) == 0:
            return
        # Get random bucket
        obj = self.__objectProducer.getObject()
        result = None
        if obj:
            result = obj.delete()
        return result

    def getError(self):
        return self.__error

    def run(self):
        # Select operation to execute
        if self.__type == Type.PUT:
            exeOp = self.__put
        elif self.__type == Type.GET:
            exeOp = self.__get
        elif self.__type == Type.DEL:
            exeOp = self.__del

        # Execute n operations
        while (not CmdOperator.stopAllOperators) and (self.__n == -1 or self.__n > 0):
            try:
                exeOp()
                if self.__n > 0:
                    self.__n -= 1
            except AssertionError as err:
                self.__error = err
                CmdOperator.stopAllOperators = True

