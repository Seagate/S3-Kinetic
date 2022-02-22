from enum import Enum
import random
import threading

#Local imports
import bucket as b
import in_file_factory as ff
import object as o

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
    """A thread to repeatatively execute an S3cmd

    Attributes:
    __name : string
        Name of this command operator
    __type : Type
        Type of operation, ex. PUT, GET, DEL, ...
    __n    : int
        Number of operations to perform.  Default to 1

    """

    def __init__(self, name, aType, n = 1):
        threading.Thread.__init__(self)
        self.__name = name
        if not Type.isValid(aType): 
            raise("Invalid function type: %d" % (aType))
        self.__type = aType 
        self.__n = n
    
    def __put(self):
        """Put file with random size"""

        fileSize = ff.getRandFileSize()
        bucket = b.Bucket(self.__name)
        obj = o.Object(fileSize)
        bucket.put(obj)

    def __get(self):
        pass 

    def __del(self):
        pass 

    def run(self):
        # Select operation to execute
        if self.__type == Type.PUT:
            exeOp = self.__put
        elif self.__type == Type.GET:
            exeOp = self.__get
        elif self.__type == Type.DEL:
            exeOp = self.__del

        bucket = b.Bucket(self.__name)
        bucket.make(refresh=False)

        # Execute n operations
        while self.__n == -1 or self.__n > 0:
            exeOp()
            if self.__n > 0:
                self.__n -= 1
            

