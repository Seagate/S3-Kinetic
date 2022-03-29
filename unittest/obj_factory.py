import random
import threading

import bucket as b
import in_file_factory as ff
import object as o

class ObjFactory:
    def __init__(self, buckets):
        self.__numObjs = 0
        self.__nxtObjIdx = 0
        self.__mutex = threading.Lock()
        self.__buckets = buckets

    def makeAll(self, numObjs):
        # upload files with sequential names into one bucket.  Filenames use zero index.
        count = numObjs
        while count > 0:
            for size in ff.DATA_FILES.keys():
                count -= 1
                if count >= 0:
                    obj = o.Object(size)
                    bucket = random.choice(self.__buckets)
                    bucket.put(obj, newName=f'obj-{count}')
                else:
                    break
        self.__numObjs = numObjs
        self.__nxtObjIdx = 0

    def getObject(self):   
        if self.__numObjs <= 0:
            return None

        try:
            self.__mutex.acquire()
            objIdx = self.__nxtObjIdx
            self.__nxtObjIdx = (self.__nxtObjIdx + 1) % self.__numObjs
        finally:
            self.__mutex.release()

        bucket = b.Bucket(random.choice(self.__buckets).fullName(), nameType='full')
        obj = o.Object()
        objName = f'obj-{objIdx}'
        obj.setName(objName)
        obj.setBucket(bucket)
        return obj
                
