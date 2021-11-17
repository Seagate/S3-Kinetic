import getpass

BUCKET_PREFIX = '%s-s3cmd-unittest-' % getpass.getuser().lower()
PYTHON='python'
PATH_TO_S3CMD = '../s3cmd'
S3CMD = '%s/s3cmd'%PATH_TO_S3CMD
S3 = 's3://'
IN_FILE = '/dev/urandom'
TESTSUITE_OUT_DIR = 'testsuite-out'
ONE_MB_FN = '_1M.bin'

def makeBucketName(suffix):
    return '%s%s' % (BUCKET_PREFIX, suffix)
