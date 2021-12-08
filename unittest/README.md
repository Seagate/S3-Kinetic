# MinIO-Kinetic Unit Test

## Requirements
    python3 (for this Minio-Kinetic Unit Test)
    python  (s3cmd does not work with python3)

## Setup
    Install memory module kernel object.
        See kineticd README.md, section Loading MemMgr Module, for instruction.
    Start minio-kinetic

## Execution 
All of the followings must be done in the unittest directory:

- To execute all tests in a file:

        python3 <file> -v

        Example:
            python3 test_bucket.py -v

- To execute a test in a file:

        python3 <file> <class>.<test> -v

        Example:
            python3 test_bucket.py TestBucket.test_make -v

- To display help screen:

        python3 <file> -h

        Example:<br>
            python3 test_bucket.py -h>
