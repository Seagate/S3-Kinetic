#include "gtest/gtest.h"

#include "leveldb/env.h"
#include "port/port.h"
#include "smrdisk/DriveEnv.h"
#include "smrdb_test_helpers.h"

using namespace leveldb;  //NOLINT

namespace com {
namespace seagate {
namespace kinetic {

static const int kDelayMicros = 100000;

class SmrdbEnvPosixTest : public ::testing::Test {
 private:
    port::Mutex mu_;
    std::string events_;

 public:
    Env* env_;
    SmrdbEnvPosixTest() : env_(DriveEnv::getInstance()) { }
};

static void SetBool(void* ptr) {
    reinterpret_cast<port::AtomicPointer*>(ptr)->NoBarrier_Store(ptr);
}

TEST_F(SmrdbEnvPosixTest, RunImmediately) {
    port::AtomicPointer called(NULL);
    env_->Schedule(&SetBool, &called, NULL);
    DriveEnv::getInstance()->SleepForMicroseconds(kDelayMicros);
    EXPECT_TRUE(called.NoBarrier_Load() != NULL);
}

TEST_F(SmrdbEnvPosixTest, RunMany) {
    port::AtomicPointer last_id(NULL);

    struct CB {
        port::AtomicPointer* last_id_ptr;   // Pointer to shared slot
        uintptr_t id;             // Order# for the execution of this callback

        CB(port::AtomicPointer* p, int i) : last_id_ptr(p), id(i) { }

        static void Run(void* v) {
            CB* cb = reinterpret_cast<CB*>(v);
            void* cur = cb->last_id_ptr->NoBarrier_Load();
            EXPECT_EQ(cb->id-1, reinterpret_cast<uintptr_t>(cur));
            cb->last_id_ptr->Release_Store(reinterpret_cast<void*>(cb->id));
        }
    };

    // Schedule in different order than start time
    CB cb1(&last_id, 1);
    CB cb2(&last_id, 2);
    CB cb3(&last_id, 3);
    CB cb4(&last_id, 4);
    env_->Schedule(&CB::Run, &cb1, NULL);
    env_->Schedule(&CB::Run, &cb2, NULL);
    env_->Schedule(&CB::Run, &cb3, NULL);
    env_->Schedule(&CB::Run, &cb4, NULL);

    DriveEnv::getInstance()->SleepForMicroseconds(kDelayMicros);
    void* cur = last_id.Acquire_Load();
    EXPECT_EQ((unsigned int) 4, reinterpret_cast<uintptr_t>(cur));
}

struct State {
    port::Mutex mu;
    int val;
    int num_running;
};

static void ThreadBody(void* arg) {
    State* s = reinterpret_cast<State*>(arg);
    s->mu.Lock();
    s->val += 1;
    s->num_running -= 1;
    s->mu.Unlock();
}

TEST_F(SmrdbEnvPosixTest, StartThread) {
    State state;
    state.val = 0;
    state.num_running = 3;
    for (int i = 0; i < 3; i++) {
        env_->StartThread(&ThreadBody, &state);
    }
    while (true) {
        state.mu.Lock();
        int num = state.num_running;
        state.mu.Unlock();
        if (num == 0) {
            break;
        }
        DriveEnv::getInstance()->SleepForMicroseconds(kDelayMicros);
    }
    EXPECT_EQ(state.val, 3);
}

} // namespace kinetic
} // namespace seagate
} // namespace com
