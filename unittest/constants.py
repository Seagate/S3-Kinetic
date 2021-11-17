import getpass

BUCKET_PREFIX = '%s-s3cmd-unittest-' % getpass.getuser().lower()
PYTHON='python'
PATH_TO_S3CMD = '../s3cmd'
S3CMD = '%s/s3cmd'%PATH_TO_S3CMD
S3 = 's3://'

def makeBucketName(suffix):
    return '%s%s' % (BUCKET_PREFIX, suffix)
