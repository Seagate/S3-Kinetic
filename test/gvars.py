#!/usr/bin/env python
# -*- coding: utf-8 -*-

import os
import getpass
import locale

import S3.Config

import utils as u

class GlobalVars:
    __config = None
    __configFile = None 
    __outDir = "testsuite-out"
    __largeObjFilename = "urandom.bin"
    __smallObjFilename = "urandom.bin"
    __bucketPrefix = None
    __encoding = None
    __haveEncoding = False
    __verbose = False
    __haveCurl = False
    __patterns = None
    
    __testFailed = 0
    __testSucceeded = 0


    def __init__(self):
        self.setEncoding()  # This line must go before setConfig()
        self.setConfig()
        self.setPatterns()
        self.setHaveEncoding()

        self.setBucketPrefix(u"%s-" % getpass.getuser().lower())

    def __str__(self):
        nameWidth = 20
        s = ("%s:" % ("Encoding")).ljust(nameWidth, " ") +  self.encoding()
        s = s + ("\n%s:" % ("Config file")).ljust(nameWidth, " ") +  str(self.configFile())
        s = s + ("\n%s:" % ("Bucket prefix")).ljust(nameWidth, " ") +  str(self.bucketPrefix())
        s = s + ("\n%s:" % ("Verbose")).ljust(nameWidth, " ") +  str(self.verbose())

        return s

    def setConfig(self, fpath=None):
        if fpath == None:
            if os.getenv("HOME"):
                self.__configFile = os.path.join(u.unicodise(os.getenv("HOME"), self.encoding()), \
                                        ".s3cfg")
            elif os.name == "nt" and os.getenv("USERPROFILE"):
                self.__configFile = os.path.join(
                    u.unicodise(os.getenv("USERPROFILE"), self.encoding()), \
                    os.getenv("APPDATA") and unicodise(os.getenv("APPDATA"), encoding) \
                    or 'Application Data', "s3cmd.ini")
        else:
            self.__configFile = fpath
        self.__config = S3.Config.Config(self.configFile())

    def setEncoding(self):
        self.__encoding = locale.getpreferredencoding()
        if not self.encoding():
            print("Guessing current system encoding failed. Consider setting $LANG variable.")
            sys.exit(1)
        try:
            unicode
        except NameError:
            # python 3 support
            # In python 3, unicode -> str, and str -> bytes
            unicode = str

    def setHaveEncoding(self):
        self.__haveEncoding = os.path.isdir('testsuite/encodings/' + self.encoding())
        if not self.haveEncoding() and \
            os.path.isfile('testsuite/encodings/%s.tar.gz' % self.encoding()):
            os.system("tar xvz -C testsuite/encodings -f testsuite/encodings/%s.tar.gz" % self.encoding())
            self.__haveEncoding = os.path.isdir('testsuite/encodings/' + self.encoding())

        if not self.haveEncoding():
            print(self.encoding() + " specific files not found.")

    def haveEncoding(self):
        return self.__haveEncoding

    def encodedPattern(self):
        if self.haveEncoding():
            return self.__patterns[self.encoding()]
        else:
            return None
        
    def setPatterns(self):
        self.__patterns = {}
        self.__patterns['UTF-8'] = u"ŪņЇЌœđЗ/☺ unicode € rocks ™"

    def haveCurl(self):
        if self.__havecurl == None:
            if u.which('curl') is not None:
                self.__haveCurl = True
            else:
                self.__haveCurl = False
        return self.__haveCurl

    def encoding(self):
        return self.__encoding

    def setVerbose(self, verbose):
        self.__verbose = verbose

    def verbose(self):
        return self.__verbose

    def setBucketPrefix(self, prefix):
        if prefix != None:
            self.__bucketPrefix = prefix        
         
    def bucketPrefix(self):
        return self.__bucketPrefix

    def configFile(self):
        return self.__configFile

    def haveCurl(self):
        return self.__haveCurl

    def incTestFailed(self): 
        self.__testFailed += 1

    def incTestSucceeded(self): 
        self.__testSucceeded += 1

    def numTests(self):
        return self.testFailed() + self.testSucceeded()

    def testFailed(self):
        return self.__testFailed

    def testSucceeded(self):
        return self.__testSucceeded

    def outDir(self):
        return self.__outDir

    def setTestFailed(self, n):
        self.__testFailed = n

    def setTestSucceeded(self, n):
        self.__testSucceeded = n
