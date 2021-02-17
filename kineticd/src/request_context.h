#ifndef KINETIC_REQUEST_CONTEXT_H_
#define KINETIC_REQUEST_CONTEXT_H_

namespace com {
namespace seagate {
namespace kinetic {

class RequestContext {
public:
    RequestContext() {
        userId_ = 1;
	    is_ssl = false;
	    writeThrough_ = false;
	    ignoreVersion_ = true;
        seq_ = 1;
        connId_ = 1;
    }

    bool ssl() { return is_ssl; }
    void setSsl(bool ssl) { is_ssl = ssl; }
    void setUserId(int64_t id) { userId_ = id; }
    int64_t userId() { return userId_; }
    bool writeThrough() { return writeThrough_; }
    void setWriteThrough(bool through) { writeThrough_ = through; }

    bool ignoreVersion() { return ignoreVersion_; }
    void setIgnoreVersion(bool ignore) { ignoreVersion_ = ignore; }
    void setSeq(int64_t seq) { seq_ = seq; }
    int64_t seq() { return seq_; }
    void setConnId(int64_t connId) { connId_ = connId; }
    int64_t connId() { return connId_; }

    bool is_ssl;
private:
    int64_t userId_;
    bool writeThrough_;
    bool ignoreVersion_;
    int64_t seq_;
    int64_t connId_;
};

} // namespace kinetic
} // namespace seagate
} // namespace com

#endif  // KINETIC_REQUEST_CONTEXT_H_
