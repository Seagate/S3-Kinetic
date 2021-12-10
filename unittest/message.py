class Message:
    DISK_USAGE = 'ERROR: incorrect disk usage'
    EMPTY = 'ERROR: %s empty'
    NOT_EMPTY = 'ERROR: %s not empty'
    FOUND = 'ERROR: %s found%s'
    NOT_FOUND = 'ERROR: %s not found%s'
    IN_SOURCE = 'ERROR: %s still in source %s'
    NOT_IN_DEST = 'ERROR: %s not in destination %s'
    SIZE_MISMATCH = 'ERROR: %s mismatched size'

    @classmethod
    def notFound(cls, arg1, arg2=None):
        if arg2 == None:
            arg2 = ""
        else:
            arg2 = f' in {arg2}'
        return cls.NOT_FOUND % (arg1, arg2)

    @classmethod
    def found(cls, arg1, arg2=None):
        if arg2 == None:
            arg2 = ""
        else:
            arg2 = f' in {arg2}'
        return cls.FOUND % (arg1, arg2) 

    @classmethod
    def empty(cls, arg):
        return cls.EMPTY % (arg)

    @classmethod
    def notEmpty(cls, arg):
        return cls.NOT_EMPTY % (arg)
    
    @classmethod
    def sizeMismatch(cls, arg):
        return cls.SIZE_MISMATCH % (arg)

    @classmethod
    def notInDest(cls, arg1, arg2):
        return cls.NOT_IN_DEST % (arg1, arg2)
    
    @classmethod
    def inSource(cls, arg1, arg2):
        return cls.IN_SOURCE % (arg1, arg2)
    
    @classmethod
    def wrongDiskUsage(cls):
        return cls.DISK_USAGE
