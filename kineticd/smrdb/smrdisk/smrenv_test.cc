// Copyright (c) 2011 The LevelDB Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file. See the AUTHORS file for names of contributors.

#include "SmrEnv.h"

#include "db/db_impl.h"
#include "leveldb/db.h"
#include "leveldb/env.h"
#include "util/testharness.h"
#include <string>
#include <vector>
#include "leveldb/env.h"

namespace leveldb {

std::string dir = "/dev/sdb";
int preused_zones = 9;

bool execute_command(const string command_line) {
  FILE* command_stream = popen(command_line.c_str(), "r");

  char* line_buffer = NULL;
  size_t line_buffer_size;
  int bytes_read;
  while ((bytes_read = getline(&line_buffer, &line_buffer_size, command_stream)) != -1) {
      if (bytes_read > 0) {
          std::string line(line_buffer, bytes_read);
          cout << command_line << ": " << line;
      }
  }

  if (line_buffer) {
      free(line_buffer);
  }

  int return_code = pclose(command_stream);
  if (WIFEXITED(return_code)) {
      return_code = WEXITSTATUS(return_code);
  } else {
      return_code = -1;
  }

  if (return_code) {
    cout << "Command <" << command_line << "> got non-zero return code " << return_code;
    return false;
  }

  return true;
}

bool clear_super_block_addresses() {
  std::stringstream command;
  uint64_t seek_to;
  seek_to = smr::Disk::SUPERBLOCK_0_ADDR/1048576;
  command << "dd if=/dev/zero of=" << dir << " bs=1048576 count=10 seek=" << seek_to  << " 2>&1";

  if (!execute_command(command.str())) {
    return false;
  }

  command.str("");
  seek_to = smr::Disk::SUPERBLOCK_1_ADDR/1048576;
  command << "dd if=/dev/zero of=" << dir << " bs=1048576 count=10 seek=" << seek_to  << " 2>&1";

  if (!execute_command(command.str())) {
    return false;
  }

  command.str("");
  seek_to = smr::Disk::SUPERBLOCK_2_ADDR/1048576;
  command << "dd if=/dev/zero of=" << dir << " bs=1048576 count=10 seek=" << seek_to  << " 2>&1";

  if (!execute_command(command.str())) {
    return false;
  }
  return true;
}

class SmrEnvTest {
 public:
  Env* env_;

  SmrEnvTest()
      : env_(NULL) {
     env_ = DriveEnv::getInstance();
     smr::Disk::initializeSuperBlockAddr(dir);
  }

  ~SmrEnvTest() {
    env_->clearDisk();
    clear_super_block_addresses();
  }
};

TEST(SmrEnvTest, DefragmentZonesAllErased) {
  int zones_to_write = 3;
  const size_t kWriteSize = 299 * 1024;
  char* scratch = new char[kWriteSize * 2];
  std::string write_data;

  for (size_t i = 0; i < kWriteSize; ++i) {
    write_data.append(1, static_cast<char>(i));
  }

  uint64_t fsize = kWriteSize*10;
  int numFilesPerZone = (256*1024*1024)/fsize; //kWriteSize/4; //TRUNCATE(kWriteSize + 4096, 4096);

  ASSERT_OK(env_->CreateDir(dir, true));
  ASSERT_EQ(env_->GetZonesUsed(), preused_zones);
  stringstream ss;

  WritableFile* writable_file;
  char buf[10];
  string fileNumPrefix("000000");

  cout << "CREATING... " << endl;
  for (int i = 4; i < zones_to_write*numFilesPerZone; ++i) {
      ss << fileNumPrefix << i;
      string fname = ss.str();
      fname = fname.substr(fname.size() - 6);
      fname += ".sst";
      ASSERT_OK(env_->NewWritableFile( dir + "/" + fname, &writable_file));
      for (int j = 0; j < 10; ++j) {
          writable_file->Append(write_data);
      }
      delete writable_file;
      ss.str("");
  }

  ASSERT_EQ(env_->GetZonesUsed(), preused_zones+zones_to_write);
  cout << "Running Defragment... " << endl;
  env_->Defragment(0);
  ASSERT_EQ(env_->GetZonesUsed(), preused_zones+zones_to_write);

  cout << "DELETING... " << endl;

  for (int i = 4; i < zones_to_write*numFilesPerZone; ++i) {
      ss << fileNumPrefix << i;
      string fname = ss.str();
      fname = fname.substr(fname.size() - 6);
      fname += ".sst";
      env_->DeleteFile(dir + "/" + fname);
      ss.str("");
  }

  env_->Defragment(0);
  ASSERT_EQ(env_->GetZonesUsed(), preused_zones);

  ASSERT_OK(env_->DeleteDir(dir));

}

TEST(SmrEnvTest, DefragmentZonesPartiallyErased) {
  int zones_to_write = 3;
  const size_t kWriteSize = 299 * 1024;
  std::string write_data;

  for (size_t i = 0; i < kWriteSize; ++i) {
    write_data.append(1, static_cast<char>(i));
  }

  uint64_t fsize = kWriteSize*10;
  int numFilesPerZone = (256*1024*1024)/fsize; //kWriteSize/4; //TRUNCATE(kWriteSize + 4096, 4096);

  ASSERT_OK(env_->CreateDir(dir, true));
  ASSERT_EQ(env_->GetZonesUsed(), preused_zones);
  stringstream ss;

  WritableFile* writable_file;
  string fileNumPrefix("000000");

  cout << "CREATING... " << endl;
  for (int i = 4; i < zones_to_write*numFilesPerZone; ++i) {
      ss << fileNumPrefix << i;
      string fname = ss.str();
      fname = fname.substr(fname.size() - 6);
      fname += ".sst";
      ASSERT_OK(env_->NewWritableFile( dir + "/" + fname, &writable_file));
      for (int j = 0; j < 10; ++j) {
          writable_file->Append(write_data);
      }
      delete writable_file;
      ss.str("");
  }

  ASSERT_EQ(env_->GetZonesUsed(), preused_zones+zones_to_write);
  cout << "Running Defragment... " << endl;
  env_->Defragment(0);
  ASSERT_EQ(env_->GetZonesUsed(), preused_zones+zones_to_write);

  cout << "DELETING... " << endl;

  // delete first half of files on first zone
  for (int i = 4; i < numFilesPerZone/2; ++i) {
      ss << fileNumPrefix << i;
      string fname = ss.str();
      fname = fname.substr(fname.size() - 6);
      fname += ".sst";
      env_->DeleteFile(dir + "/" + fname);
      ss.str("");
  }

  // delete first quarter of files on second zone
  for (int i = numFilesPerZone; i < numFilesPerZone + (2*numFilesPerZone)/4; ++i) {
      ss << fileNumPrefix << i;
      string fname = ss.str();
      fname = fname.substr(fname.size() - 6);
      fname += ".sst";
      env_->DeleteFile(dir + "/" + fname);
      ss.str("");
  }

  // delete first quarter of files on third zone
  for (int i = numFilesPerZone*2; i < numFilesPerZone + (3*numFilesPerZone)/4; ++i) {
      ss << fileNumPrefix << i;
      string fname = ss.str();
      fname = fname.substr(fname.size() - 6);
      fname += ".sst";
      env_->DeleteFile(dir + "/" + fname);
      ss.str("");
  }

  for (int i=0; i<zones_to_write; i++) {
    env_->Defragment(0);
  }

  ASSERT_EQ(env_->GetZonesUsed(), (preused_zones+zones_to_write)-1);

  ASSERT_OK(env_->DeleteDir(dir));

}

TEST(SmrEnvTest, DefragmentZonesErasedAlternate) {
  int zones_to_write = 2;
  const size_t kWriteSize = 299 * 1024;
  std::string write_data;

  for (size_t i = 0; i < kWriteSize; ++i) {
    write_data.append(1, static_cast<char>(i));
  }

  uint64_t fsize = kWriteSize*10;
  int numFilesPerZone = (256*1024*1024)/fsize; //kWriteSize/4; //TRUNCATE(kWriteSize + 4096, 4096);

  ASSERT_OK(env_->CreateDir(dir, true));
  ASSERT_EQ(env_->GetZonesUsed(), preused_zones);
  stringstream ss;

  WritableFile* writable_file;
  string fileNumPrefix("000000");

  cout << "CREATING... " << endl;
  for (int i = 4; i < zones_to_write*numFilesPerZone; ++i) {
      ss << fileNumPrefix << i;
      string fname = ss.str();
      fname = fname.substr(fname.size() - 6);
      fname += ".sst";
      ASSERT_OK(env_->NewWritableFile( dir + "/" + fname, &writable_file));
      for (int j = 0; j < 10; ++j) {
          writable_file->Append(write_data);
      }
      delete writable_file;
      ss.str("");
  }

  ASSERT_EQ(env_->GetZonesUsed(), preused_zones+zones_to_write);
  cout << "Running Defragment... " << endl;
  env_->Defragment(0);
  ASSERT_EQ(env_->GetZonesUsed(), preused_zones+zones_to_write);

  cout << "DELETING... " << endl;

  // delete half of the files on first zone, alternating files
  for (int i = 4; i < numFilesPerZone; ++i) {
      if (i % 2 == 0) {
        ss << fileNumPrefix << i;
        string fname = ss.str();
        fname = fname.substr(fname.size() - 6);
        fname += ".sst";
        env_->DeleteFile(dir + "/" + fname);
        ss.str("");
      }
  }

  // delete half of the files on second zone, alternating files
  for (int i = numFilesPerZone; i < 2*numFilesPerZone; ++i) {
      if ( i % 2 != 0) {
        ss << fileNumPrefix << i;
        string fname = ss.str();
        fname = fname.substr(fname.size() - 6);
        fname += ".sst";
        env_->DeleteFile(dir + "/" + fname);
        ss.str("");
      }
  }

  env_->Defragment(0);
  ASSERT_EQ(env_->GetZonesUsed(), (preused_zones+zones_to_write) - 1);

  ASSERT_OK(env_->DeleteDir(dir));

}

TEST(SmrEnvTest, DefragmentZonesHalfErasedAlternate) {
  int zones_to_write = 20;
  const size_t kWriteSize = 299 * 1024;
  std::string write_data;

  for (size_t i = 0; i < kWriteSize; ++i) {
    write_data.append(1, static_cast<char>(i));
  }

  uint64_t fsize = kWriteSize*10;
  int numFilesPerZone = (256*1024*1024)/fsize; //kWriteSize/4; //TRUNCATE(kWriteSize + 4096, 4096);

  ASSERT_OK(env_->CreateDir(dir, true));
  ASSERT_EQ(env_->GetZonesUsed(), preused_zones);
  stringstream ss;

  WritableFile* writable_file;
  string fileNumPrefix("000000");

  cout << "CREATING... " << endl;
  for (int i = 4; i < zones_to_write*numFilesPerZone; ++i) {
      ss << fileNumPrefix << i;
      string fname = ss.str();
      fname = fname.substr(fname.size() - 6);
      fname += ".sst";
      ASSERT_OK(env_->NewWritableFile( dir + "/" + fname, &writable_file));
      for (int j = 0; j < 10; ++j) {
          writable_file->Append(write_data);
      }
      delete writable_file;
      ss.str("");
  }

  ASSERT_EQ(env_->GetZonesUsed(), preused_zones+zones_to_write);
  cout << "Running Defragment... " << endl;
  env_->Defragment(0);
  ASSERT_EQ(env_->GetZonesUsed(), preused_zones+zones_to_write);

  cout << "DELETING... " << endl;

  // delete third of the files, alternating files
  for (int i = 4; i < zones_to_write*numFilesPerZone; ++i) {
      if (i % 2 == 0) {
        ss << fileNumPrefix << i;
        string fname = ss.str();
        fname = fname.substr(fname.size() - 6);
        fname += ".sst";
        env_->DeleteFile(dir + "/" + fname);
        ss.str("");
      }
  }

  for (int i=0; i<zones_to_write; ++i) {
    env_->Defragment(0);
  }

  ASSERT_EQ(env_->GetZonesUsed(), preused_zones+(zones_to_write/2));

  ASSERT_OK(env_->DeleteDir(dir));

}

TEST(SmrEnvTest, DefragmentZonesThirdErased) {
  int zones_to_write = 6;
  const size_t kWriteSize = 299 * 1024;
  std::string write_data;

  for (size_t i = 0; i < kWriteSize; ++i) {
    write_data.append(1, static_cast<char>(i));
  }

  uint64_t fsize = kWriteSize*10;
  int numFilesPerZone = (256*1024*1024)/fsize; //kWriteSize/4; //TRUNCATE(kWriteSize + 4096, 4096);

  ASSERT_OK(env_->CreateDir(dir, true));
  ASSERT_EQ(env_->GetZonesUsed(), preused_zones);
  stringstream ss;

  WritableFile* writable_file;
  string fileNumPrefix("000000");

  cout << "CREATING... " << endl;
  for (int i = 4; i < zones_to_write*numFilesPerZone; ++i) {
      ss << fileNumPrefix << i;
      string fname = ss.str();
      fname = fname.substr(fname.size() - 6);
      fname += ".sst";
      ASSERT_OK(env_->NewWritableFile( dir + "/" + fname, &writable_file));
      for (int j = 0; j < 10; ++j) {
          writable_file->Append(write_data);
      }
      delete writable_file;
      ss.str("");
  }

  ASSERT_EQ(env_->GetZonesUsed(), preused_zones+zones_to_write);
  cout << "Running Defragment... " << endl;
  env_->Defragment(0);
  ASSERT_EQ(env_->GetZonesUsed(), preused_zones+zones_to_write);

  cout << "DELETING... " << endl;

  // delete third of the files, alternating files
  for (int i = 4; i < zones_to_write*numFilesPerZone; ++i) {
      if (i % 3 == 0) {
        ss << fileNumPrefix << i;
        string fname = ss.str();
        fname = fname.substr(fname.size() - 6);
        fname += ".sst";
        env_->DeleteFile(dir + "/" + fname);
        ss.str("");
      }
  }

  for (int i=0; i<zones_to_write; ++i) {
    env_->Defragment(0);
  }

  ASSERT_EQ(env_->GetZonesUsed(), preused_zones+(zones_to_write-2));

  ASSERT_OK(env_->DeleteDir(dir));

}

/*
TEST(SmrEnvTest, Basics) {
  uint64_t file_size;
  WritableFile* writable_file;
  std::vector<std::string> children;

  ASSERT_OK(env_->CreateDir(dir));

  // Check that the directory is empty.
  ASSERT_TRUE(!env_->FileExists(dir + "/000001.sst"));
  ASSERT_TRUE(!env_->GetFileSize(dir + "/000001.sst", &file_size).ok());
  ASSERT_OK(env_->GetChildren(dir, &children));
  ASSERT_EQ(0 + local_file, children.size());

  // Create a file.
  ASSERT_OK(env_->NewWritableFile(dir + "/000002.sst", &writable_file));
  delete writable_file;

  // Check that the file exists.
  ASSERT_TRUE(env_->FileExists(dir + "/000002.sst"));
  ASSERT_OK(env_->GetFileSize(dir + "/000002.sst", &file_size));
  ASSERT_EQ(0, file_size);
  ASSERT_OK(env_->GetChildren(dir, &children));
  ASSERT_EQ(1 + local_file, children.size());
  //ASSERT_EQ("000002.sst", children[0]);

  // Write to the file.
  ASSERT_OK(env_->NewWritableFile(dir + "/000003.log", &writable_file));
  ASSERT_OK(writable_file->Append("abc"));
  ASSERT_OK(writable_file->Close());
  delete writable_file;

  // Check for expected size.
  ASSERT_OK(env_->GetFileSize(dir + "/000003.log", &file_size));
  ASSERT_EQ(3, file_size);

  // Check that renaming works.
  // Write to the file.
  ASSERT_OK(env_->NewWritableFile(dir + "/000005.dbtmp", &writable_file));
  ASSERT_OK(writable_file->Append("abc"));
  ASSERT_OK(writable_file->Close());
  delete writable_file;
//  ASSERT_TRUE(!env_->RenameFile(dir + "fg.tmp", dir + "CURRENT").ok());
  ASSERT_OK(env_->RenameFile(dir + "/000005.dbtmp", dir + "/CURRENT"));
  ASSERT_TRUE(!env_->FileExists(dir + "/000005.dbtmp"));
  ASSERT_TRUE(env_->FileExists(dir + "/CURRENT"));
  ASSERT_OK(env_->GetFileSize(dir + "/CURRENT", &file_size));
  ASSERT_EQ(3, file_size);

  // Check that opening non-existent file fails.
  SequentialFile* seq_file;
  RandomAccessFile* rand_file;
  ASSERT_TRUE(!env_->NewSequentialFile(dir + "/000007.sst", &seq_file).ok());
  ASSERT_TRUE(!seq_file);
  ASSERT_TRUE(!env_->NewRandomAccessFile(dir + "/000007.sst", &rand_file).ok());
  ASSERT_TRUE(!rand_file);

  // Check that deleting works.
  ASSERT_TRUE(!env_->DeleteFile(dir + "/000007.sst").ok());
  ASSERT_OK(env_->DeleteFile(dir + "/000002.sst"));
  ASSERT_TRUE(!env_->FileExists(dir + "/000002.sst"));
  ASSERT_OK(env_->DeleteFile(dir + "/000003.log"));
  ASSERT_TRUE(!env_->FileExists(dir + "/000003.log"));
  ASSERT_OK(env_->GetChildren(dir, &children));
  ASSERT_EQ(0 + local_file, children.size());
  ASSERT_OK(env_->DeleteDir(dir));
}

/*

TEST(SmrEnvTest, ReadWrite) {
  WritableFile* writable_file;
  SequentialFile* seq_file;
  RandomAccessFile* rand_file;
  Slice result;
  char scratch[100];

  ASSERT_OK(env_->CreateDir(dir));

  ASSERT_OK(env_->NewWritableFile(dir + "/000003.sst", &writable_file));
  ASSERT_OK(writable_file->Append("hello "));
  ASSERT_OK(writable_file->Append("world"));
  ASSERT_OK(writable_file->Close());
  delete writable_file;

  // Read sequentially.
  ASSERT_OK(env_->NewSequentialFile(dir +"/000003.sst", &seq_file));
  ASSERT_OK(seq_file->Read(5, &result, scratch)); // Read "hello".
  ASSERT_EQ(0, result.compare("hello"));
  ASSERT_OK(seq_file->Read(1000, &result, scratch)); // Read "world".
  ASSERT_EQ(0, result.compare(" world"));
  delete seq_file;

  ASSERT_OK(env_->NewSequentialFile(dir +"/000003.sst", &seq_file));
  ASSERT_OK(seq_file->Read(5, &result, scratch)); // Read "hello".
  ASSERT_EQ(0, result.compare("hello"));
  ASSERT_OK(seq_file->Skip(1));
  ASSERT_OK(seq_file->Read(1000, &result, scratch)); // Read "world".
  ASSERT_EQ(0, result.compare("world"));
  ASSERT_OK(seq_file->Read(1000, &result, scratch)); // Try reading past EOF.
  ASSERT_EQ(0, result.size());
  ASSERT_OK(seq_file->Skip(100)); // Try to skip past end of file.
  ASSERT_OK(seq_file->Read(1000, &result, scratch));
  ASSERT_EQ(0, result.size());
  delete seq_file;

  // Random reads.
  ASSERT_OK(env_->NewRandomAccessFile(dir + "/000003.sst", &rand_file));
  ASSERT_OK(rand_file->Read(6, 5, &result, scratch)); // Read "world".
  ASSERT_EQ(0, result.compare("world"));
  ASSERT_OK(rand_file->Read(0, 5, &result, scratch)); // Read "hello".
  ASSERT_EQ(0, result.compare("hello"));
  ASSERT_OK(rand_file->Read(10, 100, &result, scratch)); // Read "d".
  ASSERT_EQ(0, result.compare("d"));

  // Too high offset.
  ASSERT_TRUE(!rand_file->Read(1000, 5, &result, scratch).ok());
  delete rand_file;

  ASSERT_OK(env_->DeleteFile(dir + "/000003.sst"));
  ASSERT_OK(env_->DeleteDir(dir));
}
/*
TEST(SmrEnvTest, Locks) {
  FileLock* lock;

  // These are no-ops, but we test they return success.
  ASSERT_OK(env_->LockFile(dir + "LOCK", &lock));
  ASSERT_OK(env_->UnlockFile(lock));
}


TEST(SmrEnvTest, Misc) {
  std::string test_dir;
  ASSERT_OK(env_->GetTestDirectory(&test_dir));
  ASSERT_TRUE(!test_dir.empty());

  WritableFile* writable_file;
  ASSERT_OK(env_->NewWritableFile("/a/b", &writable_file));

  // These are no-ops, but we test they return success.
  ASSERT_OK(writable_file->Sync());
  ASSERT_OK(writable_file->Flush());
  ASSERT_OK(writable_file->Close());
  delete writable_file;
}

TEST(SmrEnvTest, ManifestTest) {
  const size_t kWriteSize = 300 * 1024;
  char* scratch = new char[kWriteSize * 2];

  ASSERT_OK(env_->CreateDir(dir));

  std::string write_data;
  for (size_t i = 0; i < kWriteSize; ++i) {
    write_data.append(1, static_cast<char>(i));
  }

  WritableFile* writable_file;
  ASSERT_OK(env_->NewWritableFile( dir + "/MANIFEST-000008", &writable_file));
  ASSERT_OK(writable_file->Append("foo"));
  ASSERT_OK(writable_file->Sync());
  ASSERT_OK(writable_file->Append("ofo"));
  ASSERT_OK(writable_file->Sync());
  ASSERT_OK(writable_file->Append("oof"));
  uint64_t zoneSize = 256*1024*1024;
  uint64_t biggerZoneSize = 2*zoneSize;
  int nLoops = (biggerZoneSize + zoneSize - 1)/kWriteSize;
  for (int i = 0; i < nLoops; ++i) {
      ASSERT_OK(writable_file->Append(write_data));
  }
  writable_file->Sync();
  delete writable_file;

  ASSERT_OK(env_->NewWritableFile( dir + "/MANIFEST-000009", &writable_file));
  ASSERT_OK(writable_file->Append("abc"));
  writable_file->Sync();
  delete writable_file;

  SequentialFile* seq_file;
  Slice result;
  ASSERT_OK(env_->NewSequentialFile(dir + "/MANIFEST-000008", &seq_file));
  ASSERT_OK(seq_file->Read(9, &result, scratch)); // Read "foo".
  ASSERT_EQ(0, result.compare("fooofooof"));

  size_t read = 0;
  std::string read_data;

  for (int i = 0; i < nLoops; ++i) {
      read = 0;
      read_data = "";
      while (read < kWriteSize) {
        ASSERT_OK(seq_file->Read(kWriteSize - read, &result, scratch));
        read_data.append(result.data(), result.size());
        read += result.size();
      }
      ASSERT_TRUE(write_data == read_data);
  }
  ASSERT_OK(env_->NewSequentialFile(dir + "/MANIFEST-000009", &seq_file));
  ASSERT_OK(seq_file->Read(3, &result, scratch)); // Read "foo".
  ASSERT_EQ(0, result.compare("abc"));

  delete seq_file;
  delete [] scratch;
  ASSERT_OK(env_->DeleteFile(dir + "/MANIFEST-000008"));
  ASSERT_TRUE(!env_->FileExists(dir + "/MANIFEST-000008"));
  ASSERT_OK(env_->DeleteFile(dir + "/MANIFEST-000009"));
  ASSERT_TRUE(!env_->FileExists(dir + "/MANIFEST-000009"));
  ASSERT_OK(env_->DeleteDir(dir));
}

TEST(SmrEnvTest, LargeWrite) {
  const size_t kWriteSize = 300 * 1024;
  char* scratch = new char[kWriteSize * 2];

  ASSERT_OK(env_->CreateDir(dir));

  std::string write_data;
  for (size_t i = 0; i < kWriteSize; ++i) {
    write_data.append(1, static_cast<char>(i));
  }

  WritableFile* writable_file;
  ASSERT_OK(env_->NewWritableFile( dir + "/000008.sst", &writable_file));
  ASSERT_OK(writable_file->Append("foo"));
  ASSERT_OK(writable_file->Sync());
  ASSERT_OK(writable_file->Append("ofo"));
  ASSERT_OK(writable_file->Sync());
  ASSERT_OK(writable_file->Append("oof"));
  ASSERT_OK(writable_file->Sync());


  ASSERT_OK(writable_file->Append(write_data));
  delete writable_file;

  SequentialFile* seq_file;
  Slice result;
  ASSERT_OK(env_->NewSequentialFile(dir + "/000008.sst", &seq_file));
  ASSERT_OK(seq_file->Read(9, &result, scratch)); // Read "foo".
  ASSERT_EQ(0, result.compare("fooofooof"));

  size_t read = 0;
  std::string read_data;
  while (read < kWriteSize) {
    ASSERT_OK(seq_file->Read(kWriteSize - read, &result, scratch));
    read_data.append(result.data(), result.size());
    read += result.size();
  }

  ASSERT_TRUE(write_data.size() == read_data.size());

  ASSERT_TRUE(write_data == read_data);

  delete seq_file;
  delete [] scratch;
  ASSERT_OK(env_->DeleteFile(dir + "/000008.sst"));
  ASSERT_TRUE(!env_->FileExists(dir + "/000008.sst"));
  ASSERT_OK(env_->DeleteDir(dir));
}
*/
/*
TEST(SmrEnvTest, DBTest) {
  Options options;
  options.create_if_missing = true;
  options.env = env_;
  DB* db;

  const Slice keys[] = {Slice("aaa"), Slice("bbb"), Slice("ccc")};
  const Slice vals[] = {Slice("foo"), Slice("bar"), Slice("baz")};

  ASSERT_OK(DB::Open(options, db_dir, &db));
  for (size_t i = 0; i < 3; ++i) {
    ASSERT_OK(db->Put(WriteOptions(), keys[i], vals[i]));
  }

  for (size_t i = 0; i < 3; ++i) {
    std::string res;
    ASSERT_OK(db->Get(ReadOptions(), keys[i], &res, false));
    ASSERT_TRUE(res == vals[i]);
  }

  Iterator* iterator = db->NewIterator(ReadOptions());
  iterator->SeekToFirst();
  for (size_t i = 0; i < 3; ++i) {
    ASSERT_TRUE(iterator->Valid());
    ASSERT_TRUE(keys[i] == iterator->key());
    ASSERT_TRUE(vals[i] == iterator->value());
    iterator->Next();
  }
  ASSERT_TRUE(!iterator->Valid());
  delete iterator;

  DBImpl* dbi = reinterpret_cast<DBImpl*>(db);
  ASSERT_OK(dbi->TEST_CompactMemTable());

  for (size_t i = 0; i < 3; ++i) {
    std::string res;
    ASSERT_OK(db->Get(ReadOptions(), keys[i], &res, false));
    ASSERT_TRUE(res == vals[i]);
  }

  delete db;
}
*/
}  // namespace leveldb

int main(int argc, char** argv) {
  return leveldb::test::RunAllTests();
}
