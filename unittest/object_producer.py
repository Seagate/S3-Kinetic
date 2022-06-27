import random
import threading

# local imports
import file_system
import s3bucket
import s3object

class ObjectProducer:
    """Class to produce s3objects

    Attributes:
        __numObjs : int, default 0
        __nxtObjIdx : int, default 0
        __mutex : mutex
        __buckets : list of s3buckets
    """

    def __init__(self, buckets):
        self.__numObjs = 0
        self.__nxtObjIdx = 0
        self.__mutex = threading.Lock()
        self.__buckets = buckets

    def makeAll(self, numObjs):
        """make s3objects with random different sizes"""
        print(f'Creating {numObjs} objects')

        count = numObjs
        while count > 0:
            for size in file_system.DATA_FILES.keys():
                count -= 1
                if count >= 0:
                    obj = s3object.S3Object(size)
                    bucket = random.choice(self.__buckets) # randomly pick a bucket
                    bucket.put(obj, newName=f'obj-{count}') # new s3object name is obj-#
                    if count % 10 == 0:
                        print(f'Number of remaining objects to create: {count}')
                else:
                    break
        self.__numObjs = numObjs
        self.__nxtObjIdx = 0
        print(f'{numObjs} objects created')

    def getObject(self):   
        """get an s3object

        Sequentially get an s3 object fron one bucket

        TODO: Currently, there is only one bucket in the bucket list.  Need to modify
        this method to work propertly with multiple buckets.
        """
 
        if self.__numObjs <= 0:
            return None

        try:
            self.__mutex.acquire()
            objIdx = self.__nxtObjIdx
            self.__nxtObjIdx = (self.__nxtObjIdx + 1) % self.__numObjs
        finally:
            self.__mutex.release()

        bucket = s3bucket.S3Bucket(random.choice(self.__buckets).fullName(), nameType='full')
        obj = s3object.S3Object()
        objName = f'obj-{objIdx}'
        obj.setName(objName)
        obj.setBucket(bucket)
        return obj
                
