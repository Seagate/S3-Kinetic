#!/usr/bin/env python
# -*- coding: utf-8 -*-

import os
import getpass
import locale

import S3.Config

import utils as u

gTests = ["fs", "s3cmd", "regression"] 

class GlobalVars:
    __tests = gTests
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

    def setTest(self, tests):
        if "all" in tests:
            self.__tests = gTests
        else:
            self.__tests = tests

    def tests(self):
        return self.__tests

    def setConfig(self):
        self.setConfigFile(None)
        if os.getenv("HOME"):
            self.__configFile = os.path.join(u.unicodise(os.getenv("HOME"), self.encoding()), \
                                    ".s3cfg")
        elif os.name == "nt" and os.getenv("USERPROFILE"):
            self.__configFile = os.path.join(
                                u.unicodise(os.getenv("USERPROFILE"), self.encoding()), \
                                os.getenv("APPDATA") and unicodise(os.getenv("APPDATA"), encoding) \
                                or 'Application Data', "s3cmd.ini")

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
        print("Encoding: ", self.encoding())

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
        ## Patterns for Unicode tests
        '''
        try:
            unicode
        except NameError:
            # python 3 support
            # In python 3, unicode -> str, and str -> bytes
            unicode = str
        '''
        self.__patterns = {}
        self.__patterns['UTF-8'] = u"ŪņЇЌœđЗ/☺ unicode € rocks ™"
        self.__patterns['GBK'] = u"12月31日/1-特色條目"

    def haveCurl(self):
        if self.__havecurl == None:
            if u.which('curl') is not None:
                self.__haveCurl = True
            else:
                self.__haveCurl = False
        return self.__haveCurl

    def loadConfig(self, cfgFile = None):
        if cfgFile == None:
            if os.getenv("HOME"):
                self.__configFile = os.path.join(u.unicodise(os.getenv("HOME"), \
                                               self.__encoding), ".s3cfg")
            elif os.name == "nt" and os.getenv("USERPROFILE"):
                self.__configFile = os.path.join(u.unicodise(os.getenv("USERPROFILE"), self.__encoding),
                        os.getenv("APPDATA") and \
                        u.unicodise(os.getenv("APPDATA"), self.__encoding) or \
                        'Application Data', "s3cmd.ini")
        else:
            self.__configFile = cfgFile

        self.config = S3.Config.Config(self.configFile())
        print("global: ", self.configFile())

    def encoding(self):
        return self.__encoding

    def setVerbose(self, verbose):
        self.__verbose = verbose

    def verbose(self):
        return self.__verbose

    def setBucketPrefix(self, prefix):
        self.__bucketPrefix = prefix        
         
    def bucketPrefix(self):
        return self.__bucketPrefix

    def setConfigFile(self, aFilename):
        self.__configFile = aFilename

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
