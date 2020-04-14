#include <errno.h>
#include <unistd.h>
#include <sys/syscall.h>
#include <sys/time.h>
#include <sys/resource.h>
#include <sched.h>

#include "glog/logging.h"

#include "kinetic/incoming_value.h"

#include "kinetic/reader_writer.h"
#include "kinetic/ssl_reader_writer.h"
#include <sys/time.h>
#include "value_factory.h"
#include <iostream>
#include "mem/DynamicMemory.h"
#include "smrdisk/Disk.h"

using namespace std; //NOLINT
using namespace kinetic; //NOLINT

namespace com {
namespace seagate {
namespace kinetic {
#define ROUNDUP(x, y)  ((((x) + (y) - 1) / (y)) * (y))

//char* ValueFactory::_buff = NULL;

ValueFactory::ValueFactory() {
/*
    if (ValueFactory::_buff == NULL) {
        posix_memalign((void**)&ValueFactory::_buff, 4096, 1024*1024);
    }
*/
    buff_ = NULL;
    posix_memalign((void**)&buff_, 4096, 4096);
}

IncomingValueInterface *ValueFactory::NewValue(int fd, size_t n) {
    char* buf = smr::DynamicMemory::getInstance()->allocate(n);  //Thai

    if (buf == NULL) { // && smr::Disk::_status == smr::Disk::DiskStatus::NO_SPACE) {
        // Consume value
        consume(fd, n);
        return new IncomingBuffValue(NULL, 0);
    }

#ifdef KDEBUG
    struct timeval tv;
    uint64_t start, end;
    gettimeofday(&tv, NULL);
    start = static_cast<uint64_t>(tv.tv_sec) * 1000000 + tv.tv_usec;
#endif

    int err = 0;
    ReaderWriter reader_writer(fd);
    if (!reader_writer.Read(buf, n, &err)) {
        free(buf);
        smr::DynamicMemory::getInstance()->deallocate(n);
       return NULL;
    }

#ifdef KDEBUG
    gettimeofday(&tv, NULL);
    end = static_cast<uint64_t>(tv.tv_sec) * 1000000 + tv.tv_usec;
    LOG(INFO) << "TIME TO READ VALUE FROM SOCKET BUFFER OF SIZE " << n << " " << (end - start) << endl;
#endif

    return new IncomingBuffValue(buf, n);
}

void ValueFactory::consume(int fd, size_t n) {
    ReaderWriter reader_writer(fd);
    size_t nConstBlockSize = 4096;
    int err = 0;
    bool bConsumed = true;
    while (n > 0 && bConsumed) {
        size_t nConsumeBlock = min(nConstBlockSize, n);
        bConsumed = reader_writer.Read(buff_, n, &err);
        n -= nConsumeBlock;
    }
}

void ValueFactory::consume(SSL *ssl, size_t n) {
    SslReaderWriter reader_writer(ssl);
    size_t nConstBlockSize = 4096;
    bool bConsumed = true;
    int err = 0;
    while (n > 0 && bConsumed) {
        size_t nConsumeBlock = min(nConstBlockSize, n);
        bConsumed = reader_writer.Read(buff_, nConsumeBlock, &err);
        n -= nConsumeBlock;
    }
}

IncomingValueInterface *ValueFactory::SslNewSmallValue(SSL *ssl, size_t n) {
    char* buf = smr::DynamicMemory::getInstance()->allocate(n);
    if (buf == NULL) { // && smr::Disk::_status == smr::Disk::DiskStatus::NO_SPACE) {
        // Consume value
        consume(ssl, n);
        return new IncomingBuffValue(NULL, 0);
    }
#ifdef KDEBUG
    struct timeval tv;
    uint64_t start, end;
    start = static_cast<uint64_t>(tv.tv_sec) * 1000000 + tv.tv_usec;
#endif

    SslReaderWriter reader_writer(ssl);
    int err = 0;
    if (!reader_writer.Read(buf, n, &err)) {
        free(buf);
        smr::DynamicMemory::getInstance()->deallocate(n);
        return NULL;
    }

#ifdef KDEBUG
    gettimeofday(&tv, NULL);
    end = static_cast<uint64_t>(tv.tv_sec) * 1000000 + tv.tv_usec;
    LOG(INFO) << "TIME TO READ VALUE FROM SOCKET BUFFER OF SIZE " << n << " " << (end - start) << endl;
#endif

    return new IncomingBuffValue(buf, n);
}

IncomingValueInterface *ValueFactory::SslNewValue(SSL *ssl, size_t n) {
    if (n <= 1024*1024) {
        return SslNewSmallValue(ssl, n);
    } else {
        return SslNewLargeValue(ssl, n);
    }
}

IncomingValueInterface *ValueFactory::SslNewLargeValue(SSL *ssl, size_t n) {
    LargeMemory* largeMem = new LargeMemory();
    if (!largeMem->allocate(n)) {
        delete largeMem;
        consume(ssl, n);
        return new IncomingBuffValue(NULL, 0);
    }
#ifdef KDEBUG
    struct timeval tv;
    uint64_t start, end;
    gettimeofday(&tv, NULL);
    start = static_cast<uint64_t>(tv.tv_sec) * 1000000 + tv.tv_usec;
#endif

    SslReaderWriter reader_writer(ssl);
    int size = 0;
    char* buf = largeMem->getStart(size);
    int err = 0;
    while (buf) {
        if (!reader_writer.Read(buf, size, &err)) {
            delete largeMem;
            return NULL;
        } else {
            buf = largeMem->getNext(size);
        }
    }
#ifdef KDEBUG
    gettimeofday(&tv, NULL);
    end = static_cast<uint64_t>(tv.tv_sec) * 1000000 + tv.tv_usec;
    LOG(INFO) << "TIME TO READ VALUE FROM SOCKET BUFFER OF SIZE " << n << " " << (end - start) << endl;
#endif

    return new IncomingBuffValue(largeMem);
}

} // namespace kinetic
} // namespace seagate
} // namespace com
