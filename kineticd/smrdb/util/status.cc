// Copyright (c) 2011 The LevelDB Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file. See the AUTHORS file for names of contributors.

#include <execinfo.h>
#include <iostream>
#include <fstream>
#include <sstream>
#include <stdio.h>
#include <set>
#include <stdlib.h>
#include <dirent.h>

#include "port/port.h"
#include "leveldb/status.h"

namespace leveldb {

void Status::createErrorLog(bool appStart) {
    char buff[80];
    string utilDir = "/mnt/util/";
    string curFileName("kineticd.error.log");
    string errorFileName(utilDir + curFileName);
    time_t curtime;
    time(&curtime);

    ifstream f(errorFileName);
    if (f.good()) {
        f.close();
        // Scan for all kineticd.error.log*
        set<string> datedErrorLogs;
        DIR* d = opendir(utilDir.c_str());
        if (d) {
            string prevFilePrefix = curFileName + ".";
            struct dirent* entry;
            while ((entry = readdir(d)) != NULL) {
                if (strncmp(entry->d_name, prevFilePrefix.c_str(), prevFilePrefix.length()) == 0) {
                    datedErrorLogs.insert(entry->d_name);
                }
            }
        }
        // Keep only last 4 dated error logs
        int count = 0;
        for (set<string>::reverse_iterator it = datedErrorLogs.rbegin();
             it != datedErrorLogs.rend(); ++it) {
            if (++count > 4) {
                string fullName = utilDir + *it;
                remove(fullName.c_str());
            }
        }
        closedir(d);
        // Rename kineticd.error.log to kineticd.error.log.<current date time>
        string dateFileName(errorFileName);
        struct tm* timeinfo = localtime (&curtime);
        strftime(buff, 80, ".%F-%T", timeinfo);
        dateFileName += buff;
        rename(errorFileName.c_str(), dateFileName.c_str());
    } else {
        f.close();
    }

    ofstream os(errorFileName.c_str(), ios_base::out);
    if (appStart) {
        os << "===== START Kineticd on " << ctime_r(&curtime, buff) << endl;
        os.flush();
    }
    os.close();
}

char* Status::CopyState(const char* state) {
  uint32_t size;
  memcpy(&size, state, sizeof(size));
  char* result = new char[size + 5];
  memcpy(result, state, size + 5);
  return result;
}

Status::Status(Code code, const Slice& msg, const Slice& msg2) {
  assert(code != kOk);
  string traces;
  if (code != this->kNotFound) {
      void *buffer[10];
      int nptrs = backtrace(buffer, 10);
      char** stackTraceSymbols = backtrace_symbols(buffer, nptrs);
      if (stackTraceSymbols != NULL) {
          stringstream ss;
          for (int j = 0; j < nptrs; j++) {
              ss << "\n" << stackTraceSymbols[j];
          }
          ss << "\n";
          free(stackTraceSymbols);
          stackTraceSymbols = NULL;
          traces = ss.str();
      }
  }
  const uint32_t len1 = msg.size();
  const uint32_t len2 = msg2.size();
  const uint32_t size = len1 + (len2 ? (2 + len2) : 0) + traces.size();
  char* result = new char[size + 5];
  int resultDstOff = 0;
  memcpy(result, &size, sizeof(size));
  result[4] = static_cast<char>(code);
  memcpy(result + 5, msg.data(), len1);
  resultDstOff = 5 + len1;
  if (len2) {
    result[5 + len1] = ':';
    result[6 + len1] = ' ';
    memcpy(result + 7 + len1, msg2.data(), len2);
    resultDstOff = 7 + len1 + len2;
  }
  memcpy(result + resultDstOff, traces.data(), traces.size());
  state_ = result;

  if (code != this->kNotFound && code != this->kNotAttempted) {
      string fname("/mnt/util/kineticd.error.log");
      std::ifstream in(fname.c_str(), std::ifstream::ate | std::ifstream::binary);
      if (in.tellg() >= (1 << 18)) {  // Max file size is 1/4 MB
          in.close();
          createErrorLog(false);
      } else {
          in.close();
      }
      ofstream os(fname.c_str(), ios_base::out|ios_base::app);
      time_t curtime;
      time(&curtime);
      os << "=== " << ctime(&curtime);
      os << this->ToString();
      os.flush();
      os.close();
  }
}

std::string Status::ToString() const {
  if (state_ == NULL) {
    return "OK";
  } else {
    char tmp[30];
    const char* type;
    switch (code()) {
      case kOk:
        type = "OK";
        break;
      case kNotFound:
        type = "NotFound: ";
        break;
      case kCorruption:
        type = "Corruption: ";
        break;
      case kNotSupported:
        type = "Not implemented: ";
        break;
      case kInvalidArgument:
        type = "Invalid argument: ";
        break;
      case kIOError:
        type = "IO error: ";
        break;
      case kNoSpaceAvailable:
        type = "No space available: ";
        break;
      case kFrozen:
        type = "Drive Frozen: ";
        break;
      case kSuperblockIO:
        type = "Superblock IO error: ";
        break;
      default:
        snprintf(tmp, sizeof(tmp), "Unknown code(%d): ",
                 static_cast<int>(code()));
        type = tmp;
        break;
    }
    std::string result(type);
    uint32_t length;
    memcpy(&length, state_, sizeof(length));
    result.append(state_ + 5, length);
    return result;
  }
}

}  // namespace leveldb
