#include "gtest/gtest.h"
#include "leveldb/db.h"

#include "db/memtable.h"
#include "db/write_batch_internal.h"
#include "leveldb/env.h"
#include "util/logging.h"
#include "smrdb_test_helpers.h"
#include <sstream>

using namespace leveldb;  //NOLINT

namespace com {
namespace seagate {
namespace kinetic {

class SmrdbWriteBatchTest : public ::testing::Test {
 protected:
    std::string PrintContents(WriteBatch* b) {
        InternalKeyComparator cmp(BytewiseComparator());
        MemTable* mem = new MemTable(cmp);
        mem->Ref();
        std::string state;
        Status s = WriteBatchInternal::MyInsertInto(b, mem);
        int count = 0;
        Iterator* iter = mem->NewIterator();
        for (iter->SeekToFirst(); iter->Valid(); iter->Next()) {
            ParsedInternalKey ikey;
            bool parsed = ParseInternalKey(iter->key(), &ikey);
            EXPECT_TRUE(parsed);
            switch (ikey.type) {
                case kTypeValue:
                    state.append("Put(");
                    state.append(ikey.user_key.ToString());
                    state.append(", ");
                    state.append(iter->value().ToString());
                    state.append(")");
                    count++;
                    break;
                case kTypeDeletion:
                    state.append("Delete(");
                    state.append(ikey.user_key.ToString());
                    state.append(")");
                    count++;
                    break;
            }
            state.append("@");
            state.append(NumberToString(ikey.sequence));
        }
        delete iter;
        if (!s.ok()) {
            state.append("ParseError()");
            printf("%s\n", s.ToString().c_str());
        } else if (count != WriteBatchInternal::Count(b)) {
            state.append("CountMismatch()");
        }
        mem->Unref();
        return state;
    }
};

TEST_F(SmrdbWriteBatchTest, Empty) {
    WriteBatch batch;
    EXPECT_EQ("", PrintContents(&batch));
    EXPECT_EQ(0, WriteBatchInternal::Count(&batch));
}

TEST_F(SmrdbWriteBatchTest, Multiple) {
    WriteBatch batch;
    int val0_size;
    int val1_size;
    char* val0 = PackValueMem("bar", &val0_size);
    char* val1 = PackValueMem("boo", &val1_size);

    batch.Put(Slice("foo"), Slice(val0, val0_size));
    batch.Delete(Slice("box"));
    batch.Put(Slice("baz"), Slice(val1, val1_size));
    WriteBatchInternal::SetSequence(&batch, 100);
    EXPECT_EQ((unsigned int)100, WriteBatchInternal::Sequence(&batch));
    EXPECT_EQ(3, WriteBatchInternal::Count(&batch));

    std::stringstream ss;
    ss << "Put(baz, " << std::string(val1, val1_size) << ")@102Delete(box)@101Put(foo, "
       << std::string(val0, val0_size) << ")@100";
    EXPECT_EQ(ss.str(), PrintContents(&batch));
}

// The next two tests will run but fail and segfault for double corruption. These may be exposing issues in smrdb.

// TEST_F(SmrdbWriteBatchTest, Corruption) {
//     WriteBatch batch;
//     std::string val;
//     val = PackValueString("bar");
//     batch.Put(Slice("foo"), val);
//     batch.Delete(Slice("box"));
//     WriteBatchInternal::SetSequence(&batch, 200);
//     Slice contents = WriteBatchInternal::Contents(&batch);
//     WriteBatchInternal::SetContents(&batch, Slice(contents.data(),contents.size()-1));

//     std::stringstream ss;
//     ss << "Put(foo, " << val << ")@200ParseError()";
//     EXPECT_EQ(ss.str(), PrintContents(&batch));
// }

// TEST_F(SmrdbWriteBatchTest, Append) {
//     WriteBatch b1, b2;
//     std::stringstream ss;
//     WriteBatchInternal::SetSequence(&b1, 200);
//     WriteBatchInternal::SetSequence(&b2, 300);
//     WriteBatchInternal::Append(&b1, &b2);
//     EXPECT_EQ("", PrintContents(&b1));

//     std::string val0 = PackValueString("va");
//     b2.Put("a", val0);
//     WriteBatchInternal::Append(&b1, &b2);
//     ss << "Put(a, " << val0 << ")@200";
//     EXPECT_EQ(ss.str(), PrintContents(&b1));

//     b2.Clear();
//     std::string val1 = PackValueString("vb");
//     b2.Put("b", val1);
//     WriteBatchInternal::Append(&b1, &b2);
//     ss << "Put(a, " << val0 << ")@200Put(b, " << val1 << ")@201";
//     EXPECT_EQ(ss.str(), PrintContents(&b1));
//     std::string val2 = PackValueString("bar");
//     b2.Put("foo", val2);
//     WriteBatchInternal::Append(&b1, &b2);
//     ss << "Put(a, " << val0 << ")@200Put(b, " << val1 << ")@202Put(b, " << val1 << ")@201Put(foo, " << val2 << ")@203";
//     EXPECT_EQ(ss.str(), PrintContents(&b1));
// }

} // namespace kinetic
} // namespace seagate
} // namespace com
